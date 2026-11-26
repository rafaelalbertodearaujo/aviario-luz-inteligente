/* app.c: base do firmware e escalonador por ticks (05 §6.1).
 * Parte original: Victor (FW1, base). Integração do controle: Rafael.
 * Dono de HAL_TIM_PeriodElapsedCallback (TIM2 a 100 Hz: só ticks++). */
#include "app.h"
#include "main.h"
#include "tim.h"
#include "config.h"
#include "luz.h"
#include "ina219.h"
#include "ldr.h"
#include "programa.h"
#include "controle.h"
#include "pir.h"
#include "serial.h"
#include "tela.h"
#include "bkp.h"
#include "hc05_cfg.h"
#include "iwdg.h"

static volatile uint32_t ticks;   /* 1 tick = 10 ms; só o TIM2 escreve (32 bits: leitura atômica no M3) */
static uint32_t t_100ms;          /* referência do bloco de 100 ms */
static uint32_t t_1s;             /* referência do bloco de 1 s */
static uint32_t perdidos_100ms;   /* períodos pulados por atraso (ver no depurador, T11) */
static estado_t estado;           /* retrato para serial e tela */
static uint16_t bits_salvos;      /* último DR3 gravado */
static uint8_t  fase_ant;         /* fase do programa no bloco anterior (avisos) */
static uint16_t entradas_ant;     /* contagem do PIR no bloco anterior */
static uint8_t  inspecao_ant;     /* inspeção aplicada no bloco anterior */
static uint16_t duty_suave;       /* duty da inspeção com subida e descida gradual */
#define PASSO_SUAVE 5u            /* permil por bloco de 100 ms: 15% em ~3 s */

/* Monta os bits de estado da BKP (DR3). */
static uint16_t monta_bits(void)
{
    uint16_t b = 0u;
    if (serial_get_modo() == MODO_MANUAL) { b |= BKP_BIT_MANUAL; }
    if (programa_get_demo() != 0u)        { b |= BKP_BIT_DEMO; }
    b |= (uint16_t)((serial_get_duty_manual() / 10u) << BKP_DUTY_SHIFT);
    return b;
}

/* Bloco de 100 ms: serial -> LDR -> SP -> controle -> PWM -> PIR -> INA219
 * -> retrato -> BKP -> uma página do OLED. Pior caso ~15 ms (SPEC §3). */
static void bloco_100ms(void)
{
    uint16_t min;
    uint8_t sp;
    uint8_t noite;
    uint8_t inspecao = 0u;
    uint16_t duty;
    modo_t modo;
    uint16_t bits;

    serial_processa(&estado);               /* FW8: comandos recebidos */

    ldr_le();                               /* FW2 */
    min = programa_get_min();
    sp = sp_programa(min);                  /* relógio + sp_programa */
    noite = (sp == 0u) ? 1u : 0u;
    modo = serial_get_modo();

    if (modo == MODO_MANUAL) {
        controle_reset();
        duty = serial_get_duty_manual();
    } else if ((pir_inspecao_ativa() != 0u) && (noite != 0u)) {
        /* inspeção só vale à noite: se amanhecer no meio da inspeção, o
         * controle assume já neste bloco, com o integrador zerado */
        controle_reset();
        inspecao = 1u;
        duty = DUTY_INSPECAO;               /* luz baixa de inspeção */
    } else {
        duty = controle_calcula(sp, ldr_get_pct());   /* FW6 */
    }
    /* inspeção gradual: sobe até DUTY_INSPECAO e, ao acabar, desce até apagar
     * (só à noite em AUTO; amanheceu ou manual: o controle assume direto) */
    if (inspecao != 0u) {
        if (duty_suave < DUTY_INSPECAO) {
            duty_suave = (uint16_t)(duty_suave + PASSO_SUAVE);
            if (duty_suave > DUTY_INSPECAO) { duty_suave = DUTY_INSPECAO; }
        }
        duty = duty_suave;
    } else if ((duty_suave > 0u) && (noite != 0u) && (modo == MODO_AUTO)) {
        duty_suave = (duty_suave > PASSO_SUAVE) ? (uint16_t)(duty_suave - PASSO_SUAVE) : 0u;
        duty = duty_suave;
    } else {
        duty_suave = 0u;
    }
    luz_set_duty(duty);                     /* FW4 */

    /* PIR só arma a inspeção à noite em AUTO; no MANUAL só conta */
    pir_atualiza((uint8_t)((noite != 0u) && (modo == MODO_AUTO)));   /* FW3 */
    ina219_le();                            /* FW5 */

    /* retrato do sistema */
    estado.min_galpao = min;
    estado.demo = programa_get_demo();
    estado.modo = modo;
    estado.noite = noite;
    estado.inspecao = inspecao;
    estado.sp = sp;
    estado.ldr_raw = ldr_get_raw();
    estado.luz = ldr_get_pct();
    estado.duty = duty;
    estado.ina_ok = ina219_ok();
    estado.vbus_mV = ina219_vbus_mV();
    estado.i_mA = ina219_i_mA();
    estado.p_mW = ina219_p_mW();
    estado.e_real = ina219_e_real();
    estado.e_timer = ina219_e_timer();
    estado.e_fixo = ina219_e_fixo();
    estado.econ_timer = ina219_econ_timer();
    estado.econ_sensor = ina219_econ_sensor();
    estado.entradas = pir_get_contagem();
    estado.resets = bkp_get_resets();
    estado.insp_s = pir_inspecao_restante_s();

    /* avisos no celular quando algo muda (só textos; não mexe no controle) */
    if (programa_fase(min) != fase_ant) {
        fase_ant = programa_fase(min);
        serial_aviso_fase(fase_ant, min);
    }
    if (estado.entradas > entradas_ant) {
        serial_aviso_pir(estado.entradas, (uint8_t)((noite != 0u) && (modo == MODO_AUTO)));
    }
    entradas_ant = estado.entradas;     /* volta a 0 à meia-noite sem aviso */
    if (inspecao_ant && !inspecao) {
        serial_aviso_fim_inspecao();
    }
    inspecao_ant = inspecao;

    /* modo mudou: grava na BKP */
    bits = monta_bits();
    if (bits != bits_salvos) {
        bits_salvos = bits;
        bkp_salva(min, bits);
    }

    tela_envia_pagina();                    /* FW7: ~12 ms de I2C, 6 páginas por segundo */
}

