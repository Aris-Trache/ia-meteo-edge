"""Exporte le modele int8 (.tflite) en C pour CMSIS-NN (TP3.3, comparaison Edge).

CMSIS-NN est la bibliotheque d'ARM qu'utilise le runtime TFLite Micro de X-CUBE-AI
pour les noyaux int8 ; sur Cortex-M55 elle exploite les instructions vectorielles
Helium (MVE). On appelle directement ses noyaux, avec EXACTEMENT les parametres
de quantification du .tflite (meme calcul que TFLite Micro) : le resultat doit
etre identique, a l'octet pres, a l'interpreteur TFLite de reference.

Genere :  meteo_mlp_s8.h / meteo_mlp_s8.c
Usage :   python tools/export_cmsis_nn.py models/meteo_k3_mlp_int8.tflite <dossier_sortie>
"""
import math
import os
import sys

import numpy as np

os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
import tensorflow as tf  # noqa: E402

src, outdir = sys.argv[1], sys.argv[2]
it = tf.lite.Interpreter(model_path=src)
it.allocate_tensors()
T = {t["index"]: t for t in it.get_tensor_details()}
ops = [o for o in it._get_ops_details() if o["op_name"] != "DELEGATE"]
assert [o["op_name"] for o in ops] == ["QUANTIZE", "FULLY_CONNECTED", "FULLY_CONNECTED",
                                       "FULLY_CONNECTED", "SOFTMAX", "DEQUANTIZE"], ops


def qp(i):
    q = T[i]["quantization_parameters"]
    return np.array(q["scales"], dtype=np.float64), np.array(q["zero_points"], dtype=np.int64)


def quantize_multiplier(m):
    """tflite::QuantizeMultiplier (arrondi double, comme TFLite Micro / CMSIS-NN)."""
    if m == 0.0:
        return 0, 0
    q, shift = math.frexp(m)
    q_fixed = int(round(q * (1 << 31)))
    if q_fixed == (1 << 31):
        q_fixed //= 2
        shift += 1
    if shift < -31:
        return 0, 0
    return q_fixed, shift


# ---- entree (QUANTIZE) ----
q_in = ops[0]
s_in, z_in = qp(q_in["outputs"][0])

couches = []
for k, op in enumerate(ops[1:4], 1):
    x_i, w_i, b_i = op["inputs"]
    y_i = op["outputs"][0]
    sx, zx = qp(x_i)
    sw, zw = qp(w_i)
    sy, zy = qp(y_i)
    assert np.all(zw == 0), "poids symetriques attendus"
    W = it.get_tensor(w_i).astype(np.int8)          # [sorties, entrees]
    B = it.get_tensor(b_i).astype(np.int32)
    mult, shift = zip(*(quantize_multiplier(float(sx[0] * swc / sy[0])) for swc in sw))
    relu = k < 3                                    # ReLU fusionnee dans les 2 premieres couches
    act_min = int(zy[0]) if relu else -128
    couches.append(dict(W=W, B=B, mult=mult, shift=shift, in_off=int(-zx[0]),
                        out_off=int(zy[0]), act_min=act_min, act_max=127,
                        n_in=W.shape[1], n_out=W.shape[0]))

# ---- softmax : parametres calcules comme TFLite Micro (CalculateSoftmaxParams) ----
sm = ops[4]
s_sm_in, _ = qp(sm["inputs"][0])
s_out, z_out = qp(sm["outputs"][0])
K_BITS = 5
real = min(1.0 * s_sm_in[0] * (1 << (31 - K_BITS)), (1 << 31) - 1.0)
sm_mult, sm_shift = quantize_multiplier(real)       # QuantizeMultiplierGreaterThanOne
assert sm_shift >= 0
radius = math.floor(((1 << K_BITS) - 1) * (1 << (31 - K_BITS)) / (1 << sm_shift))
sm_diff_min = -radius


def arr(nom, ctype, a, par_ligne=16):
    a = np.asarray(a).ravel()
    lignes = [", ".join(str(int(v)) for v in a[i:i + par_ligne]) for i in range(0, len(a), par_ligne)]
    return f"static const {ctype} {nom}[{len(a)}] = {{\n  " + ",\n  ".join(lignes) + "\n};\n"


H = f"""/* Genere par tools/export_cmsis_nn.py depuis {os.path.basename(src)} -- NE PAS EDITER
 * Reseau meteo int8 (quantification par canal) execute avec les noyaux CMSIS-NN
 * (ceux du runtime TFLite Micro de X-CUBE-AI ; Helium/MVE sur Cortex-M55). */
#ifndef METEO_MLP_S8_H
#define METEO_MLP_S8_H

#define METEO_S8_NB_IN   {couches[0]['n_in']}
#define METEO_S8_NB_OUT  {couches[-1]['n_out']}

/* Entree : features NORMALISEES (float, comme meteo_mlp_ref_predire)
   Sortie : probabilites (float). Retour : indice de la classe la plus probable. */
int meteo_mlp_s8_predire(const float x_norm[METEO_S8_NB_IN], float proba[METEO_S8_NB_OUT]);

/* Variante sans conversions : entree deja quantifiee, sortie int8 brute du softmax */
int meteo_mlp_s8_predire_q(const signed char x_q[METEO_S8_NB_IN], signed char y_q[METEO_S8_NB_OUT]);

/* Chemin de calcul compile ("Helium (MVE)", "DSP" ou "C pur") */
const char *meteo_mlp_s8_backend(void);

#endif
"""

