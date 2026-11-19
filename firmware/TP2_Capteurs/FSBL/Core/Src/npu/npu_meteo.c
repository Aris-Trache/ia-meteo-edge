/* ETRS923 : optimise meme en Debug (comparaison equitable avec le CPU, voir docs/TP3_NPU.md) */
#pragma GCC optimize ("O3")
/**
  ******************************************************************************
  * @file    npu_meteo.c
  * @brief   Execution du reseau meteo sur l'accelerateur NPU Neural-ART du STM32N6.
  *
  *  Le code du reseau (meteo_npu.c) est genere par ST Edge AI Core 2.2 a partir
  *  du meme modele int8 que la version CMSIS-NN (models/meteo_k3_mlp_int8.tflite) :
  *    - les 3 couches denses (+ ReLU) tournent sur le NPU (4 "epochs" materielles) ;
  *    - la quantification de l'entree, le softmax et la dequantification restent
  *      sur le CPU (npu_sw_ops.c).
  *
  *  Memoire : une seule zone, AXISRAM2 0x34100000..0x34180000 (512 Ko), libre
  *  car notre programme (FSBL) commence a 0x34180400. Le reseau y occupe 2,5 Ko :
  *  poids + echelles (copies au demarrage depuis meteo_npu_params[]) et activations.
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#include "npu_meteo.h"
#include "main.h"
#include "ll_aton_rt_user_api.h"
#include <string.h>

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(meteo_npu);

extern const uint32_t meteo_npu_params_adresse;
extern const uint32_t meteo_npu_params_taille;
extern const uint8_t  meteo_npu_params[];

static int npu_pret = 0;

/* Region RISAF "tout autorise" (identique au projet ST NPU_Validation) */
static void risaf_par_defaut(RISAF_TypeDef *risaf, uint32_t taille_espace)
{
  RISAF_BaseRegionConfig_t conf;
  conf.StartAddress   = 0x0;
  conf.EndAddress     = taille_espace;
  conf.Filtering      = RISAF_FILTER_ENABLE;
  conf.PrivWhitelist  = RIF_CID_NONE;
  conf.ReadWhitelist  = RIF_CID_MASK;
  conf.WriteWhitelist = RIF_CID_MASK;
  conf.Secure = RIF_ATTRIBUTE_SEC;
  HAL_RIF_RISAF_ConfigBaseRegion(risaf, 0, &conf);
  conf.Secure = RIF_ATTRIBUTE_NSEC;
  HAL_RIF_RISAF_ConfigBaseRegion(risaf, 1, &conf);
}

int npu_meteo_init(void)
{
  RIMC_MasterConfig_t maitre;

  /* 1. Horloge + reset du NPU (horloge NPU = IC6 = PLL1/4 = 300 MHz ici) */
  __HAL_RCC_NPU_CLK_ENABLE();
  __HAL_RCC_NPU_FORCE_RESET();
  __HAL_RCC_NPU_RELEASE_RESET();
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /* 2. Securite RIF : le NPU est un maitre du bus, il doit avoir les memes droits
        que notre code (CID 1, securise, privilegie) pour lire/ecrire la RAM. */
  maitre.MasterCID = RIF_CID_1;
  maitre.SecPriv   = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &maitre);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU, RIF_ATTRIBUTE_PRIV | RIF_ATTRIBUTE_SEC);
  risaf_par_defaut(RISAF3_S, RISAF3_LIMIT_ADDRESS_SPACE_SIZE);   /* AXISRAM2 */
  risaf_par_defaut(RISAF4_S, RISAF4_LIMIT_ADDRESS_SPACE_SIZE);   /* port maitre NPU 0 */
  risaf_par_defaut(RISAF5_S, RISAF5_LIMIT_ADDRESS_SPACE_SIZE);   /* port maitre NPU 1 */

  /* 3. Poids et echelles a leur adresse, puis ecriture en RAM (le NPU ne voit pas le cache CPU) */
  memcpy((void *)meteo_npu_params_adresse, meteo_npu_params, meteo_npu_params_taille);
  SCB_CleanDCache_by_Addr((void *)meteo_npu_params_adresse, (int32_t)((meteo_npu_params_taille + 31U) & ~31U));

  /* 4. Runtime ST (LL_ATON) */
  LL_ATON_RT_RuntimeInit();
  LL_ATON_RT_Init_Network(&NN_Instance_meteo_npu);
  npu_pret = 1;
  return 0;
}

int npu_meteo_predire(const float entree[METEO_NPU_NB_ENTREES], float proba[METEO_NPU_NB_CLASSES])
{
  LL_ATON_RT_RetValues_t r;
  const LL_Buffer_InfoTypeDef *bin  = LL_ATON_Input_Buffers_Info(&NN_Instance_meteo_npu);
  const LL_Buffer_InfoTypeDef *bout = LL_ATON_Output_Buffers_Info(&NN_Instance_meteo_npu);
  float *pin  = (float *)LL_Buffer_addr_start(&bin[0]);
  float *pout = (float *)LL_Buffer_addr_start(&bout[0]);
  int i, classe = 0;

  if (!npu_pret)
  {
    return -1;
  }

  memcpy(pin, entree, METEO_NPU_NB_ENTREES * sizeof(float));
  SCB_CleanDCache_by_Addr(pin, 64);

  LL_ATON_RT_Reset_Network(&NN_Instance_meteo_npu);
  do
  {
    /* Mode scrutation (LL_ATON_RT_POLLING) : pas d'interruption NPU, on relance
       simplement le runtime jusqu'a la fin (pas de WFE : il attendrait le tick 1 ms). */
    r = LL_ATON_RT_RunEpochBlock(&NN_Instance_meteo_npu);
  } while (r != LL_ATON_RT_DONE);

  SCB_InvalidateDCache_by_Addr(pout, 32);
  for (i = 0; i < METEO_NPU_NB_CLASSES; i++)
  {
    proba[i] = pout[i];
    if (pout[i] > pout[classe])
    {
      classe = i;
    }
  }
  return classe;
}
