"""
meteo_features.py — Features et classes meteo, implementation de reference.

ETRS923 (USMB, Master ESET) — Aris Trache.

Regle d'or du projet : les entrees du modele sont UNIQUEMENT des grandeurs que la carte
X-NUCLEO-IKS01A3 sait mesurer (temperature, humidite, pression) et ce qu'on en derive.
La MEME logique est reimplementee en C sur la STM32 (firmware : meteo_features.c) ;
toute modification ici doit etre repercutee la-bas (et les tests de coherence relances).
"""
import numpy as np
import pandas as pd

# ------------------------------------------------------------------ features
# Grandeurs instantanees
BASE = [
    "temp",      # temperature [degC]                       (HTS221 / STTS751)
    "rhum",      # humidite relative [%]                    (HTS221)
    "pres",      # pression ramenee au niveau de la mer [hPa] (LPS22HH + altitude)
    "spread",    # temperature - point de rosee [degC] : ~0 = air sature (brouillard, pluie)
]
# Tendances courtes (1 h / 3 h) : le principe du barometre
TREND3 = ["dp3", "dp1", "dt3", "drh3"]
# Historique 6 h
HIST6 = ["dp6", "dt1", "drh1", "rhmax6", "rhmin6", "sp_mean6"]
# Historique 24 h (etudie, NON retenu : 24 h de chauffe avant la 1re prediction)
HIST24 = ["dp12", "dp24", "trng24"]
# Heure / jour de l'annee (etudie, NON retenu : necessite une horloge synchronisee)
TIME = ["hsin", "hcos", "dsin", "dcos"]

FEATURES_FINAL = BASE + TREND3 + HIST6          # 14 entrees du modele embarque
HISTORY_HOURS = 6                                # profondeur d'historique necessaire


def dew_point(t, rh):
    """Point de rosee [degC], formule de Magnus (a=17.62, b=243.12). Identique en C."""
    a, b = 17.62, 243.12
    rh = np.clip(rh, 1.0, 100.0)
    g = np.log(rh / 100.0) + a * t / (b + t)
    return b * g / (a - g)


def sea_level_pressure(p_station, t, altitude_m):
    """Pression station -> niveau de la mer (formule hypsometrique, atmosphere standard).
    Indispensable SUR LA CARTE : le LPS22HH mesure la pression absolue (~985 hPa a 240 m)
    alors que Meteostat fournit la pression reduite au niveau de la mer (~1013 hPa)."""
    return p_station * (1.0 - 0.0065 * altitude_m / (t + 0.0065 * altitude_m + 273.15)) ** -5.257


def build_features(g):
    """g : UNE station, index horaire continu, colonnes temp / rhum / pres (niveau mer)."""
    f = pd.DataFrame(index=g.index)
    t, rh, p = g["temp"], g["rhum"], g["pres"]
    f["temp"], f["rhum"], f["pres"] = t, rh, p
    f["spread"] = t - dew_point(t, rh)
    f["dp3"], f["dp1"] = p - p.shift(3), p - p.shift(1)
    f["dt3"], f["drh3"] = t - t.shift(3), rh - rh.shift(3)
    f["dp6"], f["dt1"], f["drh1"] = p - p.shift(6), t - t.shift(1), rh - rh.shift(1)
    # fenetre glissante = heure courante + 5 heures precedentes (6 valeurs, comme en C)
    f["rhmax6"], f["rhmin6"] = rh.rolling(6).max(), rh.rolling(6).min()
    f["sp_mean6"] = f["spread"].rolling(6).mean()
    f["dp12"], f["dp24"] = p - p.shift(12), p - p.shift(24)
    f["trng24"] = t.rolling(24).max() - t.rolling(24).min()
    h, d = g.index.hour.values, g.index.dayofyear.values
    f["hsin"], f["hcos"] = np.sin(2 * np.pi * h / 24), np.cos(2 * np.pi * h / 24)
    f["dsin"], f["dcos"] = np.sin(2 * np.pi * d / 365.25), np.cos(2 * np.pi * d / 365.25)
    return f


def cold_start(f):
    """Valeurs des features quand la carte vient de demarrer (aucun historique) :
    tendances = 0, min/max/moyenne 6 h = valeur courante. Identique au C."""
    x = f.copy()
    for c in ("dp3", "dp1", "dt3", "drh3", "dp6", "dt1", "drh1", "dp12", "dp24", "trng24"):
        if c in x:
            x[c] = 0.0
    if "rhmax6" in x:
        x["rhmax6"] = f["rhum"]
        x["rhmin6"] = f["rhum"]
    if "sp_mean6" in x:
        x["sp_mean6"] = f["spread"]
    return x


