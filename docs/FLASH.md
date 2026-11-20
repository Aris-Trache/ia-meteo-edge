# Branche `flash` : démarrer la carte toute seule, sans PC

Le STM32N6 n'a pas de flash interne. Au démarrage en mode « flash », sa ROM de boot lit **une image signée**
dans la **flash externe** de la Nucleo (adresse 0x70000000), la copie en RAM (0x34180400, 255 Ko maximum) et la lance.

## Ce que change cette branche (par rapport à `npu`)
- `.cproject` : configuration Debug compilée en **-Os** (au lieu de -O0). Les fichiers d'IA gardent leur `#pragma O3`.
- `STM32N657X0HXQ_AXISRAM2_fsbl.ld` : plus de zone `.npu_text` ; tout le programme tient dans les 255 Ko
  (≈ 194 Ko, vérifié par compilation), en **une seule image**, comme l'exige la ROM de boot.
- Le code est identique : IA (float, int8, NPU, dans 3 h), réseau, page web, mode hors ligne.

## Procédure (sur le PC, avec STM32CubeProgrammer installé)
1. **Compiler** dans CubeIDE (Refresh puis Build) : on obtient `Debug/TP2_Capteurs_FSBL.bin`.
2. **Vérifier en mode développement** que tout marche encore (F11 puis F8), cavaliers comme d'habitude.
3. **Signer l'image** (outil fourni avec STM32CubeProgrammer, dossier `bin`) :
   ```
   STM32_SigningTool_CLI.exe -bin Debug\TP2_Capteurs_FSBL.bin -nk -of 0x80000000 -t fsbl -o Debug\TP2_Capteurs_FSBL-trusted.bin -hv 2.3 -dump Debug\TP2_Capteurs_FSBL-trusted.bin
   ```
4. **Écrire dans la flash externe**, carte toujours en mode développement :
   ```
   STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG -el "<dossier CubeProgrammer>\bin\ExternalLoader\MX25UM51245G_STM32N6570-NUCLEO.stldr" -hardRst -w Debug\TP2_Capteurs_FSBL-trusted.bin 0x70000000
   ```
   Le nom exact du chargeur dépend de la version : prendre dans `ExternalLoader` celui qui contient `N6570-NUCLEO`.
5. **Passer le cavalier BOOT1 (JP2) dans l'autre position** (démarrage sur la flash), débrancher puis rebrancher l'USB.
6. La carte démarre seule : console sur COM3 (sans debug), page web à l'adresse affichée.

Retour au mode développement : remettre JP2 en 2-3, puis F11/F8 comme avant.

## À vérifier sur la carte (non testé à ce jour)
- Le nom du chargeur externe et la position du cavalier BOOT1 pour le démarrage sur la flash (manuel UM3417 de la Nucleo-N657X0-Q).
- Les temps d'inférence en -Os (les fichiers d'IA restent en -O3 par `#pragma`).
