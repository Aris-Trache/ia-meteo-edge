# 2ᵉ IA : prévision de la météo dans 3 h

L'IA principale reconnaît **la météo du moment** à partir des tendances des 6 dernières heures.
Une 2ᵉ IA, **même réseau (14 → 32 → 16 → 3), mêmes 14 entrées**, a été entraînée à prédire **la météo dans 3 h**.
Elle tourne sur la carte à chaque mesure, juste après la première, et s'affiche dans la console et sur la page web.

## Entraînement
- Données : Meteostat, 6 stations, mêmes nettoyage et découpage temporel (train 2017–2022, val 2023, **test 2024–2025**).
- Étiquette : la classe météo observée **3 h plus tard** à la même station.
- Même normalisation que l'IA du moment (le vecteur normalisé de la carte sert aux deux IA), 15 % d'exemples « démarrage à froid ».
- Scripts : [`tools/prevision_3h/train.py`](../tools/prevision_3h/train.py) et `export.py` (génère `meteo_prev3h.c`).
- Vérification C ↔ Keras : **8/8 décisions identiques**, écart max 1,5·10⁻⁷.

## Résultats (test 2024–2025, 91 447 heures)
| Méthode | Accuracy | F1 macro | Rappel pluie |
|---|---|---|---|
| IA « dans 3 h » | 0,607 | **0,585** | **0,63** |
| IA du moment réutilisée comme prévision | 0,590 | 0,572 | — |
| Persistance : « dans 3 h comme maintenant » (avec la **vraie** météo observée) | 0,804 | 0,787 | 0,74 |

## Lecture honnête
- La 2ᵉ IA prévoit **6 pluies sur 10, 3 h à l'avance**, avec seulement T, HR et P. Elle fait mieux que l'IA du moment
  réutilisée telle quelle (+0,013 de F1).
- La persistance fait bien mieux, mais **elle suppose de connaître la vraie météo actuelle** : un observateur ou une
  station complète (pluviomètre, capteur de nuages), que la carte n'a pas. Avec 3 capteurs simples, l'information disponible est la limite.
- Pistes : ajouter un capteur de luminosité ou de pluie, la vraie météo du moment comme entrée, ou un historique plus long.
