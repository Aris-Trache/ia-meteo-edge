/**
  ******************************************************************************
  * @file    ethernetif.c
  * @brief   Pilote d'interface LwIP <-> Ethernet du STM32N657 (PHY LAN8742, RMII).
  *
  *  Role : faire passer les trames entre la pile LwIP et le controleur Ethernet.
  *
  *    reception : DMA Ethernet -> tampon du pool RX (zone non cacheable)
  *                -> interruption -> semaphore -> tache "eth_rx" -> netif->input
  *                (zero copie : LwIP travaille directement dans le tampon DMA)
  *    emission  : LwIP -> low_level_output -> descripteurs DMA -> cable
  *    lien      : tache "eth_lien" qui interroge le PHY toutes les 100 ms
  *                (cable branche / debranche, vitesse, duplex)
  *
  *  Configuration materielle reprise de l'exemple officiel ST
  *  Nx_TCP_Echo_Client (STM32CubeN6 1.4.1, NUCLEO-N657X0-Q) : broches, horloge,
  *  descripteurs DMA, reglages DMA multi-canaux, PHY LAN8742.
  *  Structure inspiree du gabarit ethernetif.c genere par STM32CubeMX pour LwIP.
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#include "ethernetif.h"
#include "eth_carte.h"
#include "lan8742.h"
#include "app_reseau.h"   /* console partagee entre taches */

#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include "lwip/netifapi.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Parametres
 * ------------------------------------------------------------------------- */
#define IFNAME0                    's'
#define IFNAME1                    't'
#define ETH_RX_BUFFER_SIZE         1536U         /* une trame Ethernet complete */
#define ETH_RX_BUFFER_CNT          12U           /* tampons de reception */
#define ETH_DMA_TRANSMIT_TIMEOUT   20U           /* ms */
#define TACHE_RX_PILE              (1024U)       /* mots de 32 bits */
#define TACHE_LIEN_PILE            (512U)
#define TACHE_RX_PRIO              (configMAX_PRIORITIES - 1)
#define TACHE_LIEN_PRIO            (2)

/* Tout ce que le DMA Ethernet lit ou ecrit va en zone NON CACHEABLE */
#define NON_CACHEABLE              __attribute__((section(".eth_noncache"), aligned(32)))

/* ---------------------------------------------------------------------------
 * Memoire DMA
 * ------------------------------------------------------------------------- */
ETH_DMADescTypeDef DMARxDscrTab[ETH_DMA_RX_CH_CNT][ETH_RX_DESC_CNT] NON_CACHEABLE;
ETH_DMADescTypeDef DMATxDscrTab[ETH_DMA_TX_CH_CNT][ETH_TX_DESC_CNT] NON_CACHEABLE;

/* Tas de LwIP (MEM_SIZE) : sert aux trames emises (LWIP_NETIF_TX_SINGLE_PBUF) */
unsigned char lwip_ram_heap[MEM_SIZE + 256U] NON_CACHEABLE;

/* Pool de reception "zero copie" : une pbuf_custom + son tampon de trame */
typedef struct
{
  struct pbuf_custom pbuf_custom;
  uint8_t buff[(ETH_RX_BUFFER_SIZE + 31U) & ~31U] __attribute__((aligned(32)));
} RxBuff_t;

/* Place le tableau du pool, cree par LWIP_MEMPOOL_DECLARE, en zone non cacheable */
extern u8_t memp_memory_RX_POOL_base[] __attribute__((section(".eth_noncache")));
LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Pool RX Ethernet")

/* ---------------------------------------------------------------------------
 * Etat
 * ------------------------------------------------------------------------- */
ETH_HandleTypeDef heth1;
static ETH_TxPacketConfigTypeDef TxConfig;
static lan8742_Object_t LAN8742;
static SemaphoreHandle_t RxPktSemaphore;
static SemaphoreHandle_t TxPktSemaphore;
static volatile uint8_t RxAllocStatus;     /* 1 = pool RX epuise */
static volatile uint32_t nb_trames_rx, nb_trames_tx, nb_pool_vide;

