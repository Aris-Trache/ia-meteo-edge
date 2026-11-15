# Consommation — estimation et protocole de mesure

> Pas de multimètre disponible pendant le projet : ce document donne une **estimation**
> tirée des datasheets (hypothèses écrites) et le **protocole** à appliquer à l'école.
> Les valeurs ci-dessous sont des ordres de grandeur, pas des mesures.

## 1. Bilan de puissance estimé (mode TP2.3 : IA + Ethernet)

| Bloc | Hypothèse / source | Ordre de grandeur |
|---|---|---|
| STM32N657, Cortex-M55 à 600 MHz, caches ON | non trouvé dans les sources consultées → **à mesurer** ; un cœur à 600 MHz consomme typiquement quelques centaines de mW | 200 – 500 mW (hypothèse) |
| STM32N657 en mode Stop | forum ST, valeurs typiques datasheet : IDDCORE ≈ 9 mA à 0,81 V, total ≈ 8 mW | ≈ 8 mW |
| PHY Ethernet LAN8742A, 100BASE-TX avec trafic | famille LAN87xx (LAN8710A) : ≈ 48 mA, ≈ 158 mW typ. | ≈ 160 mW |
| Transformateur Ethernet (magnétiques) | famille LAN87xx : ≈ 41 mA en 100BASE-TX | ≈ 135 mW à 3,3 V |
| PHY en « energy detect power down » | ≈ 6 mA, ≈ 20 mW | ≈ 20 mW |
| Capteurs SHT40 + LPS22DF + STTS22H | quelques µA à 1 mesure / 2 s | < 1 mW (négligeable) |
| ST-LINK + LEDs de la Nucleo | hors application (débogueur) | à exclure de la mesure |

**Conclusion de l'estimation :** le lien Ethernet (PHY + magnétiques ≈ 0,3 W) pèse autant
ou plus que le processeur. Pour un capteur météo sur batterie, la priorité serait de
**couper le PHY entre deux envois** (power-down) et de mettre le STM32 en Stop entre deux
mesures, bien avant d'optimiser l'IA.

## 2. Énergie d'une inférence (Edge) vs d'un envoi (Cloud)

Avec l'hypothèse P_cœur ≈ 0,3 W :

| Opération | Durée mesurée sur la carte | Énergie estimée |
|---|---|---|
| Inférence float32 -O3, caches ON | 14,7 µs | ≈ 4 µJ |
| Inférence int8 CMSIS-NN (Helium) | 8,2 µs | ≈ 2,5 µJ |
| Inférence float32 -O0, caches OFF (défaut) | 808 µs | ≈ 240 µJ |
| 1 envoi ThingSpeak (DNS + TCP + HTTP, ≈ 50–100 ms de lien actif) | ordre de grandeur du ping (20 ms) × quelques allers-retours | ≈ 30–60 mJ (PHY + magnétiques + cœur ≈ 0,6 W) |

→ **Une inférence locale coûte environ 10 000 fois moins d'énergie qu'un envoi au Cloud.**
C'est l'argument principal de l'Edge AI pour ce projet : la carte peut décider seule et
n'envoyer que lorsque c'est utile (changement de classe, alerte « précipitations »).

## 3. Protocole de mesure (à faire à l'école)

Matériel : multimètre en ampèremètre (calibre mA), ou analyseur de puissance.

1. **Isoler le courant du microcontrôleur** : retirer le cavalier de mesure de courant
   du STM32 (cavalier IDD, voir le manuel utilisateur de la NUCLEO-N657X0-Q) et brancher
   l'ampèremètre à sa place. Le courant du ST-LINK n'est alors pas compté.
2. **Mode A – repos** : programme bloqué dans une boucle vide (caches ON) → I_repos.
3. **Mode B – IA en continu** : boucle d'inférences (`mesurer_temps_inference`) → I_IA.
   Énergie par inférence = (I_IA − I_repos) × V × t_inférence.
4. **Mode C – TP2.3 complet** (`TP2_PARTIE 4`, câble branché) → I_réseau.
5. **Mode D – câble débranché** (PHY en détection d'énergie) → effet du lien Ethernet.
6. Pour le PHY (alimenté à part sur la carte), mesurer le courant total de la carte en USB
   (testeur USB) en modes C et D : la différence donne la part Ethernet.
7. Noter chaque valeur 3 fois, faire la moyenne, et compléter le tableau du §1.

## 4. NPU Neural-ART (perspective)

Le STM32N657 contient un accélérateur Neural-ART (jusqu'à 600 GOPS). Notre MLP est
minuscule (≈ quelques milliers d'opérations) : il tourne déjà en 8 µs sur le CPU, et le
temps de configuration du NPU dépasserait le calcul lui-même. Le NPU devient intéressant
pour des réseaux convolutifs (images, audio). Le déployer demande ST Edge AI Core
(compte myST) et un projet « Appli » (le FSBL de nos TP ne le permet pas dans CubeMX) :
c'est la suite logique du projet, non réalisée ici.
