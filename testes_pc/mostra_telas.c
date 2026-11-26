/* Mostra no PC o que a tela do OLED e o celular exibem, rodando o tela.c,
 * o serial.c e o programa.c REAIS com stubs do HAL. Confere a largura:
 * linha do OLED <= 21 caracteres sem corte, linha do celular <= 42.
 * Compilar com testes_pc/rodar.sh. Nao altera o firmware. */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "app.h"
#include "programa.h"
#include "serial.h"
#include "tela.h"
#include "ssd1306.h"
#include "usart.h"

/* ---- stubs ---- */
UART_HandleTypeDef huart1 = { USART1 };
uint32_t app_ticks(void) { return 0u; }

static char oled[8][128];
static unsigned char pix[64][128];     /* pixels soltos (o rosto) */
static int falhas = 0;

uint8_t ssd1306_init(void) { return 1u; }
void ssd1306_limpa(void) { memset(oled, 0, sizeof oled); memset(pix, 0, sizeof pix); }
void ssd1306_pixel(uint8_t x, uint8_t y) { if (x < 128u && y < 64u) pix[y][x] = 1u; }
void ssd1306_texto(uint8_t col, uint8_t lin, const char *s)
{
    (void)col;
    if (lin < 8u) { snprintf(oled[lin], sizeof oled[lin], "%s", s); }
}
uint8_t ssd1306_envia(void) { return 1u; }
uint8_t ssd1306_envia_pagina(uint8_t p) { (void)p; return 1u; }

static uint8_t *tx_ptr; static uint16_t tx_len;
static uint8_t *rx_ptr;
static int col_cel = 0;
static int sem_limite = 0;      /* linha do painel (P): para máquina, sem limite de 42 */
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n)
{ (void)h; tx_ptr = d; tx_len = n; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n)
{ (void)h; (void)n; rx_ptr = d; return HAL_OK; }

/* esvazia a fila de TX como a interrupcao faria, imprimindo e medindo */
static void drena(void)
{
    while (tx_len) {
        uint16_t k, n = tx_len;
        for (k = 0; k < n; k++) {
            char c = (char)tx_ptr[k];
            if (c == '\r') continue;
            if (c == '\n') { putchar('\n'); col_cel = 0; continue; }
            if (col_cel == 0) printf("  cel | ");
            putchar(c);
            if (++col_cel == 43 && !sem_limite) { printf("   <-- PASSOU DE 42"); falhas++; }
        }
        tx_len = 0;
        HAL_UART_TxCpltCallback(&huart1);
    }
}

static void envia_cmd(char c, const estado_t *e)
{
    printf("  voce> %c\n", c);
    *rx_ptr = (uint8_t)c;
    HAL_UART_RxCpltCallback(&huart1);
    serial_processa(e);
    drena();
}

/* imprime a tela; marca linha que encheu as 21 colunas sem espaco no fim
 * (provavel corte de texto) */
static void mostra_oled(const char *titulo)
{
    int l;
    printf("\n  OLED: %s\n  +---------------------+\n", titulo);
    for (l = 0; l < 8; l++) {
        size_t n = strlen(oled[l]);
        int cheia = (n >= 21 && oled[l][20] != ' ');
        printf("  |%-21.21s|%s\n", oled[l], cheia ? "  (21 colunas: conferir corte)" : "");
        if (n > 21) { printf("   <-- PASSOU DE 21\n"); falhas++; }
    }
    printf("  +---------------------+\n");
}

/* desenha os pixels soltos (o rosto) em ASCII, 2 caracteres por pixel na
 * horizontal para não ficar achatado */
static void mostra_pixels(void)
{
    int x, y, tem = 0;
    for (y = 0; y < 64; y++) for (x = 0; x < 128; x++) tem |= pix[y][x];
    if (!tem) return;
    printf("  pixels (colunas 42 a 86, linhas 14 a 56):\n");
    for (y = 14; y <= 56; y++) {
        printf("  ");
        for (x = 42; x <= 86; x++) printf("%s", pix[y][x] ? "##" : "  ");
        printf("\n");
    }
}

static void resumo(const estado_t *e)
{
    int k;
    for (k = 0; k < (int)RESUMO_S; k++) serial_telemetria(e);
    drena();
}

