/**
  ******************************************************************************
  * @file    app_reseau.h
  * @brief   TP2.3 : application reseau (FreeRTOS + LwIP) - interface avec main.c
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#ifndef APP_RESEAU_H
#define APP_RESEAU_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* Derniere mesure + prediction de l'IA embarquee, envoyee au Cloud */
typedef struct
{
  int   valide;
  float temp_c;
  float humidite_rh;
  float pression_mer_hpa;
  int   classe;            /* 0 Degage, 1 Nuageux-Couvert, 2 Precipitations */
  float proba_precip;      /* 0..1 */
  float proba[3];          /* probabilites des 3 classes (page web) */
} MeteoPublication_t;

/* Cree les taches (reseau + capteurs/IA) puis demarre l'ordonnanceur. Ne revient pas. */
void reseau_lancer(TaskFunction_t tache_capteurs, uint32_t pile_capteurs_mots);

/* Appelee par la tache capteurs/IA apres chaque prediction */
void reseau_publier_mesure(const MeteoPublication_t *m);

/* Temps d'inference mesures au demarrage [ns] (affiches sur la page web) */
void reseau_publier_bench(uint32_t ns_off, uint32_t ns_f32, uint32_t ns_s8, uint32_t ns_npu);

/* Acces exclusif a la console (printf) entre les taches */
void console_verrouiller(void);
void console_liberer(void);

#endif /* APP_RESEAU_H */
