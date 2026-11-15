/**
  * @file    cmsis_nn_build.c
  * @brief   Compile les noyaux CMSIS-NN utilises par le reseau meteo int8, en -O3.
  *
  *  Les sources d'ARM sont gardees intactes, avec l'extension .c.inc
  *  (STM32CubeIDE ne les compile donc pas seules) et incluses ici. Le #pragma
  *  applique -O3 a ces noyaux meme si le projet est en Debug (-O0), comme pour
  *  meteo_mlp_ref.c : la comparaison float / int8 est ainsi equitable.
  *
  *  Origine : CMSIS-NN fourni par le pack X-CUBE-AI 10.2.1 (Middlewares/tensorflow/
  *  third_party/cmsis_nn), licence Apache-2.0 (LICENSE_CMSIS-NN.txt).
  */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3")
#endif

#include "arm_fully_connected_per_channel_s8.c.inc"
#include "arm_fully_connected_get_buffer_sizes_s8.c.inc"
#include "arm_vector_sum_s8.c.inc"
#include "arm_nn_vec_mat_mult_t_per_ch_s8.c.inc"
#include "arm_nntables.c.inc"
#include "arm_softmax_s8.c.inc"
#include "arm_nn_softmax_common_s8.c.inc"
