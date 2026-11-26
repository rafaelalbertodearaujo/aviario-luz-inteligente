/* tela.h: tela simples do OLED (título na faixa amarela, números embaixo).
 * Parte original: Nicolas (FW7). Usa só ssd1306.h. */
#ifndef TELA_H
#define TELA_H

#include "app.h"

/* Partida (app_init): ssd1306_init() e uma tela "AVIARIO LUZ / iniciando". */
void tela_init(void);

/* Bloco de 1 s (app): redesenha o buffer a partir do retrato (não envia;
 * marca as 8 páginas para envio). */
void tela_atualiza(const estado_t *e);

/* Comandos 1 a 4 do celular (serial.c): escolhe a página e mostra o rosto
 * feliz por ~1 s. Devolve o nome da página, ou 0 se n estiver fora de 1..4. */
const char *tela_pagina(uint8_t n);

/* Bloco de 100 ms (app, no fim): envia a próxima página pendente, uma por
 * chamada (~12 ms de I2C). Em falha desiste até o próximo redesenho. */
void tela_envia_pagina(void);

#endif /* TELA_H */
