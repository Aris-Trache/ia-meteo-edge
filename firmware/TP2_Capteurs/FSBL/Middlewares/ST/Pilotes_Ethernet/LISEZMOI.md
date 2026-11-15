# Pilotes Ethernet (TP2.3)

Copies **non modifiees** de fichiers ST (STM32Cube_FW_N6 V1.4.1, licence BSD-3 / ST) :

| Fichier | Origine dans le package Cube |
|---|---|
| stm32n6xx_hal_eth.c, stm32n6xx_hal_eth_ex.c | Drivers/STM32N6xx_HAL_Driver/Src |
| stm32n6xx_hal_rif.c | Drivers/STM32N6xx_HAL_Driver/Src |
| lan8742.c | Drivers/BSP/Components/lan8742 (PHY de la NUCLEO-N657X0-Q) |

Pourquoi ici et pas dans `Drivers/` ? Le projet CubeIDE reference les fichiers HAL
par des *liens* que CubeMX regenere. Notre .ioc n'active pas l'Ethernet (FreeRTOS
et LwIP ne sont pas proposes par CubeMX pour le STM32N6), donc on les compile
depuis ce dossier, qui est un vrai dossier source du projet.
Les en-tetes (.h) restent dans `Drivers/` (deja dans les chemins d'inclusion).