#define RX_ALLOC_OK     0U
#define RX_ALLOC_ERROR  1U

static void pbuf_free_custom(struct pbuf *p);
static void tache_eth_rx(void *arg);
static void tache_eth_lien(void *arg);

/* ---------------------------------------------------------------------------
 * PHY LAN8742 : acces aux registres par le bus MDIO du MAC
 * ------------------------------------------------------------------------- */
static int32_t phy_io_init(void)   { HAL_ETH_SetMDIOClockRange(&heth1); return 0; }
static int32_t phy_io_deinit(void) { return 0; }
static int32_t phy_io_ecrire(uint32_t dev, uint32_t reg, uint32_t val)
{
  return (HAL_ETH_WritePHYRegister(&heth1, dev, reg, val) == HAL_OK) ? 0 : -1;
}
static int32_t phy_io_lire(uint32_t dev, uint32_t reg, uint32_t *val)
{
  return (HAL_ETH_ReadPHYRegister(&heth1, dev, reg, val) == HAL_OK) ? 0 : -1;
}
static int32_t phy_io_tick(void) { return (int32_t)HAL_GetTick(); }

static lan8742_IOCtx_t LAN8742_IOCtx = { phy_io_init, phy_io_deinit, phy_io_ecrire,
                                         phy_io_lire, phy_io_tick };

/* ---------------------------------------------------------------------------
 * Initialisation bas niveau
 * ------------------------------------------------------------------------- */
static void low_level_init(struct netif *netif)
{
  /* Controleur Ethernet (broches, horloges, descripteurs) : eth_carte.c */
  eth_carte_init(&heth1, &DMARxDscrTab[0][0], &DMATxDscrTab[0][0]);

  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, heth1.Init.MACAddr, ETH_HWADDR_LEN);
  netif->mtu   = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  LWIP_MEMPOOL_INIT(RX_POOL);

  /* Trames emises : CRC et bourrage ajoutes par le MAC, sommes de controle IP/TCP
     calculees par LwIP (voir lwipopts.h) */
  memset(&TxConfig, 0, sizeof(TxConfig));
  TxConfig.TxDMACh     = ETH_DMA_CH0_IDX;
  TxConfig.Attributes  = ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.CRCPadCtrl  = ETH_CRC_PAD_INSERT;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_DISABLE;

  RxPktSemaphore = xSemaphoreCreateBinary();
  TxPktSemaphore = xSemaphoreCreateBinary();
  configASSERT(RxPktSemaphore != NULL && TxPktSemaphore != NULL);

  xTaskCreate(tache_eth_rx, "eth_rx", TACHE_RX_PILE, netif, TACHE_RX_PRIO, NULL);

  /* PHY */
  LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);
  if (LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK)
  {
    printf("  [eth] PHY LAN8742 introuvable sur le bus MDIO\r\n");
    netif_set_link_down(netif);
    netif_set_down(netif);
  }
  else
  {
    printf("  [eth] PHY LAN8742 detecte (adresse MDIO %lu)\r\n", (unsigned long)LAN8742.DevAddr);
  }

  /* La tache "lien" demarre le MAC quand le cable est branche */
  xTaskCreate(tache_eth_lien, "eth_lien", TACHE_LIEN_PILE, netif, TACHE_LIEN_PRIO, NULL);
}

