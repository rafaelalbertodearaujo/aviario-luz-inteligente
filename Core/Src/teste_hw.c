/* teste_hw.c: testes de bancada por componente (escolha em config.h, TESTE_HW).
 * Com TESTE_HW = 0 nada daqui entra no binário.
 * Reusa os módulos do firmware (ldr, pir, luz, ina219, ssd1306); só o HC-05
 * usa a UART por polling, para não depender da fila do serial.c.
 * Significado de v[] em cada teste: ver o comentário de cada função. */
#include "config.h"
#include "teste_hw.h"

#if TESTE_HW

#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "usart.h"
#include "iwdg.h"
#include "ldr.h"
#include "pir.h"
#include "luz.h"
#include "ina219.h"
#include "ssd1306.h"

volatile teste_res_t teste_res;

/* ---- utilidades ---- */

/* PC13 acende em nível baixo. */
static void led(uint8_t aceso)
{
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, aceso ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* Aceso fixo = rodando; 1 Hz = OK; 5 Hz = FALHA. */
static void led_padrao(void)
{
    uint32_t t = HAL_GetTick();
    if (teste_res.estado == TESTE_OK) {
        led(((t / 500u) & 1u) != 0u);
    } else if (teste_res.estado == TESTE_FALHA) {
        led(((t / 100u) & 1u) != 0u);
    } else {
        led(1u);
    }
}

/* Uma volta do laço: cão de guarda e contador. */
static void volta(void)
{
    HAL_IWDG_Refresh(&hiwdg);
    teste_res.ciclos++;
}

/* Inteiro sem sinal para texto, em s (cabe 10 dígitos + '\0'). */
static char *u2s(uint32_t n, char *s)
{
    char tmp[11];
    uint8_t i = 0u, j = 0u;
    do {
        tmp[i++] = (char)('0' + (n % 10u));
        n /= 10u;
    } while (n != 0u);
    while (i > 0u) {
        s[j++] = tmp[--i];
    }
    s[j] = '\0';
    return s + j;
}

/* Endereço de 7 bits responde no I2C1? */
static uint8_t i2c_responde(uint8_t addr7)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr7 << 1), 2u, I2C_TIMEOUT_MS) == HAL_OK) ? 1u : 0u;
}

/* ---- 1. chip: v0 = Flash em kB, v1 = IDCODE (pode ler 0 sem depurador,
 * errata do F1), v2..v4 = ID único. OK se Flash >= 64 kB (C8T6). ---- */
static void teste_chip(void)
{
    teste_res.v[0] = (int32_t)(*(volatile uint16_t *)0x1FFFF7E0u);
    teste_res.v[1] = (int32_t)DBGMCU->IDCODE;
    teste_res.v[2] = (int32_t)(*(volatile uint32_t *)0x1FFFF7E8u);
    teste_res.v[3] = (int32_t)(*(volatile uint32_t *)0x1FFFF7ECu);
    teste_res.v[4] = (int32_t)(*(volatile uint32_t *)0x1FFFF7F0u);
    teste_res.estado = (teste_res.v[0] >= 64) ? TESTE_OK : TESTE_FALHA;
    for (;;) {
        volta();
        led_padrao();
    }
}

/* ---- 2. LDR: v0 = raw (média de 16), v1 = mín, v2 = máx, v3 = %,
 * v4 = máx - mín, v5 = s preso em cima (raw >= 4085), v6 = tensão em mV.
 * OK quando a faixa passa de 1000, com o mín abaixo de 1000 e o máx acima
 * de 2000 (tapar e iluminar; um PA0 solto oscilando não passa).
 * FALHA só se ficar 5 s preso em 4095 sem faixa: curto do PA0 ao 3V3.
 * Perto de 0 não é falha: LDR bem tapado também dá ~0 (14/11). Se não sair
 * de ~0 com luz: LDR ou fio aberto, ou LDR no GND em vez do 3V3. ---- */
static void teste_ldr(void)
{
    uint32_t t = HAL_GetTick();
    uint32_t t_ext = 0u;
    uint32_t n = 0u;
    uint16_t raw, mn = 0xFFFFu, mx = 0u;

    ldr_init();
    for (;;) {
        volta();
        led_padrao();
        if ((HAL_GetTick() - t) < 100u) {
            continue;
        }
        t += 100u;
        ldr_le();
        raw = ldr_get_raw();
        if (n < 3u) {
            n++;                        /* descarta as 3 primeiras (300 ms) */
        } else {
            if (raw < mn) { mn = raw; }
            if (raw > mx) { mx = raw; }
        }
        if (raw >= 4085u) {
            t_ext++;
        } else {
            t_ext = 0u;
        }
        teste_res.v[0] = raw;
        teste_res.v[1] = (n < 3u) ? 0 : mn;
        teste_res.v[2] = mx;
        teste_res.v[3] = ldr_get_pct();
        teste_res.v[4] = (n < 3u) ? 0 : ((int32_t)mx - (int32_t)mn);
        teste_res.v[5] = (int32_t)(t_ext / 10u);
        teste_res.v[6] = (int32_t)raw * 3300 / 4095;    /* tensão no PA0, mV */
        if (teste_res.v[4] >= 1000 && mn < 1000u && mx > 2000u) {
            teste_res.estado = TESTE_OK;
        } else if (t_ext >= 50u) {
            teste_res.estado = TESTE_FALHA;
        } else if (teste_res.estado == TESTE_FALHA) {
            teste_res.estado = TESTE_RODANDO;
        }
    }
}

