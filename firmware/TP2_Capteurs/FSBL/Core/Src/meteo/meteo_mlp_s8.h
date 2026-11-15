/* Genere par tools/export_cmsis_nn.py depuis meteo_k3_mlp_int8.tflite -- NE PAS EDITER
 * Reseau meteo int8 (quantification par canal) execute avec les noyaux CMSIS-NN
 * (ceux du runtime TFLite Micro de X-CUBE-AI ; Helium/MVE sur Cortex-M55). */
#ifndef METEO_MLP_S8_H
#define METEO_MLP_S8_H

#define METEO_S8_NB_IN   14
#define METEO_S8_NB_OUT  3

/* Entree : features NORMALISEES (float, comme meteo_mlp_ref_predire)
   Sortie : probabilites (float). Retour : indice de la classe la plus probable. */
int meteo_mlp_s8_predire(const float x_norm[METEO_S8_NB_IN], float proba[METEO_S8_NB_OUT]);

/* Variante sans conversions : entree deja quantifiee, sortie int8 brute du softmax */
int meteo_mlp_s8_predire_q(const signed char x_q[METEO_S8_NB_IN], signed char y_q[METEO_S8_NB_OUT]);

/* Chemin de calcul compile ("Helium (MVE)", "DSP" ou "C pur") */
const char *meteo_mlp_s8_backend(void);

#endif
