"""Exporte le modele meteo vers MATLAB (TP3.2, partie Cloud).

Genere dans matlab/ :
  meteo_modele.m            poids + normalisation + classes (struct M)
  donnees_test_meteo.m      vecteurs de reference (Python/Keras) pour test_meteo_matlab.m
  thingspeak_meteo_cloud.m  script AUTONOME a coller dans "MATLAB Analysis" de ThingSpeak
                            (ThingSpeak ne voit pas les fichiers du depot : tout est inclus)

Usage (depuis la racine du depot) :
  python tools/export_matlab.py models/meteo_k3_mlp.keras models/meteo_k3_mlp_meta.json \
         models/meteo_k3_mlp_vecteurs_test.json firmware/tests_hote/vecteurs_features.h matlab
"""
import json
import os
import re
import sys

import numpy as np

src_keras, src_meta, src_vect, src_feat, outdir = sys.argv[1:6]

os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
import keras  # noqa: E402

model = keras.models.load_model(src_keras, compile=False)
dense = [l for l in model.layers if l.get_weights()]
meta = json.load(open(src_meta))


def mat(a):
    """Matrice/vecteur numpy -> litteral MATLAB (17 chiffres significatifs : float32 exact)."""
    a = np.atleast_2d(np.asarray(a, dtype=np.float64))
    rows = [" ".join(f"{v:.9g}" for v in r) for r in a]
    return "[" + "; ...\n    ".join(rows) + "]"


def modele_m(nom_fonction="meteo_modele"):
    L = [f"function M = {nom_fonction}()",
         "%Genere par tools/export_matlab.py depuis " + os.path.basename(src_keras) + " -- NE PAS EDITER",
         "%  Reseau " + " -> ".join([str(dense[0].get_weights()[0].shape[0])] +
                                    [str(l.get_weights()[1].shape[0]) for l in dense]) +
         " (ReLU, softmax), meme poids que le .onnx, le .tflite et le C de la carte.",
         "  M.features = {" + ", ".join(f"'{f}'" for f in meta["features"]) + "};",
         "  M.classes  = {" + ", ".join(f"'{c}'" for c in meta["classes"]) + "};",
         f"  M.mu       = {mat(meta['mean'])};",
         f"  M.sigma    = {mat(meta['std'])};",
         "  M.altitude_m = 240;   % altitude de la carte (pression station -> niveau de la mer)",
         f"  M.W = cell(1, {len(dense)}); M.b = cell(1, {len(dense)});"]
    for i, l in enumerate(dense, 1):
        W, b = l.get_weights()
        L.append(f"  M.W{{{i}}} = {mat(W)};")
        L.append(f"  M.b{{{i}}} = {mat(b)};")
    L.append("end")
    return "\n".join(L) + "\n"


# --- vecteurs de reference -----------------------------------------------------
vect = json.load(open(src_vect))
VX = np.array([v["features"] for v in vect])
VP = np.array([v["probabilites"] for v in vect])

txt = open(src_feat).read()


def c_array(name):
    body = re.search(name + r"\[[^\]]*\](?:\[[^\]]*\])?\s*=\s*\{(.*?)\};", txt, re.S).group(1)
    rows = re.findall(r"\{([^{}]*)\}", body) or [body]
    out = [[float("nan") if t.strip() == "NAN" else float(t.strip().rstrip("f"))
            for t in r.split(",") if t.strip()] for r in rows]
    return np.array(out)


SEQ, EXP, COLD0 = c_array("SEQ"), c_array("EXP"), c_array("COLD0")


def mat_nan(a):
    return mat(a).replace("nan", "NaN")


donnees = "\n".join([
    "function D = donnees_test_meteo()",
    "%Genere par tools/export_matlab.py -- vecteurs de reference calcules en Python / Keras",
    "%  SEQ : 48 h reelles (Chambery, 10-11/03/2024) [T RH P_mer] ; EXP : features Python attendues",
    "%  COLD0 : features au demarrage a froid ; VX / VP : entrees brutes et probabilites Keras",
    f"  D.SEQ = {mat(SEQ)};",
    f"  D.EXP = {mat_nan(EXP)};",
    f"  D.COLD0 = {mat(COLD0)};",
    f"  D.VX = {mat(VX)};",
    f"  D.VP = {mat(VP)};",
    "end", ""])

# --- script ThingSpeak autonome ------------------------------------------------
here = os.path.dirname(os.path.abspath(__file__))
mdir = os.path.join(os.path.dirname(here), "matlab")
tete = open(os.path.join(mdir, "thingspeak_entete.m.txt"), encoding="utf-8").read()
fonctions = []
for f in ["meteo_cloud_etape.m", "historique_horaire.m", "meteo_features.m", "meteo_predire.m"]:
    fonctions.append(open(os.path.join(mdir, f), encoding="utf-8").read().strip())
script = (tete.rstrip() + "\n\n%% ===== Fonctions locales (copies conformes de matlab/*.m) =====\n\n"
          + "\n\n".join(fonctions) + "\n\n" + modele_m("meteo_modele").strip() + "\n")

os.makedirs(outdir, exist_ok=True)
open(os.path.join(outdir, "meteo_modele.m"), "w", encoding="utf-8").write(modele_m())
open(os.path.join(outdir, "donnees_test_meteo.m"), "w", encoding="utf-8").write(donnees)
open(os.path.join(outdir, "thingspeak_meteo_cloud.m"), "w", encoding="utf-8").write(script)
print("ok :", [l.get_weights()[0].shape for l in dense], "|", len(VX), "vecteurs |", SEQ.shape[0], "h")
