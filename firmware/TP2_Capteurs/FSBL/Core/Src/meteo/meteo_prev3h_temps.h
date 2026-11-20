/* IA meteo DANS 3 H, avec l'heure et la saison (voir meteo_prev3h_temps.c) */
#ifndef METEO_PREV3H_TEMPS_H
#define METEO_PREV3H_TEMPS_H
/* x_norm : les 14 features NORMALISEES ; temps : hsin, hcos, dsin, dcos BRUTS (voir meteo_temps.h).
   Sortie : probabilites des 3 classes ; retour : indice de la classe. */
int meteo_prev3h_temps_predire(const float x_norm[14], const float temps[4], float proba[3]);
#endif