/* ---------------------------------------------------------------------------
 * Emission
 * ------------------------------------------------------------------------- */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];
  uint32_t i = 0U;
  err_t err = ERR_OK;
  (void)netif;

  memset(Txbuffer, 0, sizeof(Txbuffer));
  for (struct pbuf *q = p; q != NULL; q = q->next)
  {
    if (i >= ETH_TX_DESC_CNT)
    {
      return ERR_IF;
    }
    Txbuffer[i].buffer = q->payload;
    Txbuffer[i].len    = q->len;
    if (i > 0U)
    {
      Txbuffer[i - 1U].next = &Txbuffer[i];
    }
    /* Securite : si une donnee est en zone cacheable, on la pousse en RAM
       avant que le DMA ne la lise (sans effet en zone non cacheable). */
    SCB_CleanDCache_by_Addr((uint32_t *)((uint32_t)q->payload & ~31U),
                            (int32_t)(q->len + 32U));
    i++;
  }

  TxConfig.Length   = p->tot_len;
  TxConfig.TxBuffer = Txbuffer;
  TxConfig.pData    = p;

  pbuf_ref(p);                     /* libere par HAL_ETH_TxFreeCallback */

  heth1.TxOpCH = ETH_DMA_CH0_IDX;
  if (HAL_ETH_Transmit_IT(&heth1, &TxConfig) == HAL_OK)
  {
    nb_trames_tx++;
    xSemaphoreTake(TxPktSemaphore, pdMS_TO_TICKS(ETH_DMA_TRANSMIT_TIMEOUT));
    HAL_ETH_ReleaseTxPacket(&heth1);
  }
  else
  {
    pbuf_free(p);
    err = ERR_IF;
  }
  return err;
}

/* ---------------------------------------------------------------------------
 * Reception
 * ------------------------------------------------------------------------- */
static struct pbuf *low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL;
  (void)netif;

  if (RxAllocStatus == RX_ALLOC_OK)
  {
    heth1.RxOpCH = ETH_DMA_CH0_IDX;
    (void)HAL_ETH_ReadData(&heth1, (void **)&p);
  }
  return p;
}

static void tache_eth_rx(void *arg)
{
  struct netif *netif = (struct netif *)arg;
  struct pbuf *p;

  for (;;)
  {
    if (xSemaphoreTake(RxPktSemaphore, portMAX_DELAY) == pdTRUE)
    {
      do
      {
        p = low_level_input(netif);
        if (p != NULL)
        {
          nb_trames_rx++;
          if (netif->input(p, netif) != ERR_OK)
          {
            pbuf_free(p);
          }
        }
      } while (p != NULL);
    }
  }
}

/* Appelees par le HAL pendant HAL_ETH_ReadData */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
  if (p != NULL)
  {
    /* Le tampon commence apres la structure pbuf_custom */
    *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
    p->custom_free_function = pbuf_free_custom;
    pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
  }
  else
  {
    RxAllocStatus = RX_ALLOC_ERROR;
    nb_pool_vide++;
    *buff = NULL;
  }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd   = (struct pbuf **)pEnd;
  struct pbuf *p;

  /* Retrouve la pbuf a partir de l'adresse du tampon */
  p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
  p->next = NULL;
  p->tot_len = 0;
  p->len = Length;

  if (*ppStart == NULL)
  {
    *ppStart = p;
  }
  else
  {
    (*ppEnd)->next = p;
  }
  *ppEnd = p;

  /* Met a jour la longueur totale de toute la chaine */
  for (p = *ppStart; p != NULL; p = p->next)
  {
    p->tot_len += Length;
  }
}

void HAL_ETH_TxFreeCallback(uint32_t *buff)
{
  pbuf_free((struct pbuf *)buff);
}

static void pbuf_free_custom(struct pbuf *p)
{
  struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;
  LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

  /* Un tampon vient d'etre rendu : la reception peut reprendre */
  if (RxAllocStatus == RX_ALLOC_ERROR)
  {
    RxAllocStatus = RX_ALLOC_OK;
    xSemaphoreGive(RxPktSemaphore);
  }
}

/* Interruptions Ethernet (contexte ISR) */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
  BaseType_t reveil = pdFALSE;
  (void)heth;
  xSemaphoreGiveFromISR(RxPktSemaphore, &reveil);
  portYIELD_FROM_ISR(reveil);
}