/* ---- 3. PIR: v0 = nível do PA1, v1 = bordas de subida vistas aqui,
 * v2 = contagem do pir.c (só depois do aquecimento de 60 s),
 * v3 = s desde ligar, v4 = inspeção ativa. PC13 copia o PA1.
 * OK com 3 contagens do pir.c (ignora os disparos sozinhos do aquecimento). ---- */
static void teste_pir(void)
{
    uint32_t t = HAL_GetTick();
    uint8_t alto, ant = 0u;
    uint32_t bordas = 0u;

    pir_init();
    for (;;) {
        volta();
        alto = (HAL_GPIO_ReadPin(PIR_EXTI_GPIO_Port, PIR_EXTI_Pin) == GPIO_PIN_SET) ? 1u : 0u;
        if (alto && !ant) {
            bordas++;
        }
        ant = alto;
        led(alto);
        if ((HAL_GetTick() - t) >= 100u) {
            t += 100u;
            pir_atualiza(1u);           /* como se fosse noite */
        }
        teste_res.v[0] = alto;
        teste_res.v[1] = (int32_t)bordas;
        teste_res.v[2] = pir_get_contagem();
        teste_res.v[3] = (int32_t)(HAL_GetTick() / 1000u);
        teste_res.v[4] = pir_inspecao_ativa();
        if (pir_get_contagem() >= 3u) {
            teste_res.estado = TESTE_OK;
        }
    }
}

/* ---- 4. HC-05: a 9600, manda "PING n" por segundo e devolve com
 * "ECO x" cada byte recebido. v0 = PINGs enviados, v1 = bytes recebidos,
 * v2 = último byte, v3 = erros de TX, v4 = erros de RX (ORE etc.).
 * OK ao receber o primeiro byte do celular. ---- */
static void teste_hc05(void)
{
    uint32_t t = HAL_GetTick();
    uint32_t n = 0u;
    char msg[24];
    char *p;
    uint8_t b;
    HAL_StatusTypeDef st;

    for (;;) {
        volta();
        led_padrao();
        st = HAL_UART_Receive(&huart1, &b, 1u, 1u);
        if (st == HAL_OK) {
            teste_res.v[1]++;
            teste_res.v[2] = b;
            teste_res.estado = TESTE_OK;
            msg[0] = 'E'; msg[1] = 'C'; msg[2] = 'O'; msg[3] = ' ';
            msg[4] = (char)b; msg[5] = '\r'; msg[6] = '\n';
            if (HAL_UART_Transmit(&huart1, (uint8_t *)msg, 7u, 50u) != HAL_OK) {
                teste_res.v[3]++;
            }
        } else if (st == HAL_ERROR) {
            teste_res.v[4]++;
            __HAL_UART_CLEAR_OREFLAG(&huart1);
            huart1.ErrorCode = HAL_UART_ERROR_NONE;
            huart1.RxState = HAL_UART_STATE_READY;
        }
        if ((HAL_GetTick() - t) >= 1000u) {
            t += 1000u;
            p = msg;
            *p++ = 'P'; *p++ = 'I'; *p++ = 'N'; *p++ = 'G'; *p++ = ' ';
            p = u2s(++n, p);
            *p++ = '\r'; *p++ = '\n';
            if (HAL_UART_Transmit(&huart1, (uint8_t *)msg, (uint16_t)(p - msg), 50u) == HAL_OK) {
                teste_res.v[0] = (int32_t)n;
            } else {
                teste_res.v[3]++;
            }
        }
    }
}

/* ---- 5. OLED: v0 = 0x3C responde, v1 = init ok, v2 = envios ok,
 * v3 = envios com falha. Tela: título e contador a cada 0,5 s.
 * OK com 0x3C, init e envios sem falha; FALHA se 0x3C não responde. ---- */
