# TP3 — Le même réseau sur le NPU Neural-ART (branche `npu`)

## Résultat mesuré sur la carte (NUCLEO-N657X0, CPU 600 MHz, NPU 300 MHz)

| Cible | Temps / inférence | Décision |
|---|---|---|
| CPU float32, caches OFF | 199,5 µs | référence |
| CPU float32, caches ON (-O3) | 14,7 µs | identique |
| CPU int8 CMSIS-NN (Helium) | **8,2 µs** | identique |
| NPU Neural-ART (int8) — runtime ST en -O0 | 122 µs | identique |
| NPU Neural-ART (int8) — runtime ST en -O3 | **57,3 µs** | identique (15/15 vecteurs, et en continu sur capteurs) |

**Conclusion** : le NPU donne exactement les mêmes décisions, mais il est ~7× plus lent que le
CPU int8 sur *ce* réseau. Notre MLP ne fait que 1 186 multiplications-additions : le coût fixe
(lancer le NPU, enchaîner ses 7 « époques », gérer les caches, et 3 opérations faites en logiciel —
quantification d'entrée, softmax, déquantification) dépasse le calcul. Le NPU (jusqu'à 600 GOPS)
est fait pour des réseaux convolutifs (images, audio) où il gagne d'un facteur 10 à 100.
**Choix retenu pour le produit : CPU Cortex-M55 + Helium (int8 CMSIS-NN)** — le NPU reste
disponible et validé pour un futur modèle plus gros (ex. caméra nuages).

## Comment c'est fait
1. `tools/npu/` : ST Edge AI Core 2.2 (inclus dans X-CUBE-AI 10.2.1) compile
   `models/meteo_k3_mlp_int8.tflite` pour le NPU (profil `meteo`, pool mémoire AXISRAM2 0x34100000,
   commande dans `tools/npu/LISEZMOI.md`). Résultat : 4 époques matérielles (couches denses),
   3 logicielles, 2,5 Ko de mémoire.
2. `FSBL/Core/Src/npu/` : code généré (`meteo_npu.c`), poids (`meteo_npu_params.c`, copiés en RAM au
   démarrage), nos opérations logicielles (`npu_sw_ops.c`, sans la bibliothèque binaire ST) et l'API
   `npu_meteo_init()` / `npu_meteo_predire()` (`npu_meteo.c` : horloge et reset NPU, droits RIF,
   exécution en mode polling, bare metal).
3. `FSBL/Middlewares/ST/AI_NPU/` : runtime `ll_aton` de ST (compilé en -O3 par `#pragma`, même en Debug,
   pour une comparaison équitable avec le code CPU).
4. Édition de liens : le runtime NPU est placé dans une zone libre d'AXISRAM2 (`NPUCODE`, 0x34110000)
   car la zone FSBL (255 Ko) était pleine.

## Pistes non réalisées
Horloge NPU à 600 MHz, ne pas réinitialiser le réseau à chaque inférence, traiter plusieurs
mesures par lot : gains possibles, mais le coût fixe restera supérieur à 8 µs pour un réseau aussi petit.
