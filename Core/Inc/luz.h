/* luz.h: PWM da fita (TIM3 CH1, PA6, 1 kHz, ARR 999) -> IRLZ44N.
 * Parte original: Rafael (FW4). */
#ifndef LUZ_H
#define LUZ_H

#include <stdint.h>

/* Partida (app_init): duty 0 e HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1). */
void luz_init(void);

/* Duty em permil, 0..1000 (satura acima de 1000). CCR1 = duty;
 * com ARR 999, CCR 1000 dá 100%. Chamada no bloco de 100 ms (app). */
void luz_set_duty(uint16_t duty);

#endif /* LUZ_H */
