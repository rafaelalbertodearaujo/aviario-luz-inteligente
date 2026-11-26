/* Teste no PC do codigo C real: programa.c, controle.c e ldr.c (raw_para_pct).
 * Compilar com testes_pc/rodar.sh. Nao altera o firmware. */
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "app.h"
#include "programa.h"
#include "controle.h"
#include "ldr.h"
/* inclui o ldr.c real para alcancar raw_para_pct (static) e ldr_le */
#include "../Core/Src/ldr.c"

/* ---- stubs ---- */
ADC_HandleTypeDef hadc1;
static uint32_t g_ticks = 0u;
static uint32_t adc_val = 0u;
uint32_t app_ticks(void) { return g_ticks; }
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h, uint32_t t) { (void)h; (void)t; return HAL_OK; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h) { (void)h; return adc_val; }

static int n_ok = 0, n_fail = 0;
static void chk(const char *nome, long obtido, long esperado)
{
    int ok = (obtido == esperado);
    printf("  [%s] %-52s obtido=%ld esperado=%ld\n", ok ? "PASSOU" : "FALHOU", nome, obtido, esperado);
    if (ok) n_ok++; else n_fail++;
}
static void chk_cond(const char *nome, int cond, const char *detalhe)
{
    printf("  [%s] %-52s %s\n", cond ? "PASSOU" : "FALHOU", nome, detalhe);
    if (cond) n_ok++; else n_fail++;
}

static void t_programa(void)
{
    int m, noite = 0, viol = 0, rampa_ok = 1;
    char buf[96];
    printf("== sp_programa (INICIO=%u FIM=%u RAMPA=%u SP_DIA=%u)\n", LUZ_INICIO_MIN, LUZ_FIM_MIN, RAMPA_MIN, SP_DIA);
    chk("04:59 (299) noite", sp_programa(299), 0);
    chk("05:00 (300) k=0 -> 60*1/30", sp_programa(300), 2);
    chk("05:29 (329) k=29 -> 60*30/30", sp_programa(329), 60);
    chk("05:30 (330) plateau", sp_programa(330), 60);
    chk("22:29 (1349) k=31 plateau", sp_programa(1349), 60);
    chk("22:30 (1350) k=30 -> 60*30/30", sp_programa(1350), 60);
    chk("22:59 (1379) k=1 -> 60*1/30", sp_programa(1379), 2);
    chk("23:00 (1380) noite", sp_programa(1380), 0);
    chk("00:00 e 23:59 (noite)", sp_programa(0) + sp_programa(1439), 0);
    for (m = 0; m < 1440; m++) {
        int s = sp_programa((uint16_t)m);
        if (m >= (int)LUZ_INICIO_MIN && m < (int)LUZ_FIM_MIN) { if (s <= 0 || s > (int)SP_DIA) viol++; }
        else if (s != 0) viol++;
        if (s == 0) noite++;
        if (programa_em_rampa((uint16_t)m) != ((m >= (int)LUZ_INICIO_MIN && m < (int)(LUZ_INICIO_MIN + RAMPA_MIN)) ||
            (m >= (int)(LUZ_FIM_MIN - RAMPA_MIN) && m < (int)LUZ_FIM_MIN))) rampa_ok = 0;
    }
    sprintf(buf, "minutos de noite=%d (esperado 360), violacoes=%d", noite, viol);
    chk_cond("dia inteiro: >0 so dentro de [05:00,23:00)", viol == 0 && noite == 360, buf);
    chk_cond("programa_em_rampa coerente em 0..1439", rampa_ok, "");

    /* fase e proxima virada (tela e celular) */
    chk("fase 04:59 = noite", programa_fase(299), FASE_NOITE);
    chk("fase 05:00 = subindo", programa_fase(300), FASE_SUBINDO);
    chk("fase 05:30 = dia", programa_fase(330), FASE_DIA);
    chk("fase 22:29 = dia", programa_fase(1349), FASE_DIA);
    chk("fase 22:30 = descendo", programa_fase(1350), FASE_DESCENDO);
    chk("fase 23:00 = noite", programa_fase(1380), FASE_NOITE);
    chk("proxima de 02:00 = 05:00 (acende)", programa_proxima_virada(120), 300);
    chk("proxima de 05:10 = 05:30 (fim da subida)", programa_proxima_virada(310), 330);
    chk("proxima de 12:00 = 22:30 (escurece)", programa_proxima_virada(720), 1350);
    chk("proxima de 22:45 = 23:00 (apaga)", programa_proxima_virada(1365), 1380);
    chk("proxima de 23:30 = 05:00 (dia seguinte)", programa_proxima_virada(1410), 300);
    {
        int m, fase_ok = 1;
        for (m = 0; m < 1440; m++) {   /* a fase muda exatamente na virada anunciada */
            uint16_t px = programa_proxima_virada((uint16_t)m);
            if (programa_fase(px) == programa_fase((uint16_t)m)) fase_ok = 0;
            if (programa_fase((uint16_t)((px + 1439u) % 1440u)) != programa_fase((uint16_t)m)) fase_ok = 0;
        }
        chk_cond("proxima virada: fase muda ali e nao antes, 0..1439", fase_ok, "");
    }
}

