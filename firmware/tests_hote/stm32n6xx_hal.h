/* HAL minimal pour compiler capteurs.c sur PC (tests hote uniquement) */
#ifndef STUB_HAL_H
#define STUB_HAL_H
#include <stdint.h>
typedef struct { int x; } I2C_HandleTypeDef;
typedef enum { HAL_OK=0, HAL_ERROR } HAL_StatusTypeDef;
#define I2C_MEMADD_SIZE_8BIT 1U
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef*,uint16_t,uint16_t,uint16_t,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef*,uint16_t,uint16_t,uint16_t,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef*,uint16_t,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef*,uint16_t,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef*,uint16_t,uint32_t,uint32_t);
void HAL_Delay(uint32_t); uint32_t HAL_GetTick(void);
#endif
