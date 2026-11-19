# NPU Neural-ART — génération du code (branche `npu`)

Profil `meteo` : une seule zone mémoire pour le NPU, **AXISRAM2 0x34100000, 512 Ko**
(libre : notre programme FSBL commence à 0x34180400). Pas de mémoire externe,
pas d'AXISRAM3-6 (évite d'avoir à les alimenter).

Commande (à lancer depuis la racine du dépôt, dans l'invite de commandes) :
```
"D:\STM32Cube\Repository\Packs\STMicroelectronics\X-CUBE-AI\10.2.1\Utilities\windows\stedgeai.exe" generate -m models\meteo_k3_mlp_int8.tflite --target stm32n6 --st-neural-art meteo@tools\npu\neuralart_meteo.json --name meteo_npu --output tools\npu\sortie --workspace tools\npu\ws
```
