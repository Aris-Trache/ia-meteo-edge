/* IA de prevision a 3 h (voir meteo_prev3h.c) */
#ifndef METEO_PREV3H_H
#define METEO_PREV3H_H
/* Entree : les 14 features NORMALISEES (les memes que l'IA du moment).
   Sortie : probabilites des 3 classes DANS 3 H ; retour : indice de la classe. */
int meteo_prev3h_predire(const float x_norm[14], float proba[3]);
#endif
