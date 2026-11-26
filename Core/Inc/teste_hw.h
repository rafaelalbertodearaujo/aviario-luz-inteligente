/* teste_hw.h: testes de bancada, um componente por vez.
 * Escolha em config.h: TESTE_HW = 0 (firmware normal) ou um número abaixo.
 * O resultado fica em RAM na variável teste_res, lida pelo ST-Link
 * (testes_hw/bancada.ps1) ou no Live Expressions do CubeIDE.
 * PC13: aceso = rodando; pisca lento = OK; pisca rápido = FALHA. */
#ifndef TESTE_HW_H
#define TESTE_HW_H

#include <stdint.h>

#define TESTE_CHIP    1u  /* passos 6-8: Blue Pill + ST-Link, ID e Flash */
#define TESTE_LDR     2u  /* passo 9: LDR no PA0 (ADC) */
#define TESTE_PIR     3u  /* passo 10: PIR no PA1 */
#define TESTE_HC05    4u  /* passo 11: HC-05 em PA9/PA10, PING e eco */
#define TESTE_OLED    5u  /* passo 12: OLED 0x3C */
#define TESTE_INA219  6u  /* passo 13: INA219 0x40, parte de dados */
#define TESTE_I2C     7u  /* varredura do barramento I2C */
#define TESTE_PWM     8u  /* fase 2: rampa no PA6 (LED de teste), com INA219 se houver */
#define TESTE_CALIB   9u  /* calibração: degraus de 10% na fita, média do LDR em cada um */

#define TESTE_MAGIC   0x7E57B00Cu

#define TESTE_RODANDO 0u
#define TESTE_OK      1u
#define TESTE_FALHA   2u

/* 12 palavras de 32 bits, nesta ordem (o script lê assim). */
typedef struct {
    uint32_t magic;     /* TESTE_MAGIC quando o teste está rodando */
    uint32_t id;        /* número do teste */
    uint32_t estado;    /* TESTE_RODANDO, TESTE_OK ou TESTE_FALHA */
    uint32_t ciclos;    /* voltas do laço (prova que não travou) */
    int32_t  v[8];      /* valores do teste (significado em teste_hw.c) */
} teste_res_t;

extern volatile teste_res_t teste_res;

/* Roda o teste escolhido para sempre (não volta). */
void teste_hw_executa(void);

#endif /* TESTE_HW_H */
