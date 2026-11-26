/* ssd1306.h: driver mínimo próprio do OLED 0,96" 128x64 (I2C1, 0x3C).
 * Parte original: Nicolas (FW7).
 * Buffer de 1024 bytes em RAM (8 páginas x 128 colunas); fonte 6x8 ASCII
 * (0x20..0x7E) embutida no .c: 21 caracteres por linha, 8 linhas.
 * Envio: comando 0x00 / dados 0x40, timeout OLED_TIMEOUT_MS por página;
 * em erro chama i2c1_recupera() (ina219.h) e devolve 0. */
#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

/* Partida: sequência de init (pode usar HAL_Delay só aqui), limpa a tela.
 * Devolve 1 se o OLED respondeu. */
uint8_t ssd1306_init(void);

/* Zera o buffer (não envia). */
void ssd1306_limpa(void);

/* Escreve texto no buffer. col em pixels (0..127), lin = página (0..7).
 * Corta no fim da linha; caractere fora da fonte vira '?'. */
void ssd1306_texto(uint8_t col, uint8_t lin, const char *s);

/* Acende um pixel no buffer (x 0..127, y 0..63; fora disso ignora). */
void ssd1306_pixel(uint8_t x, uint8_t y);

/* Envia o buffer inteiro (8 páginas, ~0,1 s a 100 kHz). 1 = ok. */
uint8_t ssd1306_envia(void);

/* Envia uma página (0..7): plano B se a tela atrasar o controle (05 §6.5). */
uint8_t ssd1306_envia_pagina(uint8_t pagina);

#endif /* SSD1306_H */
