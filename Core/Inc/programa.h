/* programa.h: relógio do galpão e programa de luz.
 * Parte original: Victor (relógio, modo V) e Nicolas (sp_programa).
 * Rampas: em [LUZ_INICIO_MIN, LUZ_INICIO_MIN + RAMPA_MIN) e
 * [LUZ_FIM_MIN - RAMPA_MIN, LUZ_FIM_MIN). */
#ifndef PROGRAMA_H
#define PROGRAMA_H

#include <stdint.h>

/* Fases do programa de luz (programa_fase) */
#define FASE_NOITE      0u  /* fora de [INICIO, FIM): fita apagada */
#define FASE_SUBINDO    1u  /* rampa de subida (amanhecer) */
#define FASE_DIA        2u  /* setpoint SP_DIA */
#define FASE_DESCENDO   3u  /* rampa de descida (anoitecer) */

/* Partida (app_init): hora inicial (0..1439, da BKP ou HORA_INICIAL_MIN)
 * e modo (1 = demonstração). Guarda app_ticks() como referência. */
void programa_init(uint16_t min_inicial, uint8_t demo);

/* Chamada a cada volta do laço (app). Se passaram ticks_por_minuto()
 * ticks desde a última virada, avança UM minuto (1439 -> 0), soma
 * ticks_por_minuto() à referência e devolve 1; senão 0.
 * O app chama em while () para não perder minutos. */
uint8_t programa_atualiza(void);

/* Minuto atual do galpão, 0..1439. */
uint16_t programa_get_min(void);

/* 6000 no modo real; no demo, 17 fora das rampas e 100 nas rampas. */
uint16_t ticks_por_minuto(void);

/* 1 = demonstração, 0 = real. */
uint8_t programa_get_demo(void);

/* Comando V (serial): troca real <-> demonstração, sem saltar minutos
 * (a referência passa a ser app_ticks()). */
void programa_alterna_demo(void);
/* Congela/retoma o relógio do galpão (comando F); devolve 1 se ficou congelado. */
uint8_t programa_alterna_congela(void);
uint8_t programa_get_congelado(void);

/* Setpoint do programa no minuto min (0..1439), em % (função pura):
 *   fora de [INICIO, FIM)              -> 0 (noite)
 *   rampa de subida (k = min - INICIO) -> SP_DIA * (k + 1) / RAMPA_MIN
 *   rampa de descida (k = FIM - min)   -> SP_DIA * k / RAMPA_MIN
 *   resto do dia                        -> SP_DIA
 * Dentro de [INICIO, FIM) o resultado é sempre > 0. */
uint8_t sp_programa(uint16_t min);

/* 1 se min está numa das duas rampas. */
uint8_t programa_em_rampa(uint16_t min);

/* Fase do programa no minuto min (FASE_*). */
uint8_t programa_fase(uint16_t min);

/* Minuto (0..1439) em que a fase atual termina (próxima virada). */
uint16_t programa_proxima_virada(uint16_t min);

/* O que acontece na próxima virada, para a tela e o celular (ASCII, sem
 * acento): "Acende as", "Sobe ate", "Escurece as" ou "Apaga as". */
const char *programa_texto_proximo(uint8_t fase);

#endif /* PROGRAMA_H */