/* Bloco de 1 s: PC13, desenho da tela (o envio é no bloco de 100 ms),
 * telemetria (vai pela fila de TX, sem bloquear). */
static void bloco_1s(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    tela_atualiza(&estado);                 /* FW7 */
    serial_telemetria(&estado);             /* FW8 */
}

/* Virada de minuto do galpão: energia, BKP e contagem do dia. */
static void minuto_virou(void)
{
    uint16_t min = programa_get_min();
    uint16_t anterior = (uint16_t)((min + MIN_POR_DIA - 1u) % MIN_POR_DIA);
    uint8_t sp_ant = sp_programa(anterior);

    ina219_soma_minuto(sp_ant, controle_dff(sp_ant));   /* FW5 */
    bkp_salva(min, bits_salvos);                         /* FW9 */
    if (min == 0u) {
        pir_zera_contagem();
    }
}

void app_init(void)
{
    uint16_t min = HORA_INICIAL_MIN;
    uint16_t bits = (MODO_DEMO_PADRAO != 0u) ? BKP_BIT_DEMO : 0u;
    modo_t modo;

    __HAL_DBGMCU_FREEZE_IWDG();             /* depurador parado não reseta a placa */

#if MODO_CONFIG_HC05
    hc05_cfg_executa();                     /* não retorna */
#endif

    bkp_init();
    (void)bkp_restaura(&min, &bits);        /* só muda min/bits após reset do IWDG */

    luz_init();                             /* fita apagada antes de tudo */
    ldr_init();
    (void)ina219_init();
    tela_init();
    pir_init();
    programa_init(min, ((bits & BKP_BIT_DEMO) != 0u) ? 1u : 0u);
    modo = ((bits & BKP_BIT_MANUAL) != 0u) ? MODO_MANUAL : MODO_AUTO;
    serial_restaura(modo, (uint16_t)(((bits >> BKP_DUTY_SHIFT) & 0xFFu) * 10u));
    serial_init();

    bits_salvos = monta_bits();
    bkp_salva(min, bits_salvos);            /* grava DR1..DR3 já na partida */
    fase_ant = programa_fase(min);          /* sem aviso de fase na partida */

    t_100ms = ticks;
    t_1s = ticks;
    (void)HAL_TIM_Base_Start_IT(&htim2);
}

/* Um bloco por vez: se o laço atrasou mais de um período, os perdidos são
 * pulados (sem rajada) e a fase é mantida. O relógio do galpão não perde
 * nada, porque conta ticks direto. */
void app_loop(void)
{
    uint32_t agora = ticks;                 /* uma leitura por volta */
    uint32_t dt;

    dt = agora - t_100ms;
    if (dt >= TICKS_100MS) {
        t_100ms += dt - (dt % TICKS_100MS);
        perdidos_100ms += (dt / TICKS_100MS) - 1u;
        bloco_100ms();
    }
    dt = agora - t_1s;
    if (dt >= TICKS_1S) {
        t_1s += dt - (dt % TICKS_1S);
        bloco_1s();
    }
    while (programa_atualiza() != 0u) {
        minuto_virou();
    }

    /* IWDG (~2 s): único refresh do firmware, só no fim do laço. */
    (void)HAL_IWDG_Refresh(&hiwdg);
}

uint32_t app_ticks(void)
{
    return ticks;
}

/* TIM2 a 100 Hz: só conta. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        ticks++;
    }
}
