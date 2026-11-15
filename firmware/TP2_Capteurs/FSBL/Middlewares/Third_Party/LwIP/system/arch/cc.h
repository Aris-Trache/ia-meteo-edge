/**
  * @file    cc.h
  * @brief   Adaptation de LwIP au compilateur arm-none-eabi-gcc + newlib (TP2.3).
  */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>        /* struct timeval fournie par newlib */

#define LWIP_TIMEVAL_PRIVATE   0
#define LWIP_ERRNO_STDINCLUDE  1   /* errno de newlib */
#ifndef BYTE_ORDER
#define BYTE_ORDER             LITTLE_ENDIAN
#endif

/* Traces et assertions LwIP sur la console UART */
#define LWIP_PLATFORM_DIAG(x)    do { printf x; printf("\r"); } while (0)  /* LwIP finit par \n seul */
#define LWIP_PLATFORM_ASSERT(x)  do { printf("LwIP assert: %s (%s:%d)\r\n", x, __FILE__, __LINE__); } while (0)

/* Nombres aleatoires : identifiants DNS/DHCP, ports source */
#define LWIP_RAND()              ((u32_t)rand())

#endif /* LWIP_ARCH_CC_H */