void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
  BaseType_t reveil = pdFALSE;
  (void)heth;
  xSemaphoreGiveFromISR(TxPktSemaphore, &reveil);
  portYIELD_FROM_ISR(reveil);
}

void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
  BaseType_t reveil = pdFALSE;
  if ((HAL_ETH_GetDMAError(heth) & ETH_DMACxSR_RBU) == ETH_DMACxSR_RBU)
  {
    /* Plus de descripteur de reception libre : on relance la lecture */
    xSemaphoreGiveFromISR(RxPktSemaphore, &reveil);
  }
  portYIELD_FROM_ISR(reveil);
}

/* ---------------------------------------------------------------------------
 * Surveillance du lien (cable)
 * ------------------------------------------------------------------------- */
static void tache_eth_lien(void *arg)
{
  struct netif *netif = (struct netif *)arg;
  ETH_MACConfigTypeDef MACConf;
  int32_t etat;
  uint32_t vitesse = 0U, duplex = 0U;

  for (;;)
  {
    etat = LAN8742_GetLinkState(&LAN8742);

    if (netif_is_link_up(netif) && (etat <= LAN8742_STATUS_LINK_DOWN))
    {
      HAL_ETH_Stop_IT(&heth1);
      netifapi_netif_set_link_down(netif);
      console_verrouiller();
      printf("  [eth] cable debranche\r\n");
      console_liberer();
    }
    else if (!netif_is_link_up(netif) && (etat > LAN8742_STATUS_LINK_DOWN))
    {
      int ok = 1;
      switch (etat)
      {
        case LAN8742_STATUS_100MBITS_FULLDUPLEX: duplex = ETH_FULLDUPLEX_MODE; vitesse = ETH_SPEED_100M; break;
        case LAN8742_STATUS_100MBITS_HALFDUPLEX: duplex = ETH_HALFDUPLEX_MODE; vitesse = ETH_SPEED_100M; break;
        case LAN8742_STATUS_10MBITS_FULLDUPLEX:  duplex = ETH_FULLDUPLEX_MODE; vitesse = ETH_SPEED_10M;  break;
        case LAN8742_STATUS_10MBITS_HALFDUPLEX:  duplex = ETH_HALFDUPLEX_MODE; vitesse = ETH_SPEED_10M;  break;
        default: ok = 0; break;
      }
      if (ok)
      {
        /* Le MAC doit utiliser la vitesse et le duplex negocies par le PHY */
        HAL_ETH_GetMACConfig(&heth1, &MACConf);
        MACConf.DuplexMode = duplex;
        MACConf.Speed      = vitesse;
        HAL_ETH_SetMACConfig(&heth1, &MACConf);
        HAL_ETH_Start_IT(&heth1);
        netifapi_netif_set_link_up(netif);
        console_verrouiller();
        printf("  [eth] cable branche : %s Mbit/s, %s duplex\r\n",
               (vitesse == ETH_SPEED_100M) ? "100" : "10",
               (duplex == ETH_FULLDUPLEX_MODE) ? "full" : "half");
        console_liberer();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/* ---------------------------------------------------------------------------
 * API publique
 * ------------------------------------------------------------------------- */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));
#if LWIP_NETIF_HOSTNAME
  netif->hostname = "nucleo-meteo";
#endif
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->output     = etharp_output;
  netif->linkoutput = low_level_output;
  low_level_init(netif);
  return ERR_OK;
}

void ethernetif_stats(uint32_t *rx, uint32_t *tx, uint32_t *pool_vide)
{
  *rx = nb_trames_rx;
  *tx = nb_trames_tx;
  *pool_vide = nb_pool_vide;
}

/* Interruption Ethernet (vecteur ETH1_IRQn, priorite 7) */
void ETH1_IRQHandler(void)
{
  HAL_ETH_IRQHandler(&heth1);
}
