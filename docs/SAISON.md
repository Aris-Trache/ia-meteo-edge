# Branche `saison` : l'heure et la date en entrée de l'IA

**Conclusion : ajouter l'heure et le jour de l'année fait gagner +0,022 de F1 à l'IA « maintenant » (0,604 → 0,626)
et +0,015 à l'IA « dans 3 h » (0,585 → 0,600). C'est surtout l'heure qui apporte ; la saison seule n'apporte rien.**

## Résultats (test 2024–2025, un essai par configuration)
| IA | Entrées | F1 | Rappel pluie | F1 démarrage à froid |
|---|---|---|---|---|
| Maintenant | 14 (version `npu`) | 0,604 | 0,697 | 0,563 |
| Maintenant | 14 + saison | 0,605 | 0,726 | 0,571 |
| **Maintenant** | **14 + heure + saison** | **0,626** | **0,728** | **0,592** |
| Dans 3 h | 14 (version `npu`) | 0,585 | 0,631 | 0,535 |
| Dans 3 h | 14 + saison | 0,584 | 0,653 | 0,551 |
| **Dans 3 h** | **14 + heure + saison** | **0,600** | **0,650** | **0,570** |

Lecture : la température capte déjà la saison. L'heure apporte le cycle de la journée (rosée du matin, orages
d'après-midi), que les capteurs seuls voient mal.

## Ce qui change sur la carte (cette branche)
- 4 entrées en plus : `hsin`, `hcos` (heure UTC), `dsin`, `dcos` (jour de l'année), mêmes formules que Python.
- **D'où vient l'heure** (`meteo_temps.c`) : des réponses ThingSpeak dès qu'Internet a répondu ; sinon de la
  **date de compilation** + le temps écoulé depuis le démarrage (la carte est relancée juste après la compilation).
- Les 2 IA principales (maintenant, dans 3 h) utilisent les 18 entrées, en float32 (environ 15 µs).
- L'IA 14 entrées reste calculée pour comparaison (float, int8 CMSIS-NN, NPU) et affichée dans la console :
  `(heure 08:45 UTC, source Internet) | sans heure ni saison : …`.
- Vérifications : C == Keras sur 8 vecteurs pour chaque IA (écart < 1,5·10⁻⁷) ; calcul de l'heure et du jour
  de l'année identique à pandas.

## Limites
- Pas encore de versions int8 ni NPU des IA 18 entrées.
- L'IA du Cloud (MATLAB) reste à 14 entrées : la comparaison Edge ↔ Cloud porte sur le modèle 14 entrées.
- Hors Internet, l'heure vient de la compilation : juste si la carte est lancée juste après la compilation.
- Scripts : `tools/saison/train_heure_saison.py` (`0` ou `3`) et `export_heure_saison.py`.
