/* app.h: base do firmware, escalonador por ticks e retrato do estado.
 * Parte original: Victor (FW1, base). Integração do controle: Rafael.
 *
 * Donos dos callbacks HAL (cada um definido em UM arquivo só):
 *   HAL_TIM_PeriodElapsedCallback -> app.c    (TIM2: só ticks++)
 *   HAL_GPIO_EXTI_Callback        -> pir.c    (PA1: só marca a borda)
 *   HAL_UART_RxCpltCallback       -> serial.c (guarda o byte e rearma)
 *   HAL_UART_TxCpltCallback       -> serial.c (manda o próximo trecho da fila de TX)
 *   HAL_UART_ErrorCallback        -> serial.c (rearma a recepção)
 *
 * Troca de estado: cada módulo expõe getters; serial e tela recebem
 * um ponteiro para o retrato estado_t, montado pelo app a cada 100 ms. */
#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef enum {
    MODO_AUTO = 0,      /* programa de luz + controle */
    MODO_MANUAL = 1     /* duty pelo celular (+/-) */
} modo_t;

/* Retrato do sistema, só leitura para serial e tela. */
typedef struct {
    uint16_t min_galpao;    /* 0..1439, minutos desde 00:00 */
    uint8_t  demo;          /* 1 = modo demonstração */
    modo_t   modo;
    uint8_t  noite;         /* 1 = programa sem luz (sp_programa == 0) */
    uint8_t  inspecao;      /* 1 = luz de inspeção aplicada agora */
    uint8_t  sp;            /* setpoint do programa, % (0..100) */
    uint16_t ldr_raw;       /* média de 16 do ADC, 0..4095 */
    int16_t  luz;           /* luz medida, % (0..LUZ_MAX_PCT) */
    uint16_t duty;          /* duty aplicado, permil (0..1000) */
    uint8_t  ina_ok;        /* 1 = última leitura do INA219 válida */
    uint16_t vbus_mV;       /* tensão do barramento, mV */
    int16_t  i_mA;          /* corrente da fita, mA */
    uint32_t p_mW;          /* potência da fita, mW */
    uint32_t e_real;        /* energias em mW*min (mWh = valor / 60) */
    uint32_t e_timer;
    uint32_t e_fixo;
    int16_t  econ_timer;    /* economia contra o timer, permil (décimos de %) */
    int16_t  econ_sensor;   /* economia do sensor, permil (pode ser negativa) */
    uint16_t entradas;      /* acionamentos do PIR no dia */
    uint16_t resets;        /* resets pelo IWDG (BKP DR4) */
    uint8_t  insp_s;        /* segundos que faltam da inspeção (no fim: o le_estado.ps1 lê os campos acima por posição) */
} estado_t;

/* Chamada uma vez em main.c (USER CODE 2), depois dos MX_..._Init(). */
void app_init(void);

/* Uma volta do laço; chamada em main.c dentro do while(1). Não bloqueia. */
void app_loop(void);

/* Ticks de 10 ms desde a partida (TIM2). Qualquer módulo pode ler. */
uint32_t app_ticks(void);

#endif /* APP_H */