C = [f"/* Genere par tools/export_cmsis_nn.py depuis {os.path.basename(src)} -- NE PAS EDITER */",
     '#include "meteo_mlp_s8.h"', '#include "../cmsis_nn/arm_nnfunctions.h"', "#include <math.h>", "#include <string.h>", "",
     "/* -O3 local, comme meteo_mlp_ref.c et cmsis_nn_build.c (projet Debug en -O0) */",
     "#if defined(__GNUC__) && !defined(__clang__)", '#pragma GCC optimize ("O3")', "#endif", "",
     f"#define Q_IN_SCALE   {s_in[0]:.9g}f", f"#define Q_IN_ZP      {int(z_in[0])}",
     f"#define Q_OUT_SCALE  {s_out[0]:.9g}f", f"#define Q_OUT_ZP     {int(z_out[0])}",
     f"#define SM_MULT      {sm_mult}", f"#define SM_SHIFT     {sm_shift}", f"#define SM_DIFF_MIN  {sm_diff_min}", ""]
for k, c in enumerate(couches, 1):
    C.append(f"/* couche {k} : {c['n_in']} -> {c['n_out']}{' + ReLU' if k < 3 else ''} */")
    C.append(arr(f"W{k}", "int8_t", c["W"]))
    C.append(arr(f"B{k}", "int32_t", c["B"], 8))
    C.append(arr(f"M{k}", "int32_t", c["mult"], 8))
    C.append(arr(f"S{k}", "int32_t", c["shift"], 16))
C.append("""/* Sommes des lignes de poids pre-calculees (biais inclus) : exigees par le noyau
   Helium. Calculees une fois au premier appel (arm_vector_sum_s8). */
""")
for k, c in enumerate(couches, 1):
    C.append(f"static int32_t KSUM{k}[{c['n_out']}];")
C.append("static int ksum_pret = 0;\n")


def fc_call(k, c, src_buf, dst_buf):
    return f"""  {{
    cmsis_nn_context ctx = {{ KSUM{k}, (int32_t)sizeof(KSUM{k}) }};
    cmsis_nn_fc_params fc = {{ {c['in_off']}, 0, {c['out_off']}, {{ {c['act_min']}, {c['act_max']} }} }};
    cmsis_nn_per_channel_quant_params qp = {{ (int32_t *)M{k}, (int32_t *)S{k} }};
    cmsis_nn_dims d_in = {{ 1, 1, 1, {c['n_in']} }}, d_w = {{ {c['n_in']}, 1, 1, {c['n_out']} }};
    cmsis_nn_dims d_b = {{ 1, 1, 1, {c['n_out']} }}, d_out = {{ 1, 1, 1, {c['n_out']} }};
    if (arm_fully_connected_per_channel_s8(&ctx, &fc, &qp, &d_in, {src_buf}, &d_w, W{k},
                                           &d_b, B{k}, &d_out, {dst_buf}) != ARM_CMSIS_NN_SUCCESS)
      return -1;
  }}"""


C.append("static void preparer(void)\n{")
for k, c in enumerate(couches, 1):
    C.append(f"  arm_vector_sum_s8(KSUM{k}, {c['n_in']}, {c['n_out']}, W{k}, {c['in_off']}, 0, B{k});")
C.append("  ksum_pret = 1;\n}\n")
C.append("int meteo_mlp_s8_predire_q(const signed char x_q[METEO_S8_NB_IN], signed char y_q[METEO_S8_NB_OUT])\n{")
C.append(f"  int8_t a1[{couches[0]['n_out']}], a2[{couches[1]['n_out']}], z[{couches[2]['n_out']}];")
C.append("  if (!ksum_pret) { preparer(); }")
C.append(fc_call(1, couches[0], "(const int8_t *)x_q", "a1"))
C.append(fc_call(2, couches[1], "a1", "a2"))
C.append(fc_call(3, couches[2], "a2", "z"))
C.append("  arm_softmax_s8(z, 1, METEO_S8_NB_OUT, SM_MULT, SM_SHIFT, SM_DIFF_MIN, (int8_t *)y_q);")
C.append("  int best = 0;\n  for (int i = 1; i < METEO_S8_NB_OUT; i++) { if (y_q[i] > y_q[best]) { best = i; } }")
C.append("  return best;\n}\n")
C.append("""int meteo_mlp_s8_predire(const float x_norm[METEO_S8_NB_IN], float proba[METEO_S8_NB_OUT])
{
  signed char xq[METEO_S8_NB_IN], yq[METEO_S8_NB_OUT];
  for (int i = 0; i < METEO_S8_NB_IN; i++)        /* QUANTIZE (arrondi comme TFLite) */
  {
    long v = lroundf(x_norm[i] / Q_IN_SCALE) + Q_IN_ZP;
    xq[i] = (signed char)(v < -128 ? -128 : (v > 127 ? 127 : v));
  }
  int c = meteo_mlp_s8_predire_q(xq, yq);
  if (c < 0) { return c; }
  for (int i = 0; i < METEO_S8_NB_OUT; i++)       /* DEQUANTIZE */
  {
    proba[i] = ((int)yq[i] - Q_OUT_ZP) * Q_OUT_SCALE;
  }
  return c;
}

const char *meteo_mlp_s8_backend(void)
{
#if defined(ARM_MATH_MVEI)
  return "Helium (MVE)";
#elif defined(ARM_MATH_DSP)
  return "DSP";
#else
  return "C pur";
#endif
}
""")

os.makedirs(outdir, exist_ok=True)
open(os.path.join(outdir, "meteo_mlp_s8.h"), "w").write(H)
open(os.path.join(outdir, "meteo_mlp_s8.c"), "w").write("\n".join(C))
print("ok : couches", [(c["n_in"], c["n_out"]) for c in couches], "| softmax", sm_mult, sm_shift, sm_diff_min)
