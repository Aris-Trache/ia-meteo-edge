/**
  * @file    eth_carte.h
  * @brief   Configuration materielle de l'Ethernet de la NUCLEO-N657X0-Q.
  */
#ifndef ETH_CARTE_H
#define ETH_CARTE_H

#include "stm32n6xx_hal.h"

/* Zone RAM non cacheable reservee au DMA Ethernet (voir le .ld) */
#define ETH_ZONE_NC_DEBUT   0x341F0000UL
#define ETH_ZONE_NC_FIN     0x341FFFFFUL

/** MPU : rend la zone ETH_ZONE_NC non cacheable. A appeler AVANT d'activer le D-cache. */
void eth_carte_mpu_config(void);

/** Securite RIF (le DMA Ethernet doit pouvoir acceder a la RAM securisee) */
void eth_carte_rif_config(void);

/** Initialise le controleur Ethernet (broches, horloge, descripteurs, DMA). */
void eth_carte_init(ETH_HandleTypeDef *heth, ETH_DMADescTypeDef *rx_desc, ETH_DMADescTypeDef *tx_desc);

#endif /* ETH_CARTE_H */