#if KI == 0
/* duty esperado do feedforward + P puro, pelas constantes do config.h */
static long esperado_p(int sp, int luz)
{
    long d = (long)D_FIXO * sp / (long)SP_DIA + (long)KP * (sp - luz) / (long)KP_DIV;
    return d < 0 ? 0 : (d > 1000 ? 1000 : d);
}
#endif

static void t_controle(void)
{
    int sp, luz, bad = 0;
    printf("== controle_calcula (D_FIXO=%d KP=%d/%d KI=%d SP_DIA=%u)\n", D_FIXO, KP, KP_DIV, KI, SP_DIA);
    chk("dff(SP_DIA) = D_FIXO", controle_dff(SP_DIA), D_FIXO);
#if KI == 0   /* com o integrador o duty depende do historico: so vale para P puro */
    chk("sp=60 luz=0   -> dff + KP*60", controle_calcula(60, 0), esperado_p(60, 0));
    chk("sp=60 luz=60  -> dff", controle_calcula(60, 60), esperado_p(60, 60));
    chk("sp=60 luz=120 -> dff - KP*60", controle_calcula(60, 120), esperado_p(60, 120));
    chk("sp=60 luz=200 -> dff - KP*140 (satura)", controle_calcula(60, 200), esperado_p(60, 200));
    controle_reset();   /* o 1o minuto da rampa vem depois da noite: o filtro do alvo (fe3e150) parte do proprio alvo */
    chk("sp=2 luz=0 (1o minuto da rampa)", controle_calcula(2, 0), esperado_p(2, 0));
#endif
    chk("sp=0 luz=0 -> 0", controle_calcula(0, 0), 0);
    chk("sp=0 luz=100 -> 0", controle_calcula(0, 100), 0);
    for (sp = 0; sp <= 255; sp++)
        for (luz = -32768; luz <= 32767; luz += 7) {
            unsigned d = controle_calcula((uint8_t)sp, (int16_t)luz);
            if (d > 1000u) bad++;
        }
    chk("saturacao: duty<=1000 (sp 0..255 x luz int16 inteiro)", bad, 0);
    chk("limite superior: sp=255 luz=-32768", controle_calcula(255, -32768), 1000);
    chk("limite inferior: sp=60 luz=32767", controle_calcula(60, 32767), 0);
}

/* planta estatica com atraso de 1 periodo: luz[n+1] = duty[n]*gnum/gden */
static void malha(int gnum, int gden, int luz0, int *luz_final, int *duty_final, int *max_luz, int *periodo_conv)
{
    int luz = luz0, duty = 0, n, conv = -1, mx = luz0, dprev = 0;
    controle_reset();
    for (n = 0; n < 400; n++) {
        duty = controle_calcula(60, (int16_t)luz);
        luz = duty * gnum / gden;
        if (luz > mx) mx = luz;
        if (conv < 0 && abs(luz - 60) <= 1 && abs(duty - dprev) <= 1) conv = n;
        dprev = duty;
    }
    *luz_final = luz; *duty_final = duty; *max_luz = mx; *periodo_conv = conv;
}

