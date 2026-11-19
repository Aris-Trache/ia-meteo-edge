/**
  ******************************************************************************
  * @file    meteo_npu.h
  * @author  STEdgeAI
  * @date    2026-11-19 17:32:00
  * @brief   Minimal description of the generated c-implemention of the network
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef LL_ATON_METEO_NPU_H
#define LL_ATON_METEO_NPU_H

/******************************************************************************/
#define LL_ATON_METEO_NPU_C_MODEL_NAME        "meteo_npu"
#define LL_ATON_METEO_NPU_ORIGIN_MODEL_NAME   "meteo_k3_mlp_int8"

/************************** USER ALLOCATED IOs ********************************/
// No user allocated inputs
// No user allocated outputs

/************************** INPUTS ********************************************/
#define LL_ATON_METEO_NPU_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_0_out_0
#define LL_ATON_METEO_NPU_IN_1_ALIGNMENT   (32)
#define LL_ATON_METEO_NPU_IN_1_SIZE_BYTES  (56)

/************************** OUTPUTS *******************************************/
#define LL_ATON_METEO_NPU_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Dequantize_10_out_0
#define LL_ATON_METEO_NPU_OUT_1_ALIGNMENT   (32)
#define LL_ATON_METEO_NPU_OUT_1_SIZE_BYTES  (12)

#endif /* LL_ATON_METEO_NPU_H */