# ------------------------------------------------------------------ classes
# Codes "coco" Meteostat : https://dev.meteostat.net/formats.html#weather-condition-codes
COCO_NAMES = {1: "Clear", 2: "Fair", 3: "Cloudy", 4: "Overcast", 5: "Fog", 6: "Freezing Fog",
              7: "Light Rain", 8: "Rain", 9: "Heavy Rain", 10: "Freezing Rain",
              11: "Heavy Freezing Rain", 12: "Sleet", 13: "Heavy Sleet", 14: "Light Snowfall",
              15: "Snowfall", 16: "Heavy Snowfall", 17: "Rain Shower", 18: "Heavy Rain Shower",
              19: "Sleet Shower", 20: "Heavy Sleet Shower", 21: "Snow Shower",
              22: "Heavy Snow Shower", 23: "Lightning", 24: "Hail", 25: "Thunderstorm",
              26: "Heavy Thunderstorm", 27: "Storm"}

_RAIN = (7, 8, 9, 17, 18)
_SNOW_MIX = (10, 11, 12, 13, 14, 15, 16, 19, 20, 21, 22)
_STORM = (23, 24, 25, 26, 27)

# Regroupements etudies (TP3 1.b : nombre de classes / precision / taille).
# Le "vent fort" du sujet est exclu : sans anemometre, la carte ne peut pas l'observer.
GROUPINGS = {
    "K2": {"names": ["Sec", "Precipitations-Phenomene"],
           "map": {**{c: 0 for c in (1, 2, 3, 4)}, **{c: 1 for c in range(5, 28)}}},
    "K3": {"names": ["Degage", "Nuageux-Couvert", "Precipitations"],
           "map": {1: 0, 2: 0, 3: 1, 4: 1, 5: 1, 6: 1, **{c: 2 for c in range(7, 28)}}},
    "K5": {"names": ["Degage", "Nuageux-Couvert", "Brouillard", "Pluie-Orage", "Neige-Mixte"],
           "map": {1: 0, 2: 0, 3: 1, 4: 1, 5: 2, 6: 2, **{c: 3 for c in _RAIN + _STORM},
                   **{c: 4 for c in _SNOW_MIX}}},
    "K6": {"names": ["Degage", "Nuageux-Couvert", "Brouillard", "Pluie", "Neige-Mixte", "Orage"],
           "map": {1: 0, 2: 0, 3: 1, 4: 1, 5: 2, 6: 2, **{c: 3 for c in _RAIN},
                   **{c: 4 for c in _SNOW_MIX}, **{c: 5 for c in _STORM}}},
    "K10": {"names": ["Clair", "Peu nuageux", "Partiellement nuageux", "Couvert", "Brouillard",
                      "Pluie", "Averses", "Neige", "Pluie+neige melees", "Orage"],
            "map": {1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 4, 7: 5, 8: 5, 9: 5, 17: 6, 18: 6,
                    14: 7, 15: 7, 16: 7, 21: 7, 22: 7, 10: 8, 11: 8, 12: 8, 13: 8, 19: 8, 20: 8,
                    23: 9, 24: 9, 25: 9, 26: 9, 27: 9}},
}

# ------------------------------------------------------------------ dataset
PLAUSIBLE = {"temp": (-30.0, 45.0), "rhum": (1.0, 100.0), "pres": (950.0, 1060.0)}
MAX_DELTA = {"dp1": 4.0, "dp3": 8.0, "dt3": 15.0, "drh3": 60.0}   # au-dela : erreur de mesure


def load_dataset(path):
    """CSV Meteostat multi-stations -> DataFrame (features + coco + station), nettoye.
    Les trous de mesure sont rendus explicites (reindex horaire) pour que les tendances
    ne soient jamais calculees entre deux heures non consecutives."""
    df = pd.read_csv(path, dtype={"station": str, "ts": str})
    df["time"] = pd.to_datetime(df["ts"], format="%Y%m%d%H")
    for col, (lo, hi) in PLAUSIBLE.items():
        df.loc[(df[col] < lo) | (df[col] > hi), col] = np.nan
    parts = []
    for st, g in df.groupby("station"):
        g = g.set_index("time").sort_index()
        g = g[~g.index.duplicated()]
        g = g.reindex(pd.date_range(g.index.min(), g.index.max(), freq="h"))
        f = build_features(g)
        f["coco"] = g["coco"]
        f["station"] = st
        parts.append(f)
    d = pd.concat(parts)
    needed = BASE + TREND3 + HIST6 + HIST24
    d = d.dropna(subset=needed + ["coco"])
    d = d[d["coco"] > 0]
    for col, m in MAX_DELTA.items():
        d = d[d[col].abs() <= m]
    d["coco"] = d["coco"].astype(int)
    return d
