/* ldr.c: LDR no ADC1 IN0 (PA0), média de 16 e escala em %.
 * Parte original: Nicolas (FW2). */
#include "ldr.h"
#include "adc.h"
#include "config.h"

static uint16_t raw_atual = 0u;   /* média das 16 amostras, 0..4095 */
static int16_t  pct_atual = 0;    /* luz em %, 0..LUZ_MAX_PCT */

/* Converte raw em % (int32), saturando em 0..LUZ_MAX_PCT. */
static int16_t raw_para_pct(uint16_t raw)
{
    int32_t faixa = (int32_t)RAW_CLARO - (int32_t)RAW_ESCURO;
    int32_t pct;

    if (faixa == 0) {               /* constantes iguais: evita divisão por zero */
        return 0;
    }
    pct = ((int32_t)raw - (int32_t)RAW_ESCURO) * 100 / faixa;
    if (pct < 0) {
        pct = 0;
    }
    if (pct > LUZ_MAX_PCT) {
        pct = LUZ_MAX_PCT;
    }
    return (int16_t)pct;
}

void ldr_init(void)
{
    /* Calibração do ADC na partida (única chamada bloqueante, curta). */
    (void)HAL_ADCEx_Calibration_Start(&hadc1);
}

void ldr_le(void)
{
    uint32_t soma = 0u;
    uint32_t i;

    for (i = 0u; i < LDR_AMOSTRAS; i++) {
        if (HAL_ADC_Start(&hadc1) != HAL_OK) {
            (void)HAL_ADC_Stop(&hadc1);
            return;                 /* falha: mantém o valor anterior */
        }
        if (HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) != HAL_OK) {
            (void)HAL_ADC_Stop(&hadc1);
            return;                 /* timeout: mantém o valor anterior */
        }
        soma += HAL_ADC_GetValue(&hadc1);
    }
    (void)HAL_ADC_Stop(&hadc1);

    raw_atual = (uint16_t)(soma / LDR_AMOSTRAS);
    pct_atual = raw_para_pct(raw_atual);
}

uint16_t ldr_get_raw(void)
{
    return raw_atual;
}

int16_t ldr_get_pct(void)
{
    return pct_atual;
}
