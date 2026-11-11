# TP3 — IA de classification météo : résultats

**ETRS923 — USMB Master ESET** · Auteur : Aris Trache · Notebook : [`notebooks/TP3_Meteostat_IA.ipynb`](../notebooks/TP3_Meteostat_IA.ipynb)
(exécutable tel quel sur Colab, il clone ce dépôt).

## 1. Le problème posé
Classer **la météo du moment** à partir de ce que la carte capteurs mesure vraiment : **température, humidité, pression**.
On s'interdit toute entrée que la carte ne peut pas produire (vent, nébulosité, pluviomètre…) : un modèle
qui dépend d'une donnée absente sur la carte ne sert à rien en vrai.

## 2. Données
- **Meteostat**, horaire, 2017 → 2025, **6 stations du même climat** : Chambéry/Aix (07491, à 3 km du campus),
  Lyon (07481), Grenoble (07486), Genève (06700), Annecy (LFLP0), Montélimar (07577).
  Script de récupération : [`data/fetch_meteostat.py`](../data/fetch_meteostat.py). Licence **CC BY-NC 4.0 (Meteostat)**.
- 459 205 heures brutes → **359 637 heures exploitables** après nettoyage (valeurs impossibles, sauts aberrants,
  trous horaires rendus explicites pour qu'aucune tendance ne soit calculée entre deux heures non consécutives).
- **Découpage temporel** : entraînement 2017–2022, validation 2023, **test 2024–2025**.
  *Pourquoi* : un découpage aléatoire mettrait deux heures voisines (presque identiques) en entraînement ET en test,
  le score serait artificiellement gonflé.
- Étiquette = code météo Meteostat (`coco`), regroupé en classes.

## 3. Entrées (features) — toutes calculables sur la carte
| Groupe | Features | Calcul embarqué |
|---|---|---|
| Instantané | `temp`, `rhum`, `pres`, `spread` = T − T_rosée | capteurs + formule de Magnus |
| Tendances 3 h | `dp3`, `dp1`, `dt3`, `drh3` | différences avec l'historique |
| Historique 6 h | `dp6`, `dt1`, `drh1`, `rhmax6`, `rhmin6`, `sp_mean6` | buffer circulaire de 6 moyennes horaires |

**Piège évité — la pression** : le capteur mesure la pression **absolue** (~985 hPa à 240 m) alors que Meteostat
fournit la pression **ramenée au niveau de la mer** (~1013 hPa). La carte applique donc la formule hypsométrique
(altitude 240 m) avant l'IA. Sans ça, toutes les entrées « pression » seraient décalées de 28 hPa.

## 4. Études (test 2024–2025)
Métrique principale : **F1 macro** (moyenne des F1 de chaque classe). *Pourquoi* : l'accuracy seule récompense
un modèle qui répond toujours « nuageux » (classe majoritaire) ; le F1 macro oblige à reconnaître aussi la pluie.

### Étude 1 — combien de classes ? (8 entrées, réseau 32-16)
| Classes | Accuracy | Acc. équilibrée | F1 macro | Réf. « toujours la classe majoritaire » |
|---|---|---|---|---|
| 2 (sec / précipitations) | 0,788 | 0,767 | 0,720 | **0,799** |
| **3 (dégagé / nuageux-couvert / précipitations)** | **0,609** | 0,617 | 0,592 | 0,553 |
| 5 | 0,549 | 0,578 | 0,465 | 0,503 |
| 6 | 0,517 | 0,574 | 0,397 | 0,503 |
| 10 | 0,366 | 0,387 | 0,275 | 0,285 |

**Choix : 3 classes.** À 2 classes, le modèle fait *moins bien* que la réponse constante « pas de pluie » (0,788 < 0,799) :
il n'apporte rien. Au-delà de 3, les classes (neige, brouillard, orage…) ne se distinguent plus avec T/HR/P seules.

### Étude 2 — quelle profondeur d'historique ? (3 classes)
| Entrées | F1 macro |
|---|---|
| Instantané (4) | 0,562 |
| + tendances 3 h (8) | 0,592 |
| **+ historique 6 h (14) ← retenu** | **0,612** |
| + historique 24 h (17) | 0,620 |
| + heure / saison (18) | 0,627 |

**Choix : 6 h.** C'est le principe du baromètre : la *variation* de pression annonce la dégradation (+0,05 de F1).
24 h n'apporte que +0,008 mais impose 24 h de RAM et 24 h d'attente après chaque redémarrage.
L'heure / saison demanderait une horloge temps réel synchronisée : écarté pour rester autonome.

### Étude 3 — quelle taille de réseau ? (14 entrées, 3 classes)
| Couches | Paramètres | F1 macro |
|---|---|---|
| 8 | 147 | 0,586 |
| 16 | 291 | 0,598 |
| **32-16 ← retenu** | **1 059** | **0,612** |
| 64-32 | 3 139 | 0,615 |
| 128-64-32 | 12 355 | 0,614 |
| Gradient boosting (référence non neuronale) | — | 0,575 |

**Choix : 32-16.** Le score plafonne ensuite : ×3 ou ×12 paramètres pour +0,003. L'information est limitée par les
capteurs, pas par le modèle. Le réseau bat aussi le gradient boosting, référence classique sur données tabulaires.

### Étude 4 — démarrage à froid
À l'allumage, la carte n'a aucun historique. On entraîne avec une part d'exemples « sans historique »
(tendances = 0, min/max/moyennes = valeur courante) :

| Part d'exemples sans historique | F1 avec historique | F1 au démarrage à froid |
|---|---|---|
| 0 % | 0,612 | 0,546 |
| **15 % ← retenu** | 0,606 | **0,564** |
| 30 % | 0,605 | 0,557 |

**Choix : 15 %.** On perd 0,006 en régime établi pour gagner 0,018 dans les premières heures.

## 5. Modèle final
Réseau dense **14 → 32 → 16 → 3** (ReLU, softmax), **1 059 paramètres**, poids de classe ∝ fréquence^−0,5.

| Test 2024–2025 | Accuracy | Acc. équilibrée | F1 macro |
|---|---|---|---|
| 6 stations | **0,620** | **0,633** | **0,606** (±0,002 sur 5 graines) |
| Chambéry seule | 0,672 | 0,704 | 0,622 |

| Classe | Précision | Rappel |
|---|---|---|
| Dégagé | 0,596 | 0,590 |
| Nuageux-Couvert | 0,694 | 0,617 |
| Précipitations | 0,486 | **0,692** |

Lecture : **7 épisodes de pluie sur 10 sont détectés**, au prix de fausses alertes (précision 0,49).
C'est le compromis voulu pour une station météo (mieux vaut prévenir). Matrice de confusion :
[`notebooks/resultats_tp3/confusion_finale.png`](../notebooks/resultats_tp3/confusion_finale.png).

**Limite honnête** : distinguer « dégagé » de « nuageux » sans capteur de lumière reste difficile.
Un capteur de luminosité serait l'amélioration matérielle la plus rentable.

## 6. Exports
| Cible | Fichier | Vérification |
|---|---|---|
| Cloud (MATLAB / ThingSpeak) | `models/meteo_k3_mlp.onnx` | écart max ONNX / Keras = 1,2·10⁻⁷ |
| Edge X-CUBE-AI | `models/meteo_k3_mlp_int8.tflite` (5 360 o) | accuracy 0,602 → 0,600, **97,1 %** de décisions identiques |
| C pur (référence embarquée) | `firmware/.../meteo/meteo_mlp_ref.c` | **8/8 décisions identiques** à Keras, écart proba 2,3·10⁻⁵ |
| Normalisation + classes | `models/meteo_k3_mlp_meta.json`, `meteo_model_params.h` | même code de features C/Python : écart max 4,9·10⁻⁵ sur 588 valeurs |

La normalisation est faite **hors du modèle**, dans le code C (`meteo_features.c`). On a ainsi le même modèle pour ONNX, TFLite et C,
et la même fonction de features testée contre Python (`firmware/tests_hote/`).

## 7. Sur la carte (NUCLEO-N657X0-Q)
### Carte capteurs : X-NUCLEO-IKS4A1 (et non IKS01A3)
Le scan I²C a trouvé `0x19 0x1E 0x38 0x44 0x5D 0x6A 0x6B` et un WHO_AM_I = `0xB4` en 0x5D : c'est la **IKS4A1**,
remplaçante de l'IKS01A3. Le pilote `capteurs.c` reconnaît désormais **les deux cartes** et range les composants par rôle :

| Rôle | IKS01A3 | IKS4A1 (carte utilisée) |
|---|---|---|
| Humidité + T | HTS221 | **SHT40** (protocole par commandes, CRC-8 vérifié) |
| Pression + T | LPS22HH | **LPS22DF** (16 moyennes internes + filtre passe-bas) |
| T de précision | STTS751 | **STTS22H** |

Le reste du programme (features, IA) n'a pas changé : les grandeurs mesurées sont les mêmes.
La température donnée au modèle est celle du **capteur d'humidité** : l'humidité relative est mesurée à *sa* température,
donc le point de rosée n'est juste qu'avec ce couple-là.

### Fonctionnement mesuré
```
Carte detectee : X-NUCLEO-IKS4A1
T=23.46 degC  HR=48.79 %  P_mer=999.63 hPa  dP3h=0.00 hPa  | historique 0/6 h
   => Nuageux-Couvert  (Degage 12%, Nuageux-Couvert 50%, Precipitations 37%)
```
Une prédiction toutes les 2 s (LED rouge = lecture, bleue = inférence, verte = attente), historique horaire sur 6 h.
Mesure faite en intérieur, alors que la météo Windows annonçait « pluie fine » : la classe « précipitations » est la 2ᵉ (37 %),
cohérent avec une pression basse (999,6 hPa). Sans historique, le modèle est en mode « démarrage à froid ».

### Temps d'inférence (C pur, float32, Cortex-M55 à 600 MHz)
| Compilation de l'inférence | Caches L1 OFF | Caches L1 ON |
|---|---|---|
| -O0 (Debug par défaut) | 808 µs | 63,2 µs |
| **-O3 (pragma local)** | 146 µs | **14,7 µs** |

**×55 sans toucher au réseau**, uniquement par la configuration du processeur et du compilateur.
- Les **caches** (×10 à ×13) sont désactivés par défaut dans le FSBL : sans eux, chaque lecture de poids
  va jusqu'à la SRAM AXI. Le programme travaille par scrutation (pas de DMA), donc activer le D-cache est sans risque.
- **-O3** (×4 à ×5) : les variables restent dans les registres au lieu d'aller-retours en pile.
  Il est appliqué par `#pragma` au seul fichier du réseau, ce qui laisse le reste du projet en -O0, debuggable pas à pas.
- 14,7 µs ≈ 8 800 cycles pour 1 059 multiplications-additions + softmax. Une inférence toutes les 2 s occupe le CPU
  **0,0007 %** du temps : la consommation sera dominée par les capteurs et l'attente, pas par l'IA.

Méthode : boucle de paquets de 1 000 inférences, répétée jusqu'à dépasser 300 ms (précision < 0,5 % malgré le tick de 1 ms).
Résultat rappelé dans la console toutes les 30 s.

## 8. Reste à faire
- [ ] Cloud : import ONNX dans MATLAB, inférence sur ThingSpeak, retour par TalkBack (nécessite un compte ThingSpeak).
- [ ] X-CUBE-AI : import du `.tflite` int8 dans CubeMX, temps CPU vs NPU (Neural-ART) comparé au C pur ci-dessus.
- [ ] Consommation : mesure du courant (IDD) en attente / mesure / inférence.
