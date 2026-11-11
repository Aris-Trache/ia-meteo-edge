/* Test hote (PC) : coherence C <-> Python des features et du reseau.
   gcc -O2 -I.. test_meteo.c ../meteo_features.c ../meteo_mlp_ref.c -lm && ./a.out */
#include <stdio.h>
#include <math.h>
#include "meteo_features.h"
#include "meteo_mlp_ref.h"
#include "vecteurs_features.h"
#include "vecteurs_mlp.h"

int main(void)
{
  MeteoHistorique_t h; meteo_historique_init(&h);
  float out[METEO_NB_FEATURES]; float err_max = 0.0f; int compares = 0, ok_cold = 1;
  for (int t = 0; t < NB_H; t++)
  {
    meteo_calculer_features(&h, SEQ[t][0], SEQ[t][1], SEQ[t][2], out);
    if (t == 0)
      for (int i = 0; i < METEO_NB_FEATURES; i++) if (fabsf(out[i] - COLD0[i]) > 1e-3f) ok_cold = 0;
    if (t >= METEO_HISTORY_HOURS)               /* historique complet : doit egaler Python */
      for (int i = 0; i < METEO_NB_FEATURES; i++) { float e = fabsf(out[i] - EXP[t][i]); if (e > err_max) err_max = e; compares++; }
    meteo_historique_ajouter(&h, SEQ[t][0], SEQ[t][1], SEQ[t][2]);
  }
  printf("features : %d valeurs comparees, ecart max C/Python = %.2e | demarrage a froid %s\n",
         compares, err_max, ok_cold ? "OK" : "DIFFERENT");
  float pm = 0.0f; int dec = 0;
  for (int v = 0; v < NB_V; v++)
  {
    float xn[14], p[3]; meteo_normaliser(VX[v], xn);
    int c = meteo_mlp_ref_predire(xn, p);
    int best = 0; for (int j = 1; j < 3; j++) if (VP[v][j] > VP[v][best]) best = j;
    dec += (c == best);
    for (int j = 0; j < 3; j++) { float e = fabsf(p[j] - VP[v][j]); if (e > pm) pm = e; }
  }
  printf("reseau C : %d/%d decisions identiques a Keras, ecart max proba = %.2e\n", dec, NB_V, pm);
  printf("pression : 985.0 hPa a 240 m, 15 degC -> %.2f hPa (Python : 1013.4)\n",
         meteo_pression_niveau_mer(985.0f, 15.0f, 240.0f));
  return (err_max < 1e-3f && ok_cold && dec == NB_V && pm < 1e-4f) ? 0 : 1;
}
