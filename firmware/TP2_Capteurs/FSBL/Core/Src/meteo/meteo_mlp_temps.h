/* IA meteo MAINTENANT, avec l'heure et la saison (voir meteo_mlp_temps.c) */
#ifndef METEO_MLP_TEMPS_H
#define METEO_MLP_TEMPS_H
/* x_norm : les 14 features NORMALISEES ; temps : hsin, hcos, dsin, dcos BRUTS (voir meteo_temps.h).
   Sortie : probabilites des 3 classes ; retour : indice de la classe. */
int meteo_temps_predire(const float x_norm[14], const float temps[4], float proba[3]);
#endif
