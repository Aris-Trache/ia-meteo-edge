/**
  ******************************************************************************
  * @file    meteo_features.c
  * @brief   Calcul embarque des entrees du modele meteo. Voir meteo_features.h.
  *
  *  Correspondance avec Python (build_features, a l'heure t) :
  *    historique : h[0] = t-1, h[1] = t-2, ... h[5] = t-6   (heures PASSEES)
  *    dp1 = p - p(t-1)  -> h[0]      dp3 = p - p(t-3) -> h[2]      dp6 = p - p(t-6) -> h[5]
  *    fenetres 6 h (rhmax6, rhmin6, sp_mean6) = heure courante + h[0..4]
  *  Si une heure manque (demarrage) : tendance = 0, fenetres calculees sur ce qui existe.
  *  Historique vide = exactement la convention cold_start() du Python.
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#include "meteo_features.h"
#include <math.h>
#include <string.h>

float meteo_point_de_rosee(float temp_c, float rhum)
{
  const float a = 17.62f, b = 243.12f;
  if (rhum < 1.0f)   { rhum = 1.0f; }
  if (rhum > 100.0f) { rhum = 100.0f; }
  float g = logf(rhum / 100.0f) + a * temp_c / (b + temp_c);
  return b * g / (a - g);
}

float meteo_pression_niveau_mer(float p_station_hpa, float temp_c, float altitude_m)
{
  float r = 1.0f - 0.0065f * altitude_m / (temp_c + 0.0065f * altitude_m + 273.15f);
  return p_station_hpa * powf(r, -5.257f);
}

void meteo_historique_init(MeteoHistorique_t *hist)
{
  memset(hist, 0, sizeof(*hist));
}

void meteo_historique_ajouter(MeteoHistorique_t *hist, float temp, float rhum, float pres_mer)
{
  /* decalage : la plus ancienne heure sort, la nouvelle entre en h[0] */
  memmove(&hist->h[1], &hist->h[0], sizeof(hist->h[0]) * (METEO_HISTORY_HOURS - 1));
  hist->h[0].temp = temp;
  hist->h[0].rhum = rhum;
  hist->h[0].pres = pres_mer;
  if (hist->n < METEO_HISTORY_HOURS) { hist->n++; }
}

/* valeur de l'heure t-k (k >= 1) si disponible */
static int passe(const MeteoHistorique_t *hist, int k, MeteoEchantillon_t *e)
{
  if (k < 1 || k > hist->n) { return 0; }
  *e = hist->h[k - 1];
  return 1;
}

void meteo_calculer_features(const MeteoHistorique_t *hist, float temp, float rhum,
                             float pres_mer, float out[METEO_NB_FEATURES])
{
  MeteoEchantillon_t e;
  float spread = temp - meteo_point_de_rosee(temp, rhum);

  out[METEO_F_TEMP]   = temp;
  out[METEO_F_RHUM]   = rhum;
  out[METEO_F_PRES]   = pres_mer;
  out[METEO_F_SPREAD] = spread;

  out[METEO_F_DP3]  = passe(hist, 3, &e) ? pres_mer - e.pres : 0.0f;
  out[METEO_F_DP1]  = passe(hist, 1, &e) ? pres_mer - e.pres : 0.0f;
  out[METEO_F_DT3]  = passe(hist, 3, &e) ? temp - e.temp     : 0.0f;
  out[METEO_F_DRH3] = passe(hist, 3, &e) ? rhum - e.rhum     : 0.0f;
  out[METEO_F_DP6]  = passe(hist, 6, &e) ? pres_mer - e.pres : 0.0f;
  out[METEO_F_DT1]  = passe(hist, 1, &e) ? temp - e.temp     : 0.0f;
  out[METEO_F_DRH1] = passe(hist, 1, &e) ? rhum - e.rhum     : 0.0f;

  /* fenetre 6 h = heure courante + 5 heures passees (ce qui existe) */
  float rh_max = rhum, rh_min = rhum, sp_sum = spread;
  int   nb = 1;
  for (int k = 1; k <= 5; k++)
  {
    if (!passe(hist, k, &e)) { break; }
    if (e.rhum > rh_max) { rh_max = e.rhum; }
    if (e.rhum < rh_min) { rh_min = e.rhum; }
    sp_sum += e.temp - meteo_point_de_rosee(e.temp, e.rhum);
    nb++;
  }
  out[METEO_F_RHMAX6]   = rh_max;
  out[METEO_F_RHMIN6]   = rh_min;
  out[METEO_F_SP_MEAN6] = sp_sum / (float)nb;
}

void meteo_normaliser(const float brut[METEO_NB_FEATURES], float norm[METEO_NB_FEATURES])
{
  for (int i = 0; i < METEO_NB_FEATURES; i++)
  {
    norm[i] = (brut[i] - METEO_MEAN[i]) / METEO_STD[i];
  }
}