static void t_malha(void)
{
    int lf, df, mx, pc;
    char buf[128];
    printf("== malha fechada (planta estatica luz=G*duty, atraso de 1 periodo de 100 ms, sp=60)\n");
    malha((int)SP_DIA, D_FIXO, 0, &lf, &df, &mx, &pc);
    sprintf(buf, "luz=%d duty=%d pico=%d convergiu em %d periodos", lf, df, mx, pc);
    chk_cond("G nominal, partida luz=0: erro em regime = 0", lf == 60 && (KI != 0 || df == D_FIXO), buf);
    malha((int)SP_DIA, D_FIXO, 150, &lf, &df, &mx, &pc);
    sprintf(buf, "luz=%d duty=%d convergiu em %d periodos", lf, df, pc);
    chk_cond("G nominal, partida luz=150: converge", abs(lf - 60) <= 1, buf);
    malha((int)SP_DIA * 8, D_FIXO * 10, 0, &lf, &df, &mx, &pc);
    sprintf(buf, "luz=%d (erro %d) duty=%d", lf, lf - 60, df);
    chk_cond("G=0.8x (D_FIXO errado): estavel, erro < 15 pontos", abs(lf - 60) < 15 && df > 0 && df < 1000, buf);
    malha((int)SP_DIA * 12, D_FIXO * 10, 0, &lf, &df, &mx, &pc);
    sprintf(buf, "luz=%d (erro %d) duty=%d", lf, lf - 60, df);
    chk_cond("G=1.2x: estavel, erro < 15 pontos", abs(lf - 60) < 15 && df > 0 && df < 1000, buf);
    malha((int)SP_DIA * 166, D_FIXO * 100, 0, &lf, &df, &mx, &pc);
    sprintf(buf, "luz=%d duty=%d (KP*G=%.2f, limite 1.0)", lf, df, (double)KP / KP_DIV * SP_DIA / D_FIXO * 1.66);
    printf("  [INFO  ] G=1.66x: %s\n", buf);
    malha((int)SP_DIA * 25, D_FIXO * 10, 0, &lf, &df, &mx, &pc);
    sprintf(buf, "luz=%d duty=%d (KP*G=%.2f, oscila/satura se passar de 1)", lf, df, (double)KP / KP_DIV * SP_DIA / D_FIXO * 2.5);
    printf("  [INFO  ] G=2.5x:  %s\n", buf);
}

static void t_ldr(void)
{
    int r, viol = 0, mono = 1, prev = -1;
    printf("== LDR raw -> pct (RAW_ESCURO=%d RAW_CLARO=%d LUZ_MAX_PCT=%d)\n", RAW_ESCURO, RAW_CLARO, LUZ_MAX_PCT);
    chk("raw=RAW_ESCURO", raw_para_pct(RAW_ESCURO), 0);
    chk("raw=RAW_CLARO", raw_para_pct(RAW_CLARO), 100);
    chk("raw=0 (abaixo do escuro): nunca negativo", raw_para_pct(0), 0);
    chk("raw=4095 -> (4095-ESCURO)*100/(CLARO-ESCURO)", raw_para_pct(4095), (4095 - RAW_ESCURO) * 100 / (RAW_CLARO - RAW_ESCURO));
    chk("raw no meio da faixa -> 50", raw_para_pct((uint16_t)(RAW_ESCURO + (RAW_CLARO - RAW_ESCURO + 1) / 2)), 50);
    for (r = 0; r <= 4095; r++) {
        int p = raw_para_pct((uint16_t)r);
        if (p < 0 || p > LUZ_MAX_PCT) viol++;
        if (p < prev) mono = 0;
        prev = p;
    }
    chk("raw 0..4095: sempre em [0, LUZ_MAX_PCT]", viol, 0);
    chk_cond("raw 0..4095: monotono", mono, "");
    adc_val = (uint16_t)RAW_CLARO; ldr_le();
    chk("ldr_le com ADC=RAW_CLARO nas 16 amostras: pct", ldr_get_pct(), 100);
    chk("ldr_le: raw medio", ldr_get_raw(), RAW_CLARO);
}

