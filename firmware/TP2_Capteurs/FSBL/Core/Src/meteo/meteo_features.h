/**
  ******************************************************************************
  * @file    meteo_features.h
  * @brief   Calcul embarque des 14 entrees du modele meteo (TP3).
  *          Portage C exact de notebooks/meteo_features.py : meme formules, meme ordre,
  *          memes conventions de demarrage a froid. Verifie par tests/test_meteo_features.c.
  *
  *  Principe : la carte garde un historique HORAIRE des 6 dernieres heures (moyennes
  *  des mesures de chaque heure). A chaque prediction, on combine la mesure courante
  *  et cet historique pour former les tendances (principe du barometre).
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#ifndef METEO_FEATURES_H
#define METEO_FEATURES_H

#include <stdint.h>
#include "meteo_model_params.h"     /* genere par le notebook : ordre, normalisation, classes */

/* Altitude du capteur [m] : sert a ramener la pression au niveau de la mer.
   Le Bourget-du-Lac (campus USMB) ~ 240 m. A adapter au lieu d'installation. */
#ifndef METEO_ALTITUDE_M
#define METEO_ALTITUDE_M   240.0f
#endif

/* Un echantillon horaire de l'historique */
typedef struct
{
  float temp;       /* degC */
  float rhum;       /* %RH  */
  float pres;       /* hPa, niveau de la mer */
} MeteoEchantillon_t;

/* Historique circulaire : h[0] = heure la plus recente ... h[n-1] = la plus ancienne */
typedef struct
{
  MeteoEchantillon_t h[METEO_HISTORY_HOURS];
  uint8_t            n;                      /* nombre d'heures disponibles (0..6) */
} MeteoHistorique_t;

/* Formules physiques (identiques au Python) */
float meteo_point_de_rosee(float temp_c, float rhum);
float meteo_pression_niveau_mer(float p_station_hpa, float temp_c, float altitude_m);

/* Historique */
void  meteo_historique_init(MeteoHistorique_t *hist);
void  meteo_historique_ajouter(MeteoHistorique_t *hist, float temp, float rhum, float pres_mer);

/**
  * @brief  Calcule les METEO_NB_FEATURES entrees BRUTES (non normalisees).
  * @param  hist  historique horaire (peut etre vide : demarrage a froid)
  * @param  temp, rhum, pres_mer  mesure courante (pression deja ramenee au niveau de la mer)
  * @param  out   tableau de METEO_NB_FEATURES floats, dans l'ordre METEO_F_xxx
  */
void  meteo_calculer_features(const MeteoHistorique_t *hist, float temp, float rhum,
                              float pres_mer, float out[METEO_NB_FEATURES]);

/* Normalisation (x - mean) / std, a appliquer juste avant le reseau */
void  meteo_normaliser(const float brut[METEO_NB_FEATURES], float norm[METEO_NB_FEATURES]);

#endif /* METEO_FEATURES_H */
