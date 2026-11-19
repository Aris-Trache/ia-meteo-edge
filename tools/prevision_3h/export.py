import json, numpy as np, tensorflow as tf
m = tf.keras.models.load_model('tools/prevision_3h/meteo_k3_prev3h.keras'); r = json.load(open('tools/prevision_3h/res_3h.json'))
Ws = [l.get_weights() for l in m.layers if l.get_weights()]
def arr2(n, a): return f"static const float {n}[{a.shape[0]}][{a.shape[1]}] = {{\n" + ",\n".join("  { " + ", ".join(f"{x:.8g}f" for x in row) + " }" for row in a) + "\n};\n"
def arr1(n, a): return f"static const float {n}[{a.shape[0]}] = {{ " + ", ".join(f"{x:.8g}f" for x in a) + " };\n"
c = ["/* Genere par tools/prevision_3h/export.py depuis meteo_k3_prev3h.keras -- NE PAS EDITER */",
     "/* IA de PREVISION : meme entree que l'IA du moment (14 features normalisees),",
     f"   sortie = classe meteo DANS 3 H. Test 2024-2025 : F1 macro {r['modele']['f1']}, rappel pluie {r['rappel_pluie']}. */",
     '#include "meteo_prev3h.h"', "#include <math.h>", '#pragma GCC optimize ("O3")', ""]
for k, (W, b) in enumerate(Ws): c += [arr2(f"P{k}", W), arr1(f"Q{k}", b)]
c += ["""static void dense(const float *x, int nin, const float *W, const float *b, int nout, float *y, int relu)
{
  for (int j = 0; j < nout; j++)
  {
    float s = b[j];
    for (int i = 0; i < nin; i++) { s += x[i] * W[i * nout + j]; }
    y[j] = (relu && s < 0.0f) ? 0.0f : s;
  }
}

int meteo_prev3h_predire(const float x_norm[14], float proba[3])
{
  float a[32], b[16];
  dense(x_norm, 14, &P0[0][0], Q0, 32, a, 1);
  dense(a, 32, &P1[0][0], Q1, 16, b, 1);
  dense(b, 16, &P2[0][0], Q2, 3, proba, 0);
  int best = 0; float mx = proba[0], s = 0.0f;
  for (int j = 1; j < 3; j++) { if (proba[j] > mx) { mx = proba[j]; best = j; } }
  for (int j = 0; j < 3; j++) { proba[j] = expf(proba[j] - mx); s += proba[j]; }
  for (int j = 0; j < 3; j++) { proba[j] /= s; }
  return best;
}
"""]
open('meteo_prev3h.c', 'w').write("\n".join(c))
open('meteo_prev3h.h', 'w').write("""/* IA de prevision a 3 h (voir meteo_prev3h.c) */
#ifndef METEO_PREV3H_H
#define METEO_PREV3H_H
/* Entree : les 14 features NORMALISEES (les memes que l'IA du moment).
   Sortie : probabilites des 3 classes DANS 3 H ; retour : indice de la classe. */
int meteo_prev3h_predire(const float x_norm[14], float proba[3]);
#endif
""")
# test parite C/Keras
v = json.load(open('tools/prevision_3h/vect_3h.json'))
t = ['#include <stdio.h>', '#include "meteo_prev3h.h"', 'int main(){', f'static const float X[{len(v)}][14]={{' + ','.join('{' + ','.join(f'{x:.8g}f' for x in e['x_norm']) + '}' for e in v) + '};',
     f'static const float P[{len(v)}][3]={{' + ','.join('{' + ','.join(f'{x:.8g}f' for x in e['proba']) + '}' for e in v) + '};',
     'float m=0;int ok=0;for(int i=0;i<' + str(len(v)) + ';i++){float p[3];int c=meteo_prev3h_predire(X[i],p);int k=0;for(int j=0;j<3;j++){float d=p[j]-P[i][j];if(d<0)d=-d;if(d>m)m=d;if(P[i][j]>P[i][k])k=j;}ok+=(c==k);}',
     'printf("decisions %d/' + str(len(v)) + ' ecart max %g\\n",ok,m);}']
open('test_prev.c', 'w').write("\n".join(t))
