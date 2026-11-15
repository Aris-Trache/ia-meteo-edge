/**
  * @file    ethernetif.h
  * @brief   Interface LwIP <-> Ethernet du STM32N657 (voir ethernetif.c).
  */
#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"
#include "stm32n6xx_hal.h"

extern ETH_HandleTypeDef heth1;

/** Fonction d'initialisation a passer a netif_add() */
err_t ethernetif_init(struct netif *netif);

/** Compteurs de trames recues / emises et de debordements du pool RX */
void ethernetif_stats(uint32_t *rx, uint32_t *tx, uint32_t *pool_vide);

#endif /* ETHERNETIF_H */
