/* Genere automatiquement par notebooks/TP3_Meteostat_IA.ipynb -- NE PAS EDITER A LA MAIN */
#ifndef METEO_MODEL_PARAMS_H
#define METEO_MODEL_PARAMS_H

#define METEO_NB_FEATURES   14
#define METEO_NB_CLASSES    3
#define METEO_HISTORY_HOURS 6

/* Ordre des entrees du modele */
#define METEO_F_TEMP      0
#define METEO_F_RHUM      1
#define METEO_F_PRES      2
#define METEO_F_SPREAD    3
#define METEO_F_DP3       4
#define METEO_F_DP1       5
#define METEO_F_DT3       6
#define METEO_F_DRH3      7
#define METEO_F_DP6       8
#define METEO_F_DT1       9
#define METEO_F_DRH1      10
#define METEO_F_RHMAX6    11
#define METEO_F_RHMIN6    12
#define METEO_F_SP_MEAN6  13

/* Normalisation : x_norm = (x - mean) / std  (statistiques du jeu d'entrainement 2017-2022) */
static const float METEO_MEAN[METEO_NB_FEATURES] = { 12.86761f, 72.5664f, 1017.785f, 5.568298f, 0.003072298f, 0.002579116f, -0.01523066f, 0.08090879f, -0.002201457f, -0.004690567f, 0.06489313f, 79.95618f, 64.6916f, 5.593514f };
static const float METEO_STD[METEO_NB_FEATURES]  = { 8.322365f, 19.26539f, 7.76324f, 4.986937f, 1.306838f, 0.5286183f, 2.954847f, 11.86015f, 2.2303f, 1.257904f, 5.890948f, 16.44112f, 19.99024f, 4.624297f };

static const char *const METEO_CLASS_NAMES[METEO_NB_CLASSES] = { "Degage", "Nuageux-Couvert", "Precipitations" };

#endif /* METEO_MODEL_PARAMS_H */
