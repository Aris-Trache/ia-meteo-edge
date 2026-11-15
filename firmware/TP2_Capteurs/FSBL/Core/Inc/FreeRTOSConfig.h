/**
  ******************************************************************************
  * @file    FreeRTOSConfig.h
  * @brief   Configuration de FreeRTOS V11.1 pour le STM32N657 (Cortex-M55), TP2.3.
  *
  *  Port utilise : portable/GCC/ARM_CM55_NTZ/non_secure, en mode "secure only" :
  *  le FSBL tourne en monde securise et il n'y a pas de monde non securise.
  *
  *  Choix et pourquoi :
  *   - FPU + MVE (Helium) sauvegardes a chaque changement de tache : le reseau int8
  *     CMSIS-NN utilise les registres vectoriels, il ne faut pas qu'une autre tache
  *     les ecrase au milieu d'une inference.
  *   - Tick a 1 kHz, partage avec le HAL (voir SysTick_Handler dans stm32n6xx_it.c).
  *   - heap_4 : allocation/liberation sans fragmentation excessive (LwIP cree et
  *     detruit des semaphores, sockets...).
  *   - Detection de debordement de pile (methode 2) + hook d'echec de malloc :
  *     le sujet demande de surveiller les "stack overflow".
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;

/* ---- Port ARMv8-M (Cortex-M55) ---- */
#define configENABLE_TRUSTZONE                 0
#define configRUN_FREERTOS_SECURE_ONLY         1
#define configENABLE_MPU                       0
#define configENABLE_FPU                       1
#define configENABLE_MVE                       1
#define configCHECK_HANDLER_INSTALLATION       0   /* handlers renommes ci-dessous */

/* ---- Ordonnanceur ---- */
#define configUSE_PREEMPTION                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ                     ( SystemCoreClock )
#define configTICK_RATE_HZ                     ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                   ( 7 )
#define configMINIMAL_STACK_SIZE               ( ( uint16_t ) 256 )   /* en mots de 32 bits */
#define configMAX_TASK_NAME_LEN                ( 16 )
#define configTICK_TYPE_WIDTH_IN_BITS          TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                1
#define configUSE_TIME_SLICING                 1

/* ---- Memoire ---- */
#define configSUPPORT_STATIC_ALLOCATION        0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                  ( ( size_t ) ( 64 * 1024 ) )
#define configAPPLICATION_ALLOCATED_HEAP       0

/* ---- Synchronisation (LwIP en a besoin) ---- */
#define configUSE_MUTEXES                      1
#define configUSE_RECURSIVE_MUTEXES            1
#define configUSE_COUNTING_SEMAPHORES          1
#define configQUEUE_REGISTRY_SIZE              8
#define configUSE_TASK_NOTIFICATIONS           1

/* ---- Surveillance ---- */
#define configCHECK_FOR_STACK_OVERFLOW         2
#define configUSE_MALLOC_FAILED_HOOK           1
#define configUSE_IDLE_HOOK                    0
#define configUSE_TICK_HOOK                    0
#define configUSE_TRACE_FACILITY               1
#define configUSE_STATS_FORMATTING_FUNCTIONS   0
#define configGENERATE_RUN_TIME_STATS          0
#define configRECORD_STACK_HIGH_ADDRESS        1

/* ---- Timers logiciels ---- */
#define configUSE_TIMERS                       1
#define configTIMER_TASK_PRIORITY              ( 2 )
#define configTIMER_QUEUE_LENGTH               10
#define configTIMER_TASK_STACK_DEPTH           ( configMINIMAL_STACK_SIZE * 2 )

/* ---- Fonctions incluses ---- */
#define INCLUDE_vTaskPrioritySet               1
#define INCLUDE_uxTaskPriorityGet              1
#define INCLUDE_vTaskDelete                    1
#define INCLUDE_vTaskSuspend                   1
#define INCLUDE_vTaskDelayUntil                1
#define INCLUDE_vTaskDelay                     1
#define INCLUDE_xTaskGetSchedulerState         1
#define INCLUDE_xTaskGetCurrentTaskHandle      1
#define INCLUDE_uxTaskGetStackHighWaterMark    1
#define INCLUDE_xSemaphoreGetMutexHolder       1

/* ---- Priorites d'interruption (4 bits de priorite sur le STM32N6) ----
 * Une interruption qui appelle l'API FreeRTOS ("...FromISR") doit avoir une
 * priorite numerique >= 5 (donc MOINS urgente). L'Ethernet est a 7, le SysTick a 15. */
#define configPRIO_BITS                        4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY        ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY   ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* Arret immediat (LED rouge fixe) si une hypothese du noyau est violee */
void vAssertCalled(const char *fichier, int ligne);
#define configASSERT( x )  if ( ( x ) == 0 ) { vAssertCalled( __FILE__, __LINE__ ); }

/* ---- Noms des exceptions ----
 * Le port definit SVC_Handler et PendSV_Handler (assembleur, ils DOIVENT etre
 * les vrais vecteurs : ils ont ete retires de stm32n6xx_it.c et du .ioc).
 * SysTick_Handler, lui, reste dans stm32n6xx_it.c (le HAL a besoin de son tick) :
 * le handler du port est renomme et appele depuis celui de CubeMX. */
#ifndef FREERTOS_GARDER_SYSTICK_HANDLER   /* defini dans stm32n6xx_it.c */
#define SysTick_Handler                        xPortSysTickHandler
#endif
void xPortSysTickHandler(void);

#endif /* FREERTOS_CONFIG_H */
