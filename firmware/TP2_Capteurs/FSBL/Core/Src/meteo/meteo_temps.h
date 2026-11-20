/**
  * @file    meteo_temps.h
  * @brief   Entrees "heure et saison" de l'IA (branche saison).
  *          hsin/hcos = heure UTC (cycle de 24 h), dsin/dcos = jour de l'annee (cycle de 365,25 j),
  *          memes formules que notebooks/meteo_features.py.
  *          Source de l'heure : 1) les reponses ThingSpeak (en-tete "Date:"), des qu'Internet a repondu ;
  *          2) sinon la date/heure de COMPILATION + le temps ecoule depuis le demarrage (la carte est
  *          relancee juste apres la compilation en mode developpement : erreur de quelques minutes).
  */
#ifndef METEO_TEMPS_H
#define METEO_TEMPS_H
#include <stdint.h>

#define METEO_TEMPS_INTERNET    1
#define METEO_TEMPS_COMPILATION 2

/* Remplit t[4] = { hsin, hcos, dsin, dcos } ; renvoie la source de l'heure.
   Si heure_utc != NULL : heure UTC courante (0..23) ; si minute != NULL : minute. */
int meteo_temps_calculer(float t[4], int *heure_utc, int *minute);

/* Fourni par app_reseau.c : heure UTC (s depuis 1970) ou 0 si Internet n'a pas encore repondu */
uint32_t reseau_heure_utc(void);
#endif
