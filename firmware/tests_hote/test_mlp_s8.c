/* Test hote : reseau int8 CMSIS-NN (meteo_mlp_s8.c) == interpreteur TFLite de reference,
   a l'octet pres (quantification d'entree ET sorties du softmax). */
#include <stdio.h>
#include <math.h>
#include "meteo_mlp_s8.h"
#include "vecteurs_s8.h"

int main(void)
{
  int diff_q = 0, diff_y = 0;
  for (int v = 0; v < NB_S8; v++)
  {
    signed char yq[3];
    /* 1) chaine int8 seule, a partir des entrees deja quantifiees par TFLite */
    meteo_mlp_s8_predire_q(S8_Q[v], yq);
    for (int j = 0; j < 3; j++) { diff_y += (yq[j] != S8_Y[v][j]); }
    /* 2) API float complete (quantification faite par notre code) */
    float p[3];
    meteo_mlp_s8_predire(S8_X[v], p);
    for (int j = 0; j < 3; j++) { diff_q += fabsf(p[j] - (S8_Y[v][j] + 128) / 256.0f) > 1e-6f; }
  }
  printf("reseau int8 : %d vecteurs, octets differents de TFLite : %d (chaine int8), %d (API float) | backend %s\n",
         NB_S8, diff_y, diff_q, meteo_mlp_s8_backend());
  return (diff_y == 0 && diff_q == 0) ? 0 : 1;
}
