/* Genere par export_c_mlp.py -- NE PAS EDITER */
#ifndef METEO_MLP_REF_H
#define METEO_MLP_REF_H

#define METEO_MLP_NB_IN   14
#define METEO_MLP_NB_OUT  3
#define METEO_MLP_PARAMS  1059

/* Reseau 14 -> 32 -> 16 -> 3 (ReLU, softmax), poids float32 en Flash/RAM.
   Entree : features NORMALISEES. Sortie : probabilites ; retour : indice de la classe. */
int meteo_mlp_ref_predire(const float x_norm[METEO_MLP_NB_IN], float proba[METEO_MLP_NB_OUT]);

/* 1 = compiler l'inference en -O3 (pragma), 0 = niveau d'optimisation du projet.
   Surchargeable depuis la ligne de commande : -DMETEO_MLP_OPTIMISE=0 */
#ifndef METEO_MLP_OPTIMISE
#define METEO_MLP_OPTIMISE 1
#endif
/* Texte decrivant l'optimisation reellement utilisee (pour les logs du benchmark) */
const char *meteo_mlp_ref_optim(void);

#endif
