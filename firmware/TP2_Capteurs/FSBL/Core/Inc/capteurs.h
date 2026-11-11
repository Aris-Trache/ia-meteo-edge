/**
  ******************************************************************************
  * @file    capteurs.h
  * @brief   Capteurs meteo sur I2C, detection automatique de la carte
  *          d'extension ST :
  *
  *            role            X-NUCLEO-IKS01A3     X-NUCLEO-IKS4A1
  *            -------------   ------------------   ------------------
  *            humidite + T    HTS221  (0x5F)       SHT40   (0x44)
  *            pression + T    LPS22HH (0x5D/0x5C)  LPS22DF (0x5D/0x5C)
  *            T de precision  STTS751 (0x4A...)    STTS22H (0x38...)
  *
  *          Le reste du programme ne voit que des ROLES (humidite, pression,
  *          temperature de precision), jamais le nom du composant : on peut
  *          changer de carte sans toucher a main.c ni au modele d'IA.
  *
  *          Couche "plateforme" (read_reg / write_reg) = HAL I2C du STM32N6,
  *          drivers registres = STMems_Standard_C_drivers (ST, licence BSD-3).
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#ifndef CAPTEURS_H
#define CAPTEURS_H

#include <stdint.h>
#include "stm32n6xx_hal.h"

/* Bits de presence d'un composant (resultat de capteurs_init) */
#define CAPTEUR_HTS221    (1U << 0)
#define CAPTEUR_LPS22HH   (1U << 1)
#define CAPTEUR_STTS751   (1U << 2)
#define CAPTEUR_SHT40     (1U << 3)
#define CAPTEUR_LPS22DF   (1U << 4)
#define CAPTEUR_STTS22H   (1U << 5)

/* Bits de ROLE (champ Mesures_t.valide) : c'est ce que main.c utilise */
#define MESURE_HUMIDITE   (1U << 0)   /* humidite_rh + temp_hr_c         */
#define MESURE_PRESSION   (1U << 1)   /* pression_hpa + temp_p_c         */
#define MESURE_TEMP_PREC  (1U << 2)   /* temp_precision_c                */

/* Une mesure complete. "valide" indique les roles lus sans erreur. */
typedef struct
{
  float    humidite_rh;        /* [%RH]  HTS221 ou SHT40                */
  float    temp_hr_c;          /* [degC] capteur d'humidite             */
  float    pression_hpa;       /* [hPa]  LPS22HH ou LPS22DF (absolue)   */
  float    temp_p_c;           /* [degC] capteur de pression            */
  float    temp_precision_c;   /* [degC] STTS751 ou STTS22H             */
  uint32_t valide;             /* bits MESURE_xxx                       */
} Mesures_t;

/**
  * @brief  Liste sur la console toutes les adresses qui repondent sur le bus.
  *         Premier test a faire quand "rien ne marche" (cablage, pull-ups...).
  */
void capteurs_scan_i2c(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Detecte et configure les capteurs (verification WHO_AM_I / CRC).
  * @retval Masque des composants trouves (bits CAPTEUR_xxx).
  */
uint32_t capteurs_init(I2C_HandleTypeDef *hi2c);

/** @brief Nom de la carte reconnue ("X-NUCLEO-IKS01A3", "X-NUCLEO-IKS4A1",
  *        "mixte" ou "aucune"), valable apres capteurs_init(). */
const char *capteurs_carte(void);

/** @brief Nom du composant qui tient un role (ex. "SHT40"), ou "-". */
const char *capteurs_nom_role(uint32_t role_mesure);

/**
  * @brief  Lit tous les capteurs presents et convertit en unites physiques.
  * @retval Masque des roles lus avec succes (identique a m->valide).
  */
uint32_t capteurs_lire(Mesures_t *m);

#endif /* CAPTEURS_H */
