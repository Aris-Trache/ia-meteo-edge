# Tests sur PC (« tests hôte »)

On teste le code C de la carte **sur le PC**, avant de le flasher : un bug trouvé ici coûte 1 s au lieu d'une session de debug.

| Test | Ce qu'il vérifie |
|---|---|
| `test_meteo.c` | features C == features Python (588 valeurs), réseau C == Keras (8/8 décisions), pression niveau mer |
| `test_capteurs_iks4a1.c` | pilote `capteurs.c` sur un bus I²C **simulé** d'IKS4A1 : détection, registres, CRC SHT40, conversions |
| `test_mlp_s8.c` | réseau **int8 CMSIS-NN** == interpréteur TFLite de référence, **à l'octet près** (400 vecteurs) |

```bash
F=../TP2_Capteurs/FSBL/Core
# Chaine IA (features + reseau)
gcc -O2 -I$F/Src/meteo test_meteo.c $F/Src/meteo/meteo_features.c $F/Src/meteo/meteo_mlp_ref.c -lm -o test_meteo && ./test_meteo
# Pilote capteurs
gcc -O2 -I. -I$F/Inc -I$F/Src test_capteurs_iks4a1.c $F/Src/capteurs.c $F/Src/stmems/*_reg.c -lm -o test_capteurs && ./test_capteurs
# Reseau int8 CMSIS-NN (sur PC : chemin "C pur" de CMSIS-NN ; sur la carte : Helium)
gcc -O2 -I$F/Src/meteo test_mlp_s8.c $F/Src/meteo/meteo_mlp_s8.c $F/Src/cmsis_nn/cmsis_nn_build.c -lm -o test_s8 && ./test_s8
```
Les vecteurs de référence (`vecteurs_*.h`) sont générés par le notebook TP3 (`vecteurs_s8.h` : interpréteur TFLite `BUILTIN_REF`).