static void teste_oled(void)
{
    uint32_t t = HAL_GetTick();
    char s[22];

    teste_res.v[0] = i2c_responde(0x3Cu);
    teste_res.v[1] = ssd1306_init();
    if (!teste_res.v[0]) {
        teste_res.estado = TESTE_FALHA;
    }
    for (;;) {
        volta();
        led_padrao();
        if ((HAL_GetTick() - t) < 500u) {
            continue;
        }
        t += 500u;
        ssd1306_limpa();
        ssd1306_texto(0u, 0u, "TESTE OLED");
        ssd1306_texto(0u, 2u, "aviario luz");
        s[0] = 'n'; s[1] = '='; (void)u2s(teste_res.ciclos & 0xFFFFu, &s[2]);
        ssd1306_texto(0u, 4u, s);
        if (ssd1306_envia()) {
            teste_res.v[2]++;
        } else {
            teste_res.v[3]++;
            i2c1_recupera();
        }
        if (teste_res.v[0] && teste_res.v[1] && teste_res.v[2] > 0 && teste_res.v[3] == 0) {
            teste_res.estado = TESTE_OK;
        }
    }
}

/* ---- 6. INA219: v0 = 0x40 responde, v1 = init ok, v2 = leitura ok,
 * v3 = Vbus mV, v4 = I mA, v5 = P mW, v6 = leituras com falha.
 * Sem a parte de potência ligada, Vbus e I ficam perto de 0 (normal).
 * OK com 0x40 e leitura ok; FALHA se 0x40 não responde. ---- */
static void teste_ina219(void)
{
    uint32_t t = HAL_GetTick();

    teste_res.v[0] = i2c_responde(0x40u);
    teste_res.v[1] = ina219_init();
    if (!teste_res.v[0]) {
        teste_res.estado = TESTE_FALHA;
    }
    for (;;) {
        volta();
        led_padrao();
        if ((HAL_GetTick() - t) < 500u) {
            continue;
        }
        t += 500u;
        ina219_le();
        teste_res.v[2] = ina219_ok();
        if (!teste_res.v[2]) {
            teste_res.v[6]++;
        }
        teste_res.v[3] = ina219_vbus_mV();
        teste_res.v[4] = ina219_i_mA();
        teste_res.v[5] = (int32_t)ina219_p_mW();
        if (teste_res.v[0] && teste_res.v[2]) {
            teste_res.estado = TESTE_OK;
        }
    }
}

/* ---- 7. Varredura I2C a cada 2 s: v0..v3 = mapa de bits dos endereços
 * de 7 bits que respondem (v0 bit n = endereço n, v1 = 32+n...), v4 = quantos,
 * v5 = 0x3C presente, v6 = 0x40 presente. OK se achar algum. ---- */
static void teste_i2c(void)
{
    uint32_t t = HAL_GetTick() - 2000u;
    uint32_t mapa[4];
    uint8_t a, k;
    int32_t qtd;

    for (;;) {
        volta();
        led_padrao();
        if ((HAL_GetTick() - t) < 2000u) {
            continue;
        }
        t = HAL_GetTick();
        mapa[0] = mapa[1] = mapa[2] = mapa[3] = 0u;
        qtd = 0;
        for (a = 0x08u; a <= 0x77u; a++) {
            HAL_IWDG_Refresh(&hiwdg);
            if (i2c_responde(a)) {
                mapa[a >> 5] |= (1u << (a & 31u));
                qtd++;
            }
        }
        if (qtd == 0 && __HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET) {
            i2c1_recupera();
        }
        for (k = 0u; k < 4u; k++) {
            teste_res.v[k] = (int32_t)mapa[k];
        }
        teste_res.v[4] = qtd;
        teste_res.v[5] = (mapa[0x3Cu >> 5] >> (0x3Cu & 31u)) & 1u;
        teste_res.v[6] = (mapa[0x40u >> 5] >> (0x40u & 31u)) & 1u;
        teste_res.estado = (qtd > 0) ? TESTE_OK : TESTE_FALHA;
    }
}

/* ---- 8. PWM no PA6: rampa 0 -> 100% em 4 s (+5% a cada 200 ms) e volta a 0.
 * v0 = duty (permil), v1 = INA219 presente, v2 = I mA, v3 = P mW,
 * v4 = maior I vista, v5 = I a 50% (última), v6 = I a 100% (última).
 * OK depois da primeira rampa completa. ---- */
static void teste_pwm(void)
{
    uint32_t t = HAL_GetTick();
    uint16_t d = 0u;
    int16_t i;

    teste_res.v[1] = i2c_responde(0x40u);
    if (teste_res.v[1]) {
        (void)ina219_init();
    }
    luz_init();
    for (;;) {
        volta();
        led_padrao();
        if ((HAL_GetTick() - t) < 200u) {
            continue;
        }
        t += 200u;
        d = (d >= DUTY_MAX) ? 0u : (uint16_t)(d + 50u);   /* +5%; depois de 100% volta a 0 */
        luz_set_duty(d);
        teste_res.v[0] = d;
        if (teste_res.v[1]) {
            ina219_le();
            i = ina219_i_mA();
            teste_res.v[2] = i;
            teste_res.v[3] = (int32_t)ina219_p_mW();
            if (i > teste_res.v[4]) { teste_res.v[4] = i; }
            if (d == 500u)  { teste_res.v[5] = i; }
            if (d == 1000u) { teste_res.v[6] = i; }
        }
        if (d == 1000u) {
            teste_res.estado = TESTE_OK;
        }
    }
}

