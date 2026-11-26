/* bkp.h: relógio e estado nos registradores de backup (05 §6.4).
 * Parte original: Victor (FW9).
 * DR1 = min_galpao; DR2 = BKP_MAGICO; DR3 = bits de estado; DR4 = resets IWDG. */
#ifndef BKP_H
#define BKP_H

#include <stdint.h>

#define BKP_MAGICO        0xA5A5u

/* Bits de DR3 */
#define BKP_BIT_MANUAL    (1u << 0)   /* 1 = modo MANUAL */
#define BKP_BIT_DEMO      (1u << 1)   /* 1 = modo demonstração */
#define BKP_DUTY_SHIFT    8u          /* bits 8..15 = duty manual / 10 (0..100) */

/* Partida (app_init, antes de tudo): liga os clocks PWR e BKP e
 * HAL_PWR_EnableBkUpAccess(). */
void bkp_init(void);

/* Partida, logo após bkp_init. Se o reset veio do IWDG (RCC_FLAG_IWDGRST)
 * e DR2 == BKP_MAGICO: preenche *min e *bits, incrementa DR4 e devolve 1.
 * Senão não mexe nos ponteiros e devolve 0. Sempre limpa os flags de
 * reset (__HAL_RCC_CLEAR_RESET_FLAGS). */
uint8_t bkp_restaura(uint16_t *min, uint16_t *bits);

/* App chama na partida, quando o minuto muda e quando os bits mudam:
 * DR1 = min, DR2 = BKP_MAGICO, DR3 = bits. */
void bkp_salva(uint16_t min, uint16_t bits);

/* Valor de DR4 (mostrado no S como rst=N). */
uint16_t bkp_get_resets(void);

#endif /* BKP_H */
