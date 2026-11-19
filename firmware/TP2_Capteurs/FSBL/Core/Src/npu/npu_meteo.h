/**
  * @file    npu_meteo.h
  * @brief   Reseau meteo sur le NPU Neural-ART (STM32N6) - interface
  * @author  Aris Trache - ETRS923, USMB Master ESET
  */
#ifndef NPU_METEO_H
#define NPU_METEO_H

#define METEO_NPU_NB_ENTREES  14
#define METEO_NPU_NB_CLASSES  3

/* A appeler une fois (apres l'activation des caches). 0 = OK */
int npu_meteo_init(void);

/* Entree = les 14 features normalisees (comme le CPU). Retour = classe (0..2), -1 si erreur */
int npu_meteo_predire(const float entree[METEO_NPU_NB_ENTREES], float proba[METEO_NPU_NB_CLASSES]);

#endif
