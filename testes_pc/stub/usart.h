/* stub minimo de usart.h (sem HAL) para compilar serial.c no PC */
#ifndef USART_H_STUB
#define USART_H_STUB
#include <stdint.h>
#include "hal_stub.h"
#define USART1 ((void *)1)
#define USART1_IRQn 37
typedef struct { void *Instance; } UART_HandleTypeDef;
extern UART_HandleTypeDef huart1;
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h);
static inline void HAL_NVIC_DisableIRQ(int irq) { (void)irq; }
static inline void HAL_NVIC_EnableIRQ(int irq) { (void)irq; }
#define __DMB() do { } while (0)
#endif
