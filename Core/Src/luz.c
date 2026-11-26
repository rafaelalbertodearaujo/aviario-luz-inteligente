/* luz.c: PWM da fita (TIM3 CH1, PA6).
 * Parte original: Rafael (FW4). */
#include "luz.h"
#include "tim.h"
#include "config.h"

/* Partida: duty 0 e PWM ligado. */
void luz_init(void)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0u);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

/* Aplica o duty (satura em DUTY_MAX); CCR 1000 com ARR 999 = 100%. */
void luz_set_duty(uint16_t duty)
{
    if (duty > DUTY_MAX) {
        duty = (uint16_t)DUTY_MAX;
    }
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
}
