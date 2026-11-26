/* pir.h: PIR HC-SR501 no PA1 (EXTI1, borda de subida, pull-down).
 * Parte original: Victor (FW3).
 * Este módulo é o DONO de HAL_GPIO_EXTI_Callback (só marca a borda).
 * Tempos contados em chamadas de 100 ms (PIR_*_MS / 100). */
#ifndef PIR_H
#define PIR_H

#include <stdint.h>

/* Partida (app_init): zera contagem, flags e temporizadores. */
void pir_init(void);

/* Bloco de 100 ms (app, depois do PWM). noite = 1 só se for noite E modo AUTO
 * (arma a inspeção; a contagem não depende dele).
 * Acionamento = borda vinda da EXTI OU nível alto no pino depois de nível
 * baixo na chamada anterior (borda perdida). Regras:
 *  - nos primeiros PIR_AQUECIMENTO_MS após ligar: descarta;
 *  - trava de PIR_TRAVA_MS entre acionamentos;
 *  - acionamento válido: entradas++; se noite, liga (ou reinicia) a
 *    inspeção por INSPECAO_MS; de dia, só conta. */
void pir_atualiza(uint8_t noite);

/* 1 enquanto a inspeção de 60 s estiver correndo. */
uint8_t pir_inspecao_ativa(void);

/* Segundos que faltam da inspeção (0 se parada), arredondado para cima. */
uint8_t pir_inspecao_restante_s(void);

/* Acionamentos válidos no dia. */
uint16_t pir_get_contagem(void);

/* App chama quando o relógio do galpão vira para 00:00. */
void pir_zera_contagem(void);

#endif /* PIR_H */
