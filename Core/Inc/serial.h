/* serial.h: USART1 (PA9/PA10, 9600 8N1) com o HC-05: comandos, resumo e avisos.
 * Parte original: Victor (FW8). Textos de 19/11: manual em BLUETOOTH_E_TELA.md.
 * Este módulo é o DONO de HAL_UART_RxCpltCallback, HAL_UART_TxCpltCallback
 * e HAL_UART_ErrorCallback.
 * RX: interrupção de 1 byte -> fila circular de 32 bytes; rearma no callback
 * de recepção E no de erro (ORE). TX: fila de 512 bytes enviada por
 * interrupção (não bloqueia; fila cheia descarta o excesso). */
#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include "app.h"

/* Partida (app_init, no fim): HAL_UART_Receive_IT(&huart1, &byte, 1) e uma
 * linha de boas-vindas. */
void serial_init(void);

/* Bloco de 100 ms (app, primeiro de tudo): consome a fila e executa
 * A M + - S V T ? W (maiúscula ou minúscula; ignora \r \n e espaço).
 * Respostas: "OK x: ..." ou "ERRO x: ...", dizendo o que fazer. */
void serial_processa(const estado_t *e);

/* Bloco de 1 s (app): a cada RESUMO_S segundos, um resumo de 7 linhas
 * (se o comando T não o desligou). */
void serial_telemetria(const estado_t *e);

/* Avisos na hora (o app chama quando muda): fase nova do programa (FASE_*,
 * com o minuto atual), entrada no PIR (inspecao = 1 se acendeu a luz de
 * inspeção) e fim da inspeção. Saem mesmo com o resumo desligado. */
void serial_aviso_fase(uint8_t fase, uint16_t min);
void serial_aviso_pir(uint16_t entradas, uint8_t inspecao);
void serial_aviso_fim_inspecao(void);

/* Põe uma string terminada em zero na fila de TX (não bloqueia). */
void serial_envia(const char *s);

modo_t   serial_get_modo(void);         /* AUTO (padrão) ou MANUAL */
uint16_t serial_get_duty_manual(void);  /* permil, 0..1000 */

/* Partida (app_init): restaura modo e duty manual vindos da BKP. */
void serial_restaura(modo_t modo, uint16_t duty_manual);

#endif /* SERIAL_H */
