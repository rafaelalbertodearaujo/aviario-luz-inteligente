/* tela.c: tela do OLED (8 linhas de 21 caracteres), redesenhada a cada 1 s.
 * Parte original: Nicolas (FW7). Textos de 19/11 e páginas de 20/11 (Rafael).
 * Linhas 0 e 1 (faixa amarela), iguais em todas as páginas: hora, dia/noite,
 * número da página, DEMO; o modo e o que ele faz.
 * Linhas 2..7: a página escolhida pelo celular (comandos 1 a 4):
 *   1 Resumo (alvo x medida, lâmpada, economia, PIR, próximo passo, LDR),
 *   2 Controle, 3 Energia, 4 Comandos.
 * Na troca aparece um rosto feliz por ~1 s. Fonte só ASCII (sem acentos);
 * só formatação inteira. Manual: BLUETOOTH_E_TELA.md, seção 2. */
#include "tela.h"
#include "ssd1306.h"
#include "programa.h"
#include "config.h"

#define TELA_COLS   21u
#define TELA_PAGS   8u      /* as 8 páginas do OLED (8 linhas) */
#define N_TELAS     4u      /* páginas que o celular escolhe (1..4) */

static uint8_t oled_ok;     /* 0 = OLED ausente: o firmware segue sem tela */
static uint8_t pag_prox = TELA_PAGS;    /* próxima página a enviar; TELA_PAGS = nada */
static uint8_t tela = 1u;   /* página escolhida, 1..N_TELAS */
static uint8_t rosto;       /* redesenhos a pular: o rosto fica na tela */

static const char *const nomes[N_TELAS] = { "RESUMO", "CONTROLE", "ENERGIA", "COMANDOS" };

/* Escreve texto em l a partir de i; devolve o novo índice. */
static uint8_t put_s(char *l, uint8_t i, const char *s)
{
    while (*s != '\0' && i < TELA_COLS) {
        l[i++] = *s++;
    }
    return i;
}