static void t_tempo(void)
{
    uint32_t i; int minutos = 0;
    char buf[110];
    printf("== ticks_por_minuto e relogio\n");
    programa_init(0, 0);   chk("real: ticks por minuto", ticks_por_minuto(), 6000);
    programa_init(299, 1); chk("demo fora de rampa (min 299)", ticks_por_minuto(), 17);
    programa_init(300, 1); chk("demo rampa de subida (min 300)", ticks_por_minuto(), 100);
    programa_init(329, 1); chk("demo rampa de subida (min 329)", ticks_por_minuto(), 100);
    programa_init(330, 1); chk("demo plateau (min 330)", ticks_por_minuto(), 17);
    programa_init(1349, 1); chk("demo plateau (min 1349)", ticks_por_minuto(), 17);
    programa_init(1350, 1); chk("demo rampa de descida (min 1350)", ticks_por_minuto(), 100);
    programa_init(1380, 1); chk("demo noite (min 1380)", ticks_por_minuto(), 17);

    g_ticks = 1000u; programa_init(10, 0);
    g_ticks = 1000u + 5999u;
    chk("real: apos 5999 ticks nao vira", programa_atualiza(), 0);
    g_ticks = 1000u + 6000u;
    chk("real: apos 6000 ticks vira", programa_atualiza(), 1);
    chk("minuto 10 -> 11", programa_get_min(), 11);
    chk("segunda chamada no mesmo tick", programa_atualiza(), 0);

    g_ticks = 0u; programa_init(1439, 0);
    g_ticks = 6000u; programa_atualiza();
    chk("1439 -> 0 (volta do dia)", programa_get_min(), 0);

    g_ticks = 0xFFFFFFFFu - 3000u; programa_init(100, 0);
    for (i = 0; i < 12000u; i++) { g_ticks++; minutos += programa_atualiza(); }
    sprintf(buf, "g_ticks final=%lu", (unsigned long)g_ticks);
    chk("volta do contador: 12000 ticks = 2 minutos", minutos, 2);
    chk("volta do contador: minuto 100 -> 102", programa_get_min(), 102);
    chk_cond("o contador de ticks realmente deu a volta", g_ticks < 12000u, buf);

    g_ticks = 0u; programa_init(10, 0);
    g_ticks = 30000u; minutos = 0;
    while (programa_atualiza()) minutos++;
    chk("laco atrasado: 30000 ticks = 5 minutos (em while)", minutos, 5);

    g_ticks = 0u; programa_init(0, 1); minutos = 0;
    for (i = 0; i < 100000u && minutos < 1440; i++) { g_ticks++; if (programa_atualiza()) minutos++; }
    sprintf(buf, "%lu ticks = %.1f s; esperado 60*100+1380*17 = 29460", (unsigned long)i, i / 100.0);
    chk_cond("demo: um dia inteiro volta a 00:00", minutos == 1440 && programa_get_min() == 0 && i == 29460u, buf);

    g_ticks = 0u; programa_init(500, 0); g_ticks = 4000u;
    programa_alterna_demo();
    chk("alterna V: minuto preservado", programa_get_min(), 500);
    chk("alterna V: agora demo", programa_get_demo(), 1);
}

#if KI != 0
static void t_ki(void)
{
    int luz = 0, duty = 0, n;
    printf("== integrador ativo (KI=%d), planta G*0.8\n", KI);
    controle_reset();
    for (n = 0; n < 2000; n++) {
        duty = controle_calcula(60, (int16_t)luz);
        luz = duty * 60 * 8 / (500 * 10);
    }
    chk_cond("KI>0: erro em regime ~0 com G errado", abs(luz - 60) <= 1, "");
    controle_reset();
    for (n = 0; n < 3000; n++) controle_calcula(60, 0);
    n = controle_calcula(60, 200);
    printf("  [INFO  ] apos 3000 periodos saturado, luz=200: duty=%d (anti-windup limita |I| a 1000)\n", n);
}
#endif

int main(void)
{
    t_programa();
    t_controle();
    t_malha();
    t_ldr();
    t_tempo();
#if KI != 0
    t_ki();
#endif
    printf("\nRESUMO: %d passaram, %d falharam\n", n_ok, n_fail);
    return n_fail != 0;
}
