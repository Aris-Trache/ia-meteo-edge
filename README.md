# IA Météo Edge — ETRS923 (USMB, Master ESET)

Classification météo à partir de capteurs embarqués (température, humidité, pression)
sur **NUCLEO-N657X0 + X-NUCLEO-IKS4A1** (IKS01A3 également supportée, détection automatique), avec comparaison **Cloud AI (ThingSpeak/MATLAB)**
vs **Edge AI (X-CUBE-AI sur STM32N6)**.

## Architecture
```
Capteurs (SHT40, LPS22DF, STTS22H) --I2C--> STM32N6 --(Ethernet/LwIP)--> ThingSpeak
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
| `tools/`     | Générateurs : notebook TP3, export du réseau en C |
| `firmware/tests_hote/` | Tests du code C sur PC (parité C/Python, pilote capteurs simulé) |

## Avancement
- [x] TP1 — MNIST : activations, optimiseurs, fonctions coût → [résultats](docs/TP1_RESULTATS.md)
- [x] TP2 — LED / chenillard / logs UART (cadencement sur échéance absolue, sans dérive)
- [x] TP2 — Lecture capteurs I²C + LEDs d'état (IKS4A1 et IKS01A3 reconnues automatiquement)
- [x] TP2 — FreeRTOS + LwIP : DHCP, DNS, ping google.com (20 ms), serveur echo TCP, envoi ThingSpeak → [résultats](docs/TP2_RESULTATS.md)
- [x] TP3 — Modèle Meteostat, 3 classes, F1 macro 0,606 → [résultats](docs/TP3_RESULTATS.md)
- [x] TP3 — Inférence embarquée sur la carte : float32 **14,7 µs**, int8 CMSIS-NN/Helium **8,2 µs** (×98 vs config par défaut)
- [x] TP3 — Cloud : script MATLAB / ThingSpeak + TalkBack prêt et testé ([matlab/](matlab/README.md))
- [x] TP3 — Cloud : la carte envoie ses mesures + sa prédiction sur le canal ThingSpeak « Meteo Capteurs »
- [x] TP3 — Cloud : canal Résultats + analyse MATLAB toutes les 10 min + TalkBack vers la carte (comparaison Edge/Cloud affichée par la carte)
- [x] TP3 — Consommation : estimation + protocole de mesure, NPU en perspective → [document](docs/CONSOMMATION.md)
- [ ] TP4 — Démo + pitch 3 min

## Démarrage rapide
- **Entraîner / rejouer le TP3** : ouvrir `notebooks/TP3_Meteostat_IA.ipynb` dans Colab (le dépôt est cloné automatiquement).
- **Carte** : importer `firmware/TP2_Capteurs` dans STM32CubeIDE, choisir le mode dans `main.c`
  (`TP2_PARTIE` 1 = chenillard, 2 = capteurs, 3 = IA météo, 4 = IA météo + FreeRTOS/LwIP/Ethernet), Debug, console COM 115200 bauds.
  Réseau (mode 4) : câble Ethernet carte ↔ PC + partage de connexion Windows (DHCP 192.168.137.x) ; clé ThingSpeak dans `FSBL/Core/Inc/secrets_thingspeak.h` (copie de `secrets_thingspeak_exemple.h`, ignoré par git).
  Boot de développement : BOOT0 = JP1 en 1-2, BOOT1 = JP2 en 2-3 (programme en RAM : relancer par Debug, pas par RESET).

## Usage de l'IA
Voir [`docs/DECLARATION_IA.md`](docs/DECLARATION_IA.md) (charte IA USMB, Annexe A).
