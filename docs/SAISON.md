# Étude : ajouter la saison en entrée de l'IA

**Conclusion : la saison n'améliore pas le score global (F1 identique à ±0,001) ; elle gagne 2 à 3 points sur la détection
de pluie et un peu au démarrage à froid. Modèle embarqué conservé sans saison.**

## Méthode
- Mêmes données Meteostat (6 stations), même découpage temporel (train 2017–2022, val 2023, **test 2024–2025**).
- Même réseau 14 → 32 → 16 → 3, même entraînement (15 % d'exemples « démarrage à froid »).
- 2 entrées ajoutées : jour de l'année codé en sinus / cosinus (`dsin`, `dcos`), soit 16 entrées.
- Script : [`tools/saison/train_saison.py`](../tools/saison/train_saison.py) (`0` = maintenant, `3` = dans 3 h).

## Résultats (test 2024–2025, un essai par configuration)
| IA | F1 sans saison | F1 avec saison | Rappel pluie sans → avec | F1 démarrage à froid sans → avec |
|---|---|---|---|---|
| Maintenant | 0,604 | 0,605 | 0,697 → 0,726 | 0,563 → 0,571 |
| Dans 3 h | 0,585 | 0,584 | 0,631 → 0,653 | 0,535 → 0,551 |

## Lecture
- La **température capte déjà l'essentiel de la saison** : l'information ajoutée est presque redondante.
- Les petits gains (pluie, démarrage à froid) sont à confirmer sur plusieurs graines avant toute décision.
- Sur la carte, la date pourrait venir de la date de compilation (`__DATE__`), de l'horloge interne (RTC) ou des réponses
  ThingSpeak ; l'intégrer imposerait de regénérer les versions int8, NPU et l'IA du Cloud pour un gain non significatif.
