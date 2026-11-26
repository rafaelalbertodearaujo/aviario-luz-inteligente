/* stub minimo de adc.h (sem HAL) para compilar ldr.c no PC */
#ifndef ADC_H_STUB
#define ADC_H_STUB
#include <stdint.h>
#include "hal_stub.h"
typedef struct { int dummy; } ADC_HandleTypeDef;
extern ADC_HandleTypeDef hadc1;
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h, uint32_t t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h);
#endif
