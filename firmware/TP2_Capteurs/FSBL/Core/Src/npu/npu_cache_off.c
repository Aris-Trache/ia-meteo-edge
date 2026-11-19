/**
  * @file    npu_cache_off.c
  * @brief   Cache du NPU (CACHEAXI) non utilise : il ne sert que pour les memoires
  *          externes. Notre reseau est en AXISRAM2 interne, donc ces fonctions
  *          appelees par le runtime ST ne font rien.
  * @author  Aris Trache - ETRS923, USMB Master ESET
  */
#include "npu_cache.h"

void npu_cache_init(void) {}
void npu_cache_enable(void) {}
void npu_cache_disable(void) {}
void npu_cache_invalidate(void) {}
void npu_cache_clean_range(uint32_t start_addr, uint32_t end_addr) { (void)start_addr; (void)end_addr; }
void npu_cache_clean_invalidate_range(uint32_t start_addr, uint32_t end_addr) { (void)start_addr; (void)end_addr; }
