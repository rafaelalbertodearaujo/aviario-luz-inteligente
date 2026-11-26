/* ldr.h: luz medida pelo LDR (ADC1 IN0, PA0, conversão única por software).
 * Parte original: Nicolas (FW2).
 * Escala: luz = (raw - RAW_ESCURO) * 100 / (RAW_CLARO - RAW_ESCURO), em int32,
 * saturada em 0..LUZ_MAX_PCT. A luz sobe com o raw (T1). */
#ifndef LDR_H
#define LDR_H

#include <stdint.h>

/* Partida (app_init): HAL_ADCEx_Calibration_Start(&hadc1). */
void ldr_init(void);

/* Bloco de 100 ms (app, primeiro): 16 conversões (Start + PollForConversion
 * com ADC_TIMEOUT_MS), média em uint32. Em timeout mantém o valor anterior. */
void ldr_le(void);

/* Média das 16 amostras, 0..4095 (para a calibração e o comando S). */
uint16_t ldr_get_raw(void);

/* Luz em %, 0..LUZ_MAX_PCT (100 = fita a 100% com a maquete fechada). */
int16_t ldr_get_pct(void);

#endif /* LDR_H */
