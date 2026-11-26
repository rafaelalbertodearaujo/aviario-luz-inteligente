/* hc05_cfg.c: configuração única do HC-05 (AT a 38400 pela USART1).
 * Parte original: Rafael (divisão de 15/10); no 06 §6.4 passou ao Victor.
 * Só é chamada com MODO_CONFIG_HC05 = 1. Pode usar HAL_Delay (modo de
 * gravação única). O IWDG não para depois de ligado: o laço dá refresh. */
#include "hc05_cfg.h"
#include "main.h"
#include "usart.h"
#include "config.h"
#include "ssd1306.h"
#include "iwdg.h"

#define RESP_MAX    21u     /* uma linha do OLED (21 caracteres) */

static void refresh(void)
{
    (void)HAL_IWDG_Refresh(&hiwdg);
}

/* Texto terminado em zero -> HAL_UART_Transmit (timeout curto). */
static void envia(const char *s)
{
    uint16_t n = 0u;
    while (s[n] != '\0') { n++; }
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)s, n, HC05_TIMEOUT_MS);
}

/* Lê a resposta por polling até o fim da linha ('\n') ou até o prazo.
 * Guarda os primeiros RESP_MAX-1 caracteres imprimíveis. */
static void le_resposta(char *r)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t n = 0u;
    uint8_t c;
    uint8_t fim = 0u;

    r[0] = '\0';
    while ((fim == 0u) && ((HAL_GetTick() - t0) < HC05_TIMEOUT_MS)) {
        refresh();
        if (HAL_UART_Receive(&huart1, &c, 1u, 5u) == HAL_OK) {
            if (c == '\n') {
                if (n > 0u) { fim = 1u; }       /* ignora linha vazia */
            } else if ((c >= 0x20u) && (c <= 0x7Eu) && (n < (RESP_MAX - 1u))) {
                r[n] = (char)c;
                n++;
                r[n] = '\0';
            } else {
                /* '\r' e o resto: descarta */
            }
        }
    }
    if (n == 0u) {
        r[0] = '?'; r[1] = '\0';                /* sem resposta */
    }
}

/* Envia o comando, lê a resposta e mostra "<rotulo><resposta>" na linha lin. */
static void passo(const char *cmd, const char *rotulo, uint8_t lin)
{
    char r[RESP_MAX];
    char l[RESP_MAX + 1u];
    uint8_t i = 0u;
    uint8_t j = 0u;

    envia(cmd);
    le_resposta(r);
    while ((rotulo[i] != '\0') && (i < (RESP_MAX - 1u))) { l[i] = rotulo[i]; i++; }
    while ((r[j] != '\0') && (i < RESP_MAX)) { l[i] = r[j]; i++; j++; }
    l[i] = '\0';
    ssd1306_texto(0u, lin, l);
    (void)ssd1306_envia();
}

void hc05_cfg_executa(void)
{
    (void)ssd1306_init();                       /* usa HAL_Delay (só aqui, na partida) */
    ssd1306_texto(0u, 0u, "HC-05 CONFIG 38400");
    (void)ssd1306_envia();

    /* USART1 a 38400 (modo AT do HC-05), em tempo de execução */
    (void)HAL_UART_DeInit(&huart1);
    huart1.Init.BaudRate = HC05_BAUD_AT;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        ssd1306_texto(0u, 1u, "ERRO UART");
        (void)ssd1306_envia();
    } else {
        passo("AT\r\n", "AT:", 2u);
        passo("AT+NAME=" HC05_NOME "\r\n", "NOME:", 3u);
        passo("AT+PSWD=" HC05_PIN "\r\n", "PIN:", 4u);
        ssd1306_texto(0u, 6u, "FIM: volte p/ 0");
        (void)ssd1306_envia();
    }

    for (;;) {                                  /* pisca o PC13, sem retornar */
        refresh();
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(250u);
    }
}
