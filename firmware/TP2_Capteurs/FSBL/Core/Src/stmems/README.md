# Drivers capteurs STMicroelectronics (STMems_Standard_C_drivers)

Fichiers `xxx_reg.c / xxx_reg.h` copiés sans modification depuis les dépôts officiels ST
(sous-modules de https://github.com/STMicroelectronics/STMems_Standard_C_drivers) :

| Capteur | Dépôt | Commit | Rôle |
|---|---|---|---|
| HTS221  | STMicroelectronics/hts221-pid  | a91e690 (2026-10-08) | humidité + température |
| LPS22HH | STMicroelectronics/lps22hh-pid | e919ee1 (2026-07-09) | pression + température |
| STTS751 | STMicroelectronics/stts751-pid | 5a0af60 (2026-10-08) | température de précision |
| LPS22DF | STMicroelectronics/lps22df-pid | c13c244 | pression + température (X-NUCLEO-IKS4A1) |
| STTS22H | STMicroelectronics/stts22h-pid | 1f26d89 | température de précision (X-NUCLEO-IKS4A1) |

Licence BSD-3-Clause (voir `LICENSE_*.md`). La couche plateforme (read_reg / write_reg
sur le HAL I2C du STM32N6) est implémentée dans `Core/Src/capteurs.c`.

Le SHT40 (humidité + température de la X-NUCLEO-IKS4A1) n'a pas de registres : il obéit à
des commandes de 1 octet (0xFD = mesure haute précision). Son protocole est codé directement
dans `capteurs.c` (avec la vérification CRC-8 de la datasheet Sensirion).
