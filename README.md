# IA Météo Edge — ETRS923 (USMB, Master ESET)

Classification météo à partir de capteurs embarqués (température, humidité, pression)
sur **NUCLEO-N657X0 + X-NUCLEO-IKS01A3**, avec comparaison **Cloud AI (ThingSpeak/MATLAB)**
vs **Edge AI (X-CUBE-AI sur STM32N6)**.

## Architecture
```
Capteurs (HTS221, LPS22HH, STTS751) --I2C--> STM32N6 --(Ethernet/LwIP)--> ThingSpeak
                                               |                              |
                                       Inférence locale              Inférence MATLAB (ONNX)
                                       (X-CUBE-AI / NPU)                      |
                                               <----------- TalkBack ---------
```

## Arborescence
| Dossier      | Contenu |
|--------------|---------|
| `notebooks/` | TP1 (MNIST) et TP3 (Meteostat) — entraînement Python/TensorFlow (Colab) |
| `firmware/`  | Projets STM32CubeIDE (TP2 capteurs + réseau, TP3 inférence embarquée) |
| `matlab/`    | Scripts ThingSpeak (inférence cloud, TalkBack) |
| `models/`    | Modèles exportés (.keras, .onnx, .tflite) |
| `data/`      | Jeux de données (ou script de récupération Meteostat) |
| `docs/`      | Résultats, figures, pitch, déclaration d'usage de l'IA |

## Avancement
- [ ] TP1 — MNIST : activations, optimiseurs, fonctions coût
- [ ] TP2 — LED / chenillard / logs UART
- [ ] TP2 — Lecture capteurs I²C + LEDs d'état
- [ ] TP2 — FreeRTOS + LwIP + ping
- [ ] TP3 — Modèle Meteostat (classification)
- [ ] TP3 — Cloud : ONNX → MATLAB → ThingSpeak + TalkBack
- [ ] TP3 — Edge : X-CUBE-AI sur N6, latence, consommation
- [ ] TP4 — Démo + pitch 3 min

## Usage de l'IA
Voir [`docs/DECLARATION_IA.md`](docs/DECLARATION_IA.md) (charte IA USMB, Annexe A).
