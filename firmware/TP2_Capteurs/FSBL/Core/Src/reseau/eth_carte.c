/**
  ******************************************************************************
  * @file    eth_carte.c
  * @brief   Ethernet de la NUCLEO-N657X0-Q : broches, horloge, securite RIF, MPU
  *          et reglages DMA.
  *
  *  Valeurs reprises de l'exemple officiel ST "Nx_TCP_Echo_Client"
  *  (STM32CubeN6 1.4.1), qui tourne lui aussi dans le FSBL :
  *   - PHY LAN8742 en RMII : PF4 MDIO, PG11 MDC, PF5 CLK, PF7 REF_CLK,
  *     PF10 CRS_DV, PF11 TX_EN, PF12/PF13 TXD0/1, PF14/PF15 RXD0/1 (AF11)
  *   - horloge du MAC = HCLK (200 MHz)
  *   - interruption ETH1 en priorite 7 (compatible FreeRTOS, voir FreeRTOSConfig.h)
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#include "eth_carte.h"
#include "main.h"
#include <string.h>

/* Adresse MAC "administree localement" (bit 1 du 1er octet a 1) : pas de
   risque de collision avec un vrai constructeur sur le reseau local. */
static uint8_t adresse_mac[6] = { 0x02, 0x80, 0xE1, 0x00, 0x06, 0x57 };

/* ---------------------------------------------------------------------------
 * MPU : zone RAM non cacheable pour le DMA Ethernet
 * ------------------------------------------------------------------------- */
void eth_carte_mpu_config(void)
{
  MPU_Region_InitTypeDef r = {0};
  MPU_Attributes_InitTypeDef a = {0};
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  HAL_MPU_Disable();

  r.Enable           = MPU_REGION_ENABLE;
  r.Number           = MPU_REGION_NUMBER0;
  r.BaseAddress      = ETH_ZONE_NC_DEBUT;
  r.LimitAddress     = ETH_ZONE_NC_FIN;
  r.AttributesIndex  = MPU_ATTRIBUTES_NUMBER0;
  r.AccessPermission = MPU_REGION_ALL_RW;
  r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;   /* donnees seulement */
  r.DisablePrivExec  = MPU_PRIV_INSTRUCTION_ACCESS_DISABLE;
  r.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  HAL_MPU_ConfigRegion(&r);

  a.Number     = MPU_ATTRIBUTES_NUMBER0;
  a.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
  HAL_MPU_ConfigMemoryAttributes(&a);

  /* PRIVDEF : le reste de la memoire garde ses attributs par defaut */
  HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);
  __set_PRIMASK(primask);
}

/* ---------------------------------------------------------------------------
 * RIF : sur le STM32N6, chaque peripherique et chaque maitre DMA a des
 * attributs securise / privilegie. Le FSBL tourne en securise : le DMA
 * Ethernet doit l'etre aussi pour lire/ecrire nos tampons.
 * ------------------------------------------------------------------------- */
void eth_carte_rif_config(void)
{
  RIMC_MasterConfig_t maitre = {0};
  const uint16_t broches_f = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_10 |
                             GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;

  __HAL_RCC_RIFSC_CLK_ENABLE();

  maitre.MasterCID = RIF_CID_1;
  maitre.SecPriv   = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_ETH1, &maitre);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_ETH1, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  for (uint32_t i = 0; i < 16U; i++)
  {
    if (broches_f & (1U << i))
    {
      HAL_GPIO_ConfigPinAttributes(GPIOF, (uint16_t)(1U << i), GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    }
  }
  HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_11, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
}

/* ---------------------------------------------------------------------------
 * MSP : appelee par HAL_ETH_Init
 * ------------------------------------------------------------------------- */
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
  GPIO_InitTypeDef g = {0};
  RCC_PeriphCLKInitTypeDef clk = {0};

  if (heth->Instance != ETH1)
  {
    return;
  }

  clk.PeriphClockSelection = RCC_PERIPHCLK_ETH1;
  clk.Eth1ClockSelection   = RCC_ETH1CLKSOURCE_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_ETH1_CLK_ENABLE();
  __HAL_RCC_ETH1MAC_CLK_ENABLE();
  __HAL_RCC_ETH1TX_CLK_ENABLE();
  __HAL_RCC_ETH1RX_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  g.Pin = GPIO_PIN_4 | GPIO_PIN_10 | GPIO_PIN_7 | GPIO_PIN_5 | GPIO_PIN_15 |
          GPIO_PIN_14 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_12;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_NOPULL;
  g.Speed     = GPIO_SPEED_FREQ_LOW;
  g.Alternate = GPIO_AF11_ETH1;
  HAL_GPIO_Init(GPIOF, &g);

  g.Pin = GPIO_PIN_11;
  HAL_GPIO_Init(GPIOG, &g);

  HAL_NVIC_SetPriority(ETH1_IRQn, 7, 0);
  HAL_NVIC_EnableIRQ(ETH1_IRQn);
}

/* ---------------------------------------------------------------------------
 * Initialisation du MAC et du DMA
 * ------------------------------------------------------------------------- */
void eth_carte_init(ETH_HandleTypeDef *heth, ETH_DMADescTypeDef *rx_desc, ETH_DMADescTypeDef *tx_desc)
{
  ETH_DMAConfigTypeDef dma;

  heth->Instance            = ETH1;
  heth->Init.MACAddr        = adresse_mac;
  heth->Init.MediaInterface = HAL_ETH_RMII_MODE;
  for (uint32_t ch = 0; ch < ETH_DMA_CH_CNT; ch++)
  {
    heth->Init.TxDesc[ch] = &tx_desc[ch * ETH_TX_DESC_CNT];
    heth->Init.RxDesc[ch] = &rx_desc[ch * ETH_RX_DESC_CNT];
  }
  heth->Init.RxBuffLen = 1536;

  if (HAL_ETH_Init(heth) != HAL_OK)
  {
    Error_Handler();
  }

  /* Reglages DMA identiques au pilote ST de cette carte (multi-canaux) */
  memset(&dma, 0, sizeof(dma));
  HAL_ETH_GetDMAConfig(heth, &dma);
  dma.AddressAlignedBeats          = ENABLE;
  dma.AXIBLENMaxSize               = ETH_BLEN_MAX_SIZE_4;
  dma.BurstMode                    = ETH_BURSTLENGTH_FIXED;
  dma.RxOSRLimit                   = ETH_RX_OSR_LIMIT_3;
  dma.TxOSRLimit                   = ETH_TX_OSR_LIMIT_3;
  dma.TransmitArbitrationAlgorithm = ETH_DMATXARBITRATION_FIXED_PRIO;
  dma.TransmitPriority             = DISABLE;
  for (uint32_t ch = 0; ch < ETH_DMA_CH_CNT; ch++)
  {
    dma.DMACh[ch].FlushRxPacket        = DISABLE;
    dma.DMACh[ch].PBLx8Mode            = DISABLE;
    dma.DMACh[ch].RxDMABurstLength     = ETH_RXDMABURSTLENGTH_32BEAT;
    dma.DMACh[ch].SecondPacketOperate  = DISABLE;
    dma.DMACh[ch].TCPSegmentation      = DISABLE;
    dma.DMACh[ch].TxDMABurstLength     = ETH_TXDMABURSTLENGTH_32BEAT;
    dma.DMACh[ch].DescriptorSkipLength = ETH_DMA_DESC_SKIP_LENGTH_32;
    dma.DMACh[ch].MaximumSegmentSize   = 0x218U;
  }
  HAL_ETH_SetDMAConfig(heth, &dma);
}
