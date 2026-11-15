# Déclaration d'usage de l'IA (Charte USMB — Annexe A)

1. **Outil et version** : Claude (Anthropic), modèle Claude Opus 5.5, novembre 2026.
   **Objectif** : accompagnement pédagogique, explications, aide à la structuration du code
   et à la rédaction de la documentation. Usage autorisé par l'enseignant.
2. **Étapes & prompts principaux** :
   - TP1 : structure du notebook MNIST (protocole, boucle d'expériences), analyse des résultats et rédaction de `TP1_RESULTATS.md`. Expériences exécutées sur Colab par l'auteur, choix validés par l'auteur.
   - TP2 : configuration CubeMX (I²C1, broches, pull-up) guidée pas à pas ; code du chenillard, des logs UART
     et du pilote capteurs (`capteurs.c`, couche plateforme des drivers ST). Diagnostic de la carte capteurs
     (IKS4A1 identifiée par scan I²C) et adaptation du pilote aux deux cartes.
   - TP3 : récupération des données Meteostat, conception des features calculables sur la carte, notebook
     d'études (nombre de classes, historique, taille du réseau, démarrage à froid), exports ONNX / TFLite int8 / C,
     code embarqué des features et de l'inférence, banc de mesure du temps d'inférence, rédaction de `TP3_RESULTATS.md`.
     Partie Cloud (scripts MATLAB / ThingSpeak, testés sous GNU Octave) ; version int8 CMSIS-NN générée depuis le
     `.tflite` et vérifiée à l'octet près contre TFLite ; installation de X-CUBE-AI dans CubeMX.
   - TP2.3 : portage à la main de FreeRTOS V11.1.0 (port Cortex-M55) et LwIP 2.2.1 sur le projet FSBL du STM32N6
     (absents de CubeMX pour ce composant), pilote Ethernet LwIP ↔ HAL (MPU non cacheable, sécurité RIF, PHY LAN8742)
     en reprenant la configuration de l'exemple officiel ST ; tâches capteurs/IA et réseau, ping par socket BSD,
     client HTTP ThingSpeak, serveur echo. Bugs trouvés en test sur carte et corrigés (DHCP lancé sur interface
     inactive, tampon HTTP trop petit).
   - Mode de travail : l'IA a eu accès au PC de l'auteur pendant une session (compilation et exécution dans
     STM32CubeIDE, lecture de la console série) ; les commits et `git push` sont faits par l'auteur.
3. **Vérifications et validations humaines** : chaque code a été relu, compris, compilé
   et testé sur carte / Colab par l'auteur. Contrôles automatiques ajoutés : parité features C / Python
   (588 valeurs, écart < 5·10⁻⁵), parité réseau C / Keras (8/8 décisions), ONNX / Keras (écart 1,2·10⁻⁷),
   pilote capteurs testé sur bus I²C simulé puis sur la carte réelle, réseau int8 CMSIS-NN == TFLite
   (400 vecteurs, 0 octet différent), MATLAB == Python (5·10⁻⁷). Toutes les mesures du rapport TP3
   (prédictions, temps d'inférence) ont été relevées sur la carte.
4. **Données personnelles** : non utilisées. Données météo : Meteostat, licence CC BY-NC 4.0.