/* Escreve v em decimal com 'larg' dígitos, preenchendo com 'pad'. */
static uint8_t put_n(char *l, uint8_t i, uint32_t v, uint8_t larg, char pad)
{
    char t[10];
    uint8_t n = 0u, k;

    do {
        t[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u && n < sizeof(t));
    for (k = n; k < larg && i < TELA_COLS; k++) {
        l[i++] = pad;
    }
    while (n > 0u && i < TELA_COLS) {
        l[i++] = t[--n];
    }
    return i;
}

/* Inteiro com sinal: "-12", "+3" ou "0". */
static uint8_t put_sinal(char *l, uint8_t i, int32_t v)
{
    if (v < 0) {
        i = put_s(l, i, "-");
        v = -v;
    } else if (v > 0) {
        i = put_s(l, i, "+");
    }
    return put_n(l, i, (uint32_t)v, 1u, ' ');
}

/* Percentual com sinal (permil / 10), limitado a -99..99. */
static uint8_t put_pct(char *l, uint8_t i, int16_t permil)
{
    int16_t v = (int16_t)(permil / 10);

    if (v < 0) {
        v = (int16_t)(-v);
        if (v > 99) { v = 99; }
        i = put_s(l, i, "-");
    } else if (v > 99) {
        v = 99;
    }
    i = put_n(l, i, (uint32_t)v, 1u, ' ');
    return put_s(l, i, "%");
}

/* Permil como "19,6" (uma casa). */
static uint8_t put_dec(char *l, uint8_t i, uint32_t permil)
{
    i = put_n(l, i, permil / 10u, 1u, ' ');
    i = put_s(l, i, ",");
    return put_n(l, i, permil % 10u, 1u, ' ');
}

/* mW como "1,25 W". */
static uint8_t put_w(char *l, uint8_t i, uint32_t mw)
{
    if (mw > 99999u) {
        mw = 99999u;
    }
    i = put_n(l, i, mw / 1000u, 1u, ' ');
    i = put_s(l, i, ",");
    i = put_n(l, i, (mw % 1000u) / 10u, 2u, '0');
    return put_s(l, i, " W");
}

/* Linha vazia (espaços) terminada em '\0'. */
static void limpa_linha(char *l)
{
    uint8_t k;

    for (k = 0u; k < TELA_COLS; k++) {
        l[k] = ' ';
    }
    l[TELA_COLS] = '\0';
}

/* ---------- rosto feliz (troca de página) ---------- */

/* Círculo de raio 18 em (64, 35), olhos e sorriso, só com inteiros. */
static void desenha_rosto(void)
{
    int16_t x, y;
    int32_t d2;

    for (y = -19; y <= 19; y++) {
        for (x = -19; x <= 19; x++) {
            d2 = (int32_t)x * x + (int32_t)y * y;
            if (d2 >= 17 * 17 && d2 <= 18 * 18 + 6) {                 /* contorno */
                ssd1306_pixel((uint8_t)(64 + x), (uint8_t)(35 + y));
            }
            if ((x >= -8 && x <= -5 && y >= -8 && y <= -4) ||          /* olhos */
                (x >= 5 && x <= 8 && y >= -8 && y <= -4)) {
                ssd1306_pixel((uint8_t)(64 + x), (uint8_t)(35 + y));
            }
            if (y >= 3 && d2 >= 9 * 9 && d2 <= 11 * 11) {              /* sorriso */
                ssd1306_pixel((uint8_t)(64 + x), (uint8_t)(35 + y));
            }
        }
    }
}

const char *tela_pagina(uint8_t n)
{
    char l[TELA_COLS + 1u];
    uint8_t i;

    if (n < 1u || n > N_TELAS) {
        return 0;
    }
    tela = n;
    if (!oled_ok) {
        return nomes[n - 1u];
    }
    ssd1306_limpa();
    ssd1306_texto(0u, 0u, "AVIARIO LUZ");
    desenha_rosto();
    limpa_linha(l);
    i = put_n(l, 4u, n, 1u, ' ');
    i = put_s(l, i, "/4  ");
    (void)put_s(l, i, nomes[n - 1u]);
    ssd1306_texto(0u, 7u, l);
    rosto = 1u;         /* o próximo redesenho mantém o rosto */
    pag_prox = 0u;
    return nomes[n - 1u];
}

void tela_init(void)
{
    oled_ok = ssd1306_init();
    if (!oled_ok) {
        return;
    }
    ssd1306_limpa();
    ssd1306_texto(0u, 0u, "AVIARIO LUZ");
    ssd1306_texto(0u, 2u, "iniciando");
    (void)ssd1306_envia();
}

/* ---------- páginas (linhas 2..7) ---------- */

/* 1: resumo (a tela de 19/11) */
static void pag_resumo(const estado_t *e, char *l)
{
    uint8_t i;
    uint16_t px;

    /* 2: luz que o programa quer x luz que o LDR mede (pode passar de 100%:
     * BLUETOOTH_E_TELA.md, seção 5) */
    limpa_linha(l);
    i = put_s(l, 0u, "Alvo ");
    i = put_n(l, i, e->sp, 2u, ' ');
    (void)put_s(l, i, "%");
    i = put_s(l, 10u, "Medida ");
    i = put_n(l, i, (uint32_t)(e->luz < 0 ? 0 : e->luz), 1u, ' ');
    (void)put_s(l, i, "%");
    ssd1306_texto(0u, 2u, l);

    /* 3: quanto a fita está acesa (duty) e a potência que o INA219 mede */
    limpa_linha(l);
    i = put_s(l, 0u, "Lampada ");
    i = put_n(l, i, (uint32_t)(e->duty / 10u), 1u, ' ');
    (void)put_s(l, i, "%");
    if (e->ina_ok) {
        uint32_t p = (e->p_mW > 99999u) ? 99999u : e->p_mW;
        i = put_n(l, 13u, p / 1000u, 2u, ' ');
        i = put_s(l, i, ",");
        i = put_n(l, i, (p % 1000u) / 10u, 2u, '0');
        (void)put_s(l, i, " W");
    } else {
        (void)put_s(l, 13u, "sem INA");
    }
    ssd1306_texto(0u, 3u, l);

    /* 4: economia contra um timer comum ("--" sem energia medida) */
    limpa_linha(l);
    i = put_s(l, 0u, (e->econ_timer <= -100) ? "Econ. " : "Economia ");
    if (e->e_real == 0u || e->e_timer == 0u) {
        i = put_s(l, i, "--");
    } else {
        i = put_pct(l, i, e->econ_timer);
    }
    (void)put_s(l, i, " vs timer");
    ssd1306_texto(0u, 4u, l);

    /* 5: entradas na antessala hoje (PIR) */
    limpa_linha(l);
    i = put_s(l, 0u, (e->entradas > 99u) ? "Entradas PIR: " : "Entradas PIR hoje: ");
    (void)put_n(l, i, e->entradas, 1u, ' ');
    ssd1306_texto(0u, 5u, l);

    /* 6: o próximo passo do programa de luz */
    limpa_linha(l);
    px = programa_proxima_virada(e->min_galpao);
    i = put_s(l, 0u, programa_texto_proximo(programa_fase(e->min_galpao)));
    i = put_s(l, i, " ");
    i = put_n(l, i, (uint32_t)(px / 60u), 2u, '0');
    i = put_s(l, i, ":");
    (void)put_n(l, i, (uint32_t)(px % 60u), 2u, '0');
    ssd1306_texto(0u, 6u, l);

    /* 25/11 (pedido do Rafael): a linha 7 (LDR bruto e "BT: mande ?") saiu da tela 1 para a apresentação ficar
     * só com o essencial; o LDR bruto está na tela 2 e os comandos na tela 4 */
}

/* 2: controle (o que o laço faz e com que números) */
static void pag_controle(const estado_t *e, char *l)
{
    uint8_t i;

    limpa_linha(l);
    (void)put_s(l, 0u, "CONTROLE DA LUZ");
    ssd1306_texto(0u, 2u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "Alvo ");
    i = put_n(l, i, e->sp, 2u, ' ');
    (void)put_s(l, i, "%");
    i = put_s(l, 10u, "Medida ");
    i = put_n(l, i, (uint32_t)(e->luz < 0 ? 0 : e->luz), 1u, ' ');
    (void)put_s(l, i, "%");
    ssd1306_texto(0u, 3u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "Erro ");
    i = put_sinal(l, i, (e->sp == 0u) ? 0 : (int32_t)e->sp - (int32_t)e->luz);
    (void)put_s(l, i, " pontos");
    ssd1306_texto(0u, 4u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "Lampada ");
    i = put_dec(l, i, e->duty);
    (void)put_s(l, i, "%");
    ssd1306_texto(0u, 5u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "KP ");
    i = put_n(l, i, (uint32_t)KP, 1u, ' ');
    i = put_s(l, i, "  KI ");
    i = put_n(l, i, (uint32_t)KI, 1u, ' ');
    i = put_s(l, i, "  Dfix ");
    i = put_n(l, i, (uint32_t)D_FIXO / 10u, 1u, ' ');
    (void)put_s(l, i, "%");
    ssd1306_texto(0u, 6u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "LDR ");
    i = put_n(l, i, e->ldr_raw, 4u, ' ');
    i = put_s(l, i, " (");
    i = put_n(l, i, (uint32_t)RAW_ESCURO, 1u, ' ');
    i = put_s(l, i, "-");
    i = put_n(l, i, (uint32_t)RAW_CLARO, 1u, ' ');
    (void)put_s(l, i, ")");
    ssd1306_texto(0u, 7u, l);
}

/* 3: energia (INA219 e a comparação com o timer) */
static void pag_energia(const estado_t *e, char *l)
{
    uint8_t i;

    limpa_linha(l);
    (void)put_s(l, 0u, "ENERGIA DA FITA");
    ssd1306_texto(0u, 2u, l);

    limpa_linha(l);
    if (e->ina_ok) {
        i = put_s(l, 0u, "Tensao   ");
        i = put_n(l, i, e->vbus_mV / 1000u, 1u, ' ');
        i = put_s(l, i, ",");
        i = put_n(l, i, (e->vbus_mV % 1000u) / 10u, 2u, '0');
        (void)put_s(l, i, " V");
    } else {
        (void)put_s(l, 0u, "INA219 sem leitura");
    }
    ssd1306_texto(0u, 3u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "Corrente ");
    i = put_n(l, i, (uint32_t)(e->i_mA < 0 ? 0 : e->i_mA), 1u, ' ');
    (void)put_s(l, i, " mA");
    ssd1306_texto(0u, 4u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "Potencia ");
    (void)put_w(l, i, e->p_mW);
    ssd1306_texto(0u, 5u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "Gasto hoje ");
    i = put_n(l, i, e->e_real / 60u, 1u, ' ');
    (void)put_s(l, i, " mWh");
    ssd1306_texto(0u, 6u, l);

    limpa_linha(l);
    i = put_s(l, 0u, "Economia ");
    if (e->e_real == 0u || e->e_timer == 0u) {
        i = put_s(l, i, "--");
    } else {
        i = put_pct(l, i, e->econ_timer);
    }
    (void)put_s(l, i, " vs timer");
    ssd1306_texto(0u, 7u, l);
}

/* 4: comandos do celular */
static void pag_comandos(char *l)
{
    static const char *const linhas[6] = {
        "COMANDOS NO CELULAR",
        "A auto     M manual",
        "+ - lampada 10%",
        "V relogio  S numeros",
        "T resumo   ? ajuda",
        "1 2 3 4  troca tela",
    };
    uint8_t k;

    for (k = 0u; k < 6u; k++) {
        limpa_linha(l);
        (void)put_s(l, 0u, linhas[k]);
        ssd1306_texto(0u, (uint8_t)(2u + k), l);
    }
}

void tela_atualiza(const estado_t *e)
{
    char l[TELA_COLS + 1u];
    uint8_t i;

    if (!oled_ok || e == 0) {
        return;
    }
    if (rosto != 0u) {      /* o rosto da troca de página fica mais 1 s */
        rosto--;
        return;
    }
    ssd1306_limpa();

    /* 0 (amarela): HH:MM  DIA|NOITE  n/4  DEMO */
    limpa_linha(l);
    i = put_n(l, 0u, (uint32_t)(e->min_galpao / 60u), 2u, '0');
    i = put_s(l, i, ":");
    (void)put_n(l, i, (uint32_t)(e->min_galpao % 60u), 2u, '0');
    (void)put_s(l, 7u, e->noite ? "NOITE" : "DIA");
    i = put_n(l, 13u, tela, 1u, ' ');
    (void)put_s(l, i, "/4");
    if (e->demo) {
        (void)put_s(l, 17u, "DEMO");
    }
    ssd1306_texto(0u, 0u, l);

    /* 1 (amarela): o modo e o que ele faz agora */
    limpa_linha(l);
    if (e->inspecao) {
        i = put_s(l, 0u, "INSPECAO PIR: ");
        i = put_n(l, i, e->insp_s, 1u, ' ');
        (void)put_s(l, i, " s");
    } else if (e->modo == MODO_MANUAL) {
        (void)put_s(l, 0u, "MANUAL: +/- no cel");
    } else if (e->noite) {
        (void)put_s(l, 0u, "NOITE: fita apagada");
    } else {
        (void)put_s(l, 0u, "AUTO: segue programa");
    }
    ssd1306_texto(0u, 1u, l);

    switch (tela) {
    case 2u:  pag_controle(e, l); break;
    case 3u:  pag_energia(e, l);  break;
    case 4u:  pag_comandos(l);    break;
    default:  pag_resumo(e, l);   break;
    }

    pag_prox = 0u;      /* uma página por bloco de 100 ms (8 x ~12 ms) */
}

void tela_envia_pagina(void)
{
    if (!oled_ok || pag_prox >= TELA_PAGS) {
        return;
    }
    if (ssd1306_envia_pagina(pag_prox)) {
        pag_prox++;
    } else {
        pag_prox = TELA_PAGS;               /* falha: desiste; redesenha em 1 s */
    }
}
