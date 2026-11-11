"""Exporte le MLP Keras en C pur (float32) : meteo_mlp_ref.c / .h
Usage : python export_c_mlp.py models/meteo_k3_mlp.keras firmware_dir
Sert de reference (et de secours) face au code genere par X-CUBE-AI.
"""
import os, sys
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"
import numpy as np
import tensorflow as tf

src, outdir = sys.argv[1], sys.argv[2]
m = tf.keras.models.load_model(src)
dense = [l for l in m.layers if isinstance(l, tf.keras.layers.Dense)]
acts = [l.get_config()["activation"] for l in dense]
assert all(a == "relu" for a in acts[:-1]) and acts[-1] == "softmax", acts

def arr(a):
    return ",\n  ".join(", ".join(f"{v:.8g}f" for v in row) for row in np.atleast_2d(a))

dims = [dense[0].kernel.shape[0]] + [l.units for l in dense]
c = ["/* Genere par export_c_mlp.py depuis " + os.path.basename(src) + " -- NE PAS EDITER */",
     '#include "meteo_mlp_ref.h"', "#include <math.h>", "",
     "/* Optimisation locale : ce fichier est compile en -O3 meme dans un projet Debug (-O0).",
     "   Le reste du projet reste debuggable pas a pas. Mettre METEO_MLP_OPTIMISE a 0 pour comparer. */",
     "#if METEO_MLP_OPTIMISE && defined(__GNUC__) && !defined(__clang__)",
     '#pragma GCC optimize ("O3")',
     '#define METEO_MLP_OPTIM_TXT "-O3 (pragma local)"',
     "#else",
     '#define METEO_MLP_OPTIM_TXT "niveau du projet"',
     "#endif",
     "const char *meteo_mlp_ref_optim(void) { return METEO_MLP_OPTIM_TXT; }", ""]
for i, l in enumerate(dense):
    W, b = l.get_weights()                     # W : [entrees][sorties]
    rows = ",\n  ".join("{ " + ", ".join(f"{v:.8g}f" for v in r) + " }" for r in W)
    c.append(f"static const float W{i}[{W.shape[0]}][{W.shape[1]}] = {{\n  {rows}\n}};")
    c.append(f"static const float B{i}[{b.shape[0]}] = {{ " + ", ".join(f"{v:.8g}f" for v in b) + " };")
c.append("")
c.append("static void dense(const float *x, int nin, const float *W, const float *b, int nout, float *y, int relu)\n{\n"
         "  for (int j = 0; j < nout; j++)\n  {\n    float s = b[j];\n"
         "    for (int i = 0; i < nin; i++) { s += x[i] * W[i * nout + j]; }\n"
         "    y[j] = (relu && s < 0.0f) ? 0.0f : s;\n  }\n}\n")
maxd = max(dims)
body = ["int meteo_mlp_ref_predire(const float x_norm[METEO_MLP_NB_IN], float proba[METEO_MLP_NB_OUT])\n{",
        f"  float a[{maxd}], b[{maxd}];", "  const float *in = x_norm;"]
buf = ["a", "b"]
for i, l in enumerate(dense):
    last = i == len(dense) - 1
    tgt = "proba" if last else buf[i % 2]
    body.append(f"  dense(in, {dims[i]}, &W{i}[0][0], B{i}, {dims[i+1]}, {tgt}, {0 if last else 1});")
    if not last:
        body.append(f"  in = {tgt};")
body += ["  /* softmax (stable) + argmax */",
         "  float mx = proba[0]; int best = 0;",
         "  for (int j = 1; j < METEO_MLP_NB_OUT; j++) { if (proba[j] > mx) { mx = proba[j]; best = j; } }",
         "  float s = 0.0f;",
         "  for (int j = 0; j < METEO_MLP_NB_OUT; j++) { proba[j] = expf(proba[j] - mx); s += proba[j]; }",
         "  for (int j = 0; j < METEO_MLP_NB_OUT; j++) { proba[j] /= s; }",
         "  return best;", "}", ""]
c += body
h = ["/* Genere par export_c_mlp.py -- NE PAS EDITER */", "#ifndef METEO_MLP_REF_H", "#define METEO_MLP_REF_H", "",
     f"#define METEO_MLP_NB_IN   {dims[0]}", f"#define METEO_MLP_NB_OUT  {dims[-1]}",
     f"#define METEO_MLP_PARAMS  {m.count_params()}", "",
     "/* Reseau " + " -> ".join(map(str, dims)) + " (ReLU, softmax), poids float32 en Flash/RAM.",
     "   Entree : features NORMALISEES. Sortie : probabilites ; retour : indice de la classe. */",
     "int meteo_mlp_ref_predire(const float x_norm[METEO_MLP_NB_IN], float proba[METEO_MLP_NB_OUT]);", "",
     "/* 1 = compiler l'inference en -O3 (pragma), 0 = niveau d'optimisation du projet.",
     "   Surchargeable depuis la ligne de commande : -DMETEO_MLP_OPTIMISE=0 */",
     "#ifndef METEO_MLP_OPTIMISE", "#define METEO_MLP_OPTIMISE 1", "#endif",
     "/* Texte decrivant l'optimisation reellement utilisee (pour les logs du benchmark) */",
     "const char *meteo_mlp_ref_optim(void);", "",
     "#endif", ""]
os.makedirs(outdir, exist_ok=True)
open(os.path.join(outdir, "meteo_mlp_ref.c"), "w").write("\n".join(c))
open(os.path.join(outdir, "meteo_mlp_ref.h"), "w").write("\n".join(h))
print("ok", dims, m.count_params())
