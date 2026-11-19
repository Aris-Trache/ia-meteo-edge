# Runtime NPU Neural-ART (ST)

Copie des fichiers du runtime NPU de **X-CUBE-AI 10.2.1** (`Middlewares/ST/AI/Npu/ll_aton`
et `Npu/Devices/STM32N6XX`), licence STMicroelectronics (voir le pack X-CUBE-AI).

Seule modification : `ll_aton_runtime.c` définit `PRIx64` si la newlib-nano ne le fait pas.

Configuration (symboles du projet CubeIDE) :
`LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6`, `LL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL`,
`LL_ATON_RT_MODE=LL_ATON_RT_POLLING` (pas d'interruption NPU, plus simple avec FreeRTOS).

Le code de ce dossier et le réseau généré sont placés par l'éditeur de liens dans la zone
`NPUCODE` (0x34110000), en dehors des 255 Ko réservés au FSBL.