/* ---- 9. Calibração (maquete fechada, LDR na posição final): a fita passa
 * por 0, 10, ..., 100% e recomeça, 2,5 s em cada degrau. Em cada degrau
 * espera 1,2 s (o LDR acomodar) e tira a média do raw nos 1,3 s seguintes.
 * v0 = duty agora (permil), v1 = raw agora, v2 = degraus concluídos (sobe
 * sempre; o script grava um ponto quando muda), v3 = duty do último degrau,
 * v4 = raw médio dele, v5 = máx - mín do raw nele (ruído ou mau contato),
 * v6 = I mA, v7 = P mW (última leitura do degrau; 0 sem INA219).
 * OK depois da 1ª varredura se o raw a 100% passa o de 0% em 1000 ou mais;
 * FALHA se não passa (LDR sem ver a fita, aberto ou longe demais). ---- */
#define CAL_DEGRAU_MS   2500u
#define CAL_ESPERA_MS   1200u
#define CAL_PASSO       100u

static void teste_calib(void)
{
    uint32_t t = HAL_GetTick();
    uint32_t t_degrau = t;
    uint32_t soma = 0u, n = 0u;
    uint16_t duty = 0u, raw, mn = 0xFFFFu, mx = 0u;
    int32_t raw0 = -1;
    uint8_t tem_ina;

    ldr_init();
    tem_ina = i2c_responde(0x40u);
    if (tem_ina) {
        (void)ina219_init();
    }
    luz_init();
    luz_set_duty(duty);
    for (;;) {
        volta();
        led_padrao();
        if ((HAL_GetTick() - t) < 100u) {
            continue;
        }
        t += 100u;
        ldr_le();
        raw = ldr_get_raw();
        teste_res.v[0] = duty;
        teste_res.v[1] = raw;
        if ((t - t_degrau) >= CAL_ESPERA_MS) {      /* janela de média */
            soma += raw;
            n++;
            if (raw < mn) { mn = raw; }
            if (raw > mx) { mx = raw; }
        }
        if ((t - t_degrau) < CAL_DEGRAU_MS) {
            continue;
        }
        /* fim do degrau: publica o ponto e passa ao próximo */
        if (tem_ina) {
            ina219_le();
            teste_res.v[6] = ina219_i_mA();
            teste_res.v[7] = (int32_t)ina219_p_mW();
        }
        teste_res.v[3] = duty;
        teste_res.v[4] = (n > 0u) ? (int32_t)(soma / n) : 0;
        teste_res.v[5] = (n > 0u) ? (int32_t)mx - (int32_t)mn : 0;
        teste_res.v[2]++;
        if (duty == 0u) {
            raw0 = teste_res.v[4];
        } else if (duty >= DUTY_MAX && raw0 >= 0) {
            teste_res.estado = ((teste_res.v[4] - raw0) >= 1000) ? TESTE_OK : TESTE_FALHA;
        }
        duty = (duty >= DUTY_MAX) ? 0u : (uint16_t)(duty + CAL_PASSO);
        luz_set_duty(duty);
        t_degrau = t;
        soma = 0u;
        n = 0u;
        mn = 0xFFFFu;
        mx = 0u;
    }
}

void teste_hw_executa(void)
{
    uint8_t k;

    __HAL_DBGMCU_FREEZE_IWDG();     /* IWDG parado quando o depurador pausa */
    teste_res.id = TESTE_HW;
    teste_res.estado = TESTE_RODANDO;
    teste_res.ciclos = 0u;
    for (k = 0u; k < 8u; k++) {
        teste_res.v[k] = 0;
    }
    teste_res.magic = TESTE_MAGIC;

    switch (TESTE_HW) {
    case TESTE_CHIP:   teste_chip();   break;
    case TESTE_LDR:    teste_ldr();    break;
    case TESTE_PIR:    teste_pir();    break;
    case TESTE_HC05:   teste_hc05();   break;
    case TESTE_OLED:   teste_oled();   break;
    case TESTE_INA219: teste_ina219(); break;
    case TESTE_I2C:    teste_i2c();    break;
    case TESTE_PWM:    teste_pwm();    break;
    case TESTE_CALIB:  teste_calib();  break;
    default: break;
    }
    teste_res.estado = TESTE_FALHA;  /* número de teste inválido */
    for (;;) {
        volta();
        led_padrao();
    }
}

#endif /* TESTE_HW */
