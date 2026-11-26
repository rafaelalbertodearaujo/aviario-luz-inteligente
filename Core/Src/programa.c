/* programa.c: relógio do galpão e programa de luz.
 * Parte original: Victor (relógio, modo V) e Nicolas (sp_programa). */
#include "programa.h"
#include "app.h"
#include "config.h"

static uint16_t min_galpao = 0u;   /* 0..1439 */
static uint8_t  modo_demo = 0u;    /* 1 = demonstração */
static uint32_t ref_ticks = 0u;    /* tick da última virada de minuto */
static uint8_t  congelado = 0u;    /* 1 = relógio parado (comando F) */

void programa_init(uint16_t min_inicial, uint8_t demo)
{
    min_galpao = (uint16_t)(min_inicial % MIN_POR_DIA);
    modo_demo = (demo != 0u) ? 1u : 0u;
    ref_ticks = app_ticks();
}

uint8_t programa_atualiza(void)
{
    uint16_t tpm = ticks_por_minuto();

    if (congelado != 0u) {          /* parado: não acumula tempo */
        ref_ticks = app_ticks();
        return 0u;
    }

    /* Diferença em uint32: correta mesmo se app_ticks() der a volta. */
    if ((uint32_t)(app_ticks() - ref_ticks) >= tpm) {
        ref_ticks += tpm;
        min_galpao = (uint16_t)((min_galpao + 1u) % MIN_POR_DIA);
        return 1u;
    }
    return 0u;
}

uint16_t programa_get_min(void)
{
    return min_galpao;
}

uint16_t ticks_por_minuto(void)
{
    if (modo_demo == 0u) {
        return TPM_REAL;
    }
    return programa_em_rampa(min_galpao) ? TPM_RAMPA : TPM_DEMO;
}

uint8_t programa_alterna_congela(void)
{
    congelado = (congelado != 0u) ? 0u : 1u;
    ref_ticks = app_ticks();       /* retoma sem saltar minutos */
    return congelado;
}

uint8_t programa_get_congelado(void)
{
    return congelado;
}

uint8_t programa_get_demo(void)
{
    return modo_demo;
}

void programa_alterna_demo(void)
{
    modo_demo = (modo_demo != 0u) ? 0u : 1u;
    ref_ticks = app_ticks();       /* sem saltar minutos */
}

uint8_t sp_programa(uint16_t min)
{
    uint32_t k;

    if (min < LUZ_INICIO_MIN || min >= LUZ_FIM_MIN) {
        return 0u;                              /* noite */
    }
    k = (uint32_t)min - LUZ_INICIO_MIN;
    if (k < RAMPA_MIN) {                        /* rampa de subida */
        return (uint8_t)((SP_DIA * (k + 1u)) / RAMPA_MIN);
    }
    k = (uint32_t)LUZ_FIM_MIN - min;            /* 1..RAMPA_MIN na descida */
    if (k <= RAMPA_MIN) {                       /* rampa de descida */
        return (uint8_t)((SP_DIA * k) / RAMPA_MIN);
    }
    return (uint8_t)SP_DIA;
}

uint8_t programa_em_rampa(uint16_t min)
{
    uint8_t f = programa_fase(min);
    return (f == FASE_SUBINDO || f == FASE_DESCENDO) ? 1u : 0u;
}

uint8_t programa_fase(uint16_t min)
{
    if (min < LUZ_INICIO_MIN || min >= LUZ_FIM_MIN) {
        return FASE_NOITE;
    }
    if (((uint32_t)min - LUZ_INICIO_MIN) < RAMPA_MIN) {
        return FASE_SUBINDO;
    }
    if (((uint32_t)LUZ_FIM_MIN - min) <= RAMPA_MIN) {
        return FASE_DESCENDO;
    }
    return FASE_DIA;
}

uint16_t programa_proxima_virada(uint16_t min)
{
    switch (programa_fase(min)) {
    case FASE_SUBINDO:  return (uint16_t)(LUZ_INICIO_MIN + RAMPA_MIN);
    case FASE_DIA:      return (uint16_t)(LUZ_FIM_MIN - RAMPA_MIN);
    case FASE_DESCENDO: return (uint16_t)LUZ_FIM_MIN;
    default:            return (uint16_t)LUZ_INICIO_MIN;
    }
}

const char *programa_texto_proximo(uint8_t fase)
{
    switch (fase) {
    case FASE_SUBINDO:  return "Sobe ate";
    case FASE_DIA:      return "Escurece as";
    case FASE_DESCENDO: return "Apaga as";
    default:            return "Acende as";
    }
}