int main(void)
{
    estado_t e;

    programa_init(351, 1);
    tela_init();
    serial_init(); drena();

    printf("\n== 1. Dia, AUTO, relogio acelerado\n");
    memset(&e, 0, sizeof e);
    e.min_galpao = 351; e.demo = 1; e.modo = MODO_AUTO; e.sp = 60; e.luz = 32; e.ldr_raw = 2150;
    e.duty = 640; e.ina_ok = 1; e.vbus_mV = 4810; e.i_mA = 250; e.p_mW = 1250;
    e.e_real = 650u * 60u; e.e_timer = 1000u * 60u; e.e_fixo = 800u * 60u;
    e.econ_timer = 350; e.econ_sensor = 187; e.entradas = 3;
    tela_atualiza(&e); mostra_oled("dia, AUTO");
    resumo(&e);
    envia_cmd('?', &e);
    envia_cmd('+', &e);
    envia_cmd('S', &e);

    printf("\n== 2. Noite, inspecao do PIR\n");
    e.min_galpao = 130; e.noite = 1; e.sp = 0; e.inspecao = 1; e.insp_s = 45; e.duty = 150;
    e.p_mW = 205; e.i_mA = 43; e.luz = 9; e.ldr_raw = 480;
    serial_aviso_pir(4, 1); drena();
    e.entradas = 4;
    tela_atualiza(&e); mostra_oled("noite, inspecao");
    resumo(&e);
    serial_aviso_fim_inspecao(); drena();
    e.inspecao = 0; e.insp_s = 0; e.duty = 0; e.p_mW = 0; e.i_mA = 0;
    tela_atualiza(&e); mostra_oled("noite, AUTO, sem inspecao");

    printf("\n== 3. MANUAL, relogio real, lanterna (luz 126%%), INA219 falhando, sem energia\n");
    envia_cmd('M', &e);
    envia_cmd('+', &e);
    envia_cmd('V', &e);
    e.min_galpao = 840; e.noite = 0; e.sp = 60; e.demo = 0; e.modo = MODO_MANUAL; e.duty = 700;
    e.luz = 126; e.ldr_raw = 3990; e.ina_ok = 0; e.e_real = 0; e.entradas = 123;
    tela_atualiza(&e); mostra_oled("MANUAL, INA219 falhou");
    resumo(&e);
    envia_cmd('T', &e);
    resumo(&e);
    envia_cmd('T', &e);
    envia_cmd('x', &e);
    envia_cmd('A', &e);

    printf("\n== 4. Economia negativa e potencia alta\n");
    e.modo = MODO_AUTO; e.ina_ok = 1; e.p_mW = 12345; e.e_real = 1200u * 60u; e.econ_timer = -200;
    e.entradas = 7;
    tela_atualiza(&e); mostra_oled("economia -20%");

    printf("\n== 5. Avisos das fases do programa\n");
    serial_aviso_fase(FASE_SUBINDO, (uint16_t)LUZ_INICIO_MIN);
    serial_aviso_fase(FASE_DIA, (uint16_t)(LUZ_INICIO_MIN + RAMPA_MIN));
    serial_aviso_fase(FASE_DESCENDO, (uint16_t)(LUZ_FIM_MIN - RAMPA_MIN));
    serial_aviso_fase(FASE_NOITE, (uint16_t)LUZ_FIM_MIN);
    drena();

    printf("\n== 6. Paginas do OLED pelo celular (1 a 4) e o rosto na troca\n");
    e.min_galpao = 760; e.noite = 0; e.sp = 60; e.luz = 59; e.duty = 196; e.ldr_raw = 1808;
    e.vbus_mV = 4720; e.i_mA = 51; e.p_mW = 241; e.e_real = 650u * 60u; e.econ_timer = 350;
    envia_cmd('2', &e);
    mostra_oled("logo depois do 2: rosto"); mostra_pixels();
    tela_atualiza(&e); mostra_oled("1 s depois: rosto continua");
    tela_atualiza(&e); mostra_oled("2/4 CONTROLE");
    envia_cmd('3', &e); tela_atualiza(&e); tela_atualiza(&e); mostra_oled("3/4 ENERGIA");
    e.ina_ok = 0; tela_atualiza(&e); mostra_oled("3/4 ENERGIA, INA219 falhou"); e.ina_ok = 1;
    envia_cmd('4', &e); tela_atualiza(&e); tela_atualiza(&e); mostra_oled("4/4 COMANDOS");
    envia_cmd('1', &e); tela_atualiza(&e); tela_atualiza(&e); mostra_oled("1/4 RESUMO");
    envia_cmd('5', &e);

    printf("\n== 7. Painel do notebook (P): 1 linha por segundo; T volta ao resumo\n");
    envia_cmd('P', &e);
    sem_limite = 1;
    serial_telemetria(&e); drena();
    e.e_real = 0; e.inspecao = 1; e.insp_s = 30; e.luz = -3; e.i_mA = -2;
    serial_telemetria(&e); drena();
    sem_limite = 0;
    e.e_real = 650u * 60u; e.inspecao = 0; e.luz = 59; e.i_mA = 51;
    envia_cmd('T', &e);
    resumo(&e);

    printf("\nRESUMO: %d linha(s) larga(s) demais\n", falhas);
    return falhas != 0;
}
