import json, numpy as np, tensorflow as tf
def gen(H, nom, fic):
    m = tf.keras.models.load_model(f'tools/saison/meteo_k3_temps_{H}h.keras'); r = json.load(open(f'tools/saison/res_temps_{H}h.json'))
    Ws = [l.get_weights() for l in m.layers if l.get_weights()]
    a2 = lambda n, a: f"static const float {n}[{a.shape[0]}][{a.shape[1]}] = {{\n" + ",\n".join("  { " + ", ".join(f"{x:.8g}f" for x in row) + " }" for row in a) + "\n};\n"
    a1 = lambda n, a: f"static const float {n}[{len(a)}] = {{ " + ", ".join(f"{x:.8g}f" for x in a) + " };\n"
    quoi = "MAINTENANT" if H == 0 else f"DANS {H} H"
    c = [f"/* Genere par tools/heure_saison/export_temps.py -- NE PAS EDITER */",
         f"/* IA meteo {quoi} avec l'heure et la saison : 14 features normalisees + 4 entrees de temps",
         f"   (hsin, hcos, dsin, dcos). Test 2024-2025 : F1 {r['avec_saison']['f1']} (sans heure/saison : {r['sans_saison']['f1']}). */",
         f'#include "{fic}.h"', "#include <math.h>", '#pragma GCC optimize ("O3")', "",
         a1("MU_T", r['mu_temps']), a1("SD_T", r['sd_temps'])]
    for k, (W, b) in enumerate(Ws): c += [a2(f"W{k}", W), a1(f"B{k}", b)]
    c += [f"""static void dense(const float *x, int nin, const float *W, const float *b, int nout, float *y, int relu)
{{
  for (int j = 0; j < nout; j++)
  {{
    float s = b[j];
    for (int i = 0; i < nin; i++) {{ s += x[i] * W[i * nout + j]; }}
    y[j] = (relu && s < 0.0f) ? 0.0f : s;
  }}
}}

int {nom}(const float x_norm[14], const float temps[4], float proba[3])
{{
  float in[18], a[32], b[16];
  for (int i = 0; i < 14; i++) {{ in[i] = x_norm[i]; }}
  for (int i = 0; i < 4; i++) {{ in[14 + i] = (temps[i] - MU_T[i]) / SD_T[i]; }}
  dense(in, 18, &W0[0][0], B0, 32, a, 1);
  dense(a, 32, &W1[0][0], B1, 16, b, 1);
  dense(b, 16, &W2[0][0], B2, 3, proba, 0);
  int best = 0; float mx = proba[0], s = 0.0f;
  for (int j = 1; j < 3; j++) {{ if (proba[j] > mx) {{ mx = proba[j]; best = j; }} }}
  for (int j = 0; j < 3; j++) {{ proba[j] = expf(proba[j] - mx); s += proba[j]; }}
  for (int j = 0; j < 3; j++) {{ proba[j] /= s; }}
  return best;
}}
"""]
    open(f'{fic}.c', 'w').write("\n".join(c))
    open(f'{fic}.h', 'w').write(f"""/* IA meteo {quoi}, avec l'heure et la saison (voir {fic}.c) */
#ifndef {fic.upper()}_H
#define {fic.upper()}_H
/* x_norm : les 14 features NORMALISEES ; temps : hsin, hcos, dsin, dcos BRUTS (voir meteo_temps.h).
   Sortie : probabilites des 3 classes ; retour : indice de la classe. */
int {nom}(const float x_norm[14], const float temps[4], float proba[3]);
#endif
""")
    v = json.load(open(f'tools/saison/vect_temps_{H}h.json'))
    mu = np.array(r['mu_temps']); sd = np.array(r['sd_temps'])
    t = ['#include <stdio.h>', f'#include "{fic}.h"', 'int main(){',
         f'static const float X[8][14]={{' + ','.join('{' + ','.join(f'{x:.8g}f' for x in e['x_norm'][:14]) + '}' for e in v) + '};',
         f'static const float T[8][4]={{' + ','.join('{' + ','.join(f'{x:.8g}f' for x in (np.array(e['x_norm'][14:]) * sd + mu)) + '}' for e in v) + '};',
         f'static const float P[8][3]={{' + ','.join('{' + ','.join(f'{x:.8g}f' for x in e['proba']) + '}' for e in v) + '};',
         f'float m=0;int ok=0;for(int i=0;i<8;i++){{float p[3];int c={nom}(X[i],T[i],p);int k=0;for(int j=0;j<3;j++){{float d=p[j]-P[i][j];if(d<0)d=-d;if(d>m)m=d;if(P[i][j]>P[i][k])k=j;}}ok+=(c==k);}}',
         f'printf("{fic} : decisions %d/8 ecart max %g\\n",ok,m);}}']
    open(f'test_{fic}.c', 'w').write("\n".join(t))
gen(0, 'meteo_temps_predire', 'meteo_mlp_temps'); gen(3, 'meteo_prev3h_temps_predire', 'meteo_prev3h_temps')
