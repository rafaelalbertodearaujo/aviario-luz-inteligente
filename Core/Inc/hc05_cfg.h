/* hc05_cfg.h: configuração única do HC-05 nº 1 (MODO_CONFIG_HC05 = 1).
 * Parte original: Rafael (divisão de 15/10); no 06 §6.4 passou ao Victor. */
#ifndef HC05_CFG_H
#define HC05_CFG_H

/* Chamada no começo de app_init quando MODO_CONFIG_HC05 = 1; não retorna.
 * Pré-requisito: EN/KEY do HC-05 em 3,3 V ANTES de energizar.
 * Passos: USART1 para HC05_BAUD_AT (38400) com HAL_UART_Init; envia
 * "AT\r\n", "AT+NAME=<HC05_NOME>\r\n", "AT+PSWD=<HC05_PIN>\r\n" em modo
 * bloqueante (HC05_TIMEOUT_MS), lê cada resposta por polling (sem IT) e
 * mostra no OLED (ssd1306_texto/envia); depois fica parado piscando o PC13. */
void hc05_cfg_executa(void);

#endif /* HC05_CFG_H */
