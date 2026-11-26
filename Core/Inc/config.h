/* config.h: todas as constantes do firmware (ver SPEC_FIRMWARE.md).
 * Parte original: todos. Valores com CALIBRAR saem da sessão de
 * calibração (05 §6.2); com GRUPO, de decisão dos três. */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------- Modos de compilação ---------- */
#define MODO_CONFIG_HC05    0   /* 1 = firmware de configuração única do HC-05 */
#define TESTE_HW            0   /* 0 = firmware normal; 1..9 = teste de bancada (teste_hw.h) */

/* ---------- Tempo (TIM2 a 100 Hz: 1 tick = 10 ms) ---------- */
#define TICKS_100MS         10u
#define TICKS_1S            100u
#define TPM_REAL            6000u   /* ticks por minuto do galpão, modo real */
#define TPM_DEMO            17u     /* modo demonstração, fora das rampas */
#define TPM_RAMPA           100u    /* modo demonstração, nas rampas */
#define MODO_DEMO_PADRAO    1u      /* GRUPO: 1 = liga em demonstração */

/* ---------- Programa de luz (minutos desde 00:00, 0..1439) ---------- */
#define MIN_POR_DIA         1440u
#define HORA_INICIAL_MIN    (12u * 60u) /* GRUPO: hora ao ligar na tomada */
#define LUZ_INICIO_MIN      (5u * 60u)  /* GRUPO: 05:00 */
#define LUZ_FIM_MIN         (23u * 60u) /* GRUPO: 23:00 */
#define RAMPA_MIN           30u         /* duração de cada rampa, min do galpão */
#define SP_DIA              60u         /* setpoint de dia, % (até ~70) */

#if SP_DIA == 0
#error "SP_DIA nao pode ser 0: setpoint 0 e tratado como noite"
#endif
#if SP_DIA < RAMPA_MIN
#error "SP_DIA < RAMPA_MIN: no 1o minuto da rampa o setpoint daria 0 (noite)"
#endif

/* ---------- LDR / ADC (PA0, 12 bits) ---------- */
#define RAW_ESCURO          4       /* teste 9 em 20/11 (mantido em 25/11: no escuro a placa lê 6 a 10, o teste 25/11 sugeriu 0) */
#define RAW_CLARO           3280    /* teste 9 em 25/11: fita 100%, maquete fechada (era 3009 em 20/11) */
#define LDR_AMOSTRAS        16u
#define LUZ_MAX_PCT         200     /* teto da escala (a cortina passa de 100%) */
#define ADC_TIMEOUT_MS      2u

/* ---------- Controle (FW6) ---------- */
#define DUTY_MAX            1000u   /* duty em permil: 0..1000 */
#define D_FIXO              145     /* teste 9 em 25/11: duty que dá 60% de luz, maquete fechada (era 196 em 20/11) */
#define KP                  1       /* teste 9 em 25/11: ganho local 0,18 %/permil em 60% e 0,52 entre 0 e 10% de duty.
                                       Com o D_FIXO novo (145) o KP 2 FALHA no teste do PC "G=1.2x" (oscila e satura em 0; pico de luz 114
                                       na partida); KP 1 passa 63/63 (converge em 8 periodos, KP*G 0,69 mesmo com G 1,66x). Era 2 em 20/11.
                                       O erro que sobra do P puro e zerado pelo KI */
#define KP_DIV              1       /* P = KP * erro / KP_DIV (ganho fracionário; >= 1) */
#define KI                  2       /* 24/11: integrador ligado (0 = sem integrador); validar com a lanterna */
#define KI_DIV              10      /* I = integ / KI_DIV; integ += KI * erro a cada 100 ms */

/* ---------- INA219 (FW5) ---------- */
#define INA219_ADDR         (0x40u << 1)  /* endereço de 8 bits do HAL */
#define INA219_CONFIG       0x17FFu       /* 16 V, ÷4, 128 amostras, contínuo: NÃO muda */
#define R_SHUNT_MOHM        100u          /* 0,1 ohm */
#define P100_mW             1426u         /* teste 9 em 25/11: fita COB a 100% na maquete (292 mA; era 1152 e 256 mA em 20/11) */
#define I2C_TIMEOUT_MS      5u

/* ---------- OLED (FW7) ---------- */
#define OLED_ADDR           (0x3Cu << 1)
#define OLED_TIMEOUT_MS     20u     /* por página de 128 bytes (~12 ms a 100 kHz) */

/* ---------- PIR (FW3) ---------- */
#define PIR_AQUECIMENTO_MS  60000u  /* ignora acionamentos após ligar */
#define PIR_TRAVA_MS        2000u   /* intervalo mínimo entre acionamentos */
#define INSPECAO_MS         15000u  /* duração da luz de inspeção (25/11: 15 s, pedido do Rafael para a demonstração e os testes; era 60 s) */
#define DUTY_INSPECAO       150u    /* CALIBRAR: 15% (05); a COB pode pedir menos (11) */

/* ---------- Serial (FW8) ---------- */
#define UART_TX_TIMEOUT_MS  100u
#define PASSO_MANUAL        100u    /* +/- 10% no manual */
#define RESUMO_S            5u      /* resumo no celular a cada N s (comando T liga/desliga) */

/* ---------- HC-05 (configuração única) ---------- */
#define HC05_BAUD_AT        38400u
#define HC05_NOME           "AVIARIO_LUZ"   /* GRUPO: nome único na sala */
#define HC05_PIN            "1234"          /* GRUPO: trocar por um PIN próprio */
#define HC05_TIMEOUT_MS     500u

#endif /* CONFIG_H */
