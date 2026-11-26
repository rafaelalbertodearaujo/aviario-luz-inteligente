/* ina219.h: medição da fita (I2C1, 0x40) e energia/economias.
 * Parte original: Rafael (FW5).
 * Contas (inteiras):
 *   vbus_mV = (reg2 >> 3) * 4
 *   shunt   = (int16_t)reg1, LSB 10 uV; i_mA = shunt * 10 / R_SHUNT_MOHM
 *   p_mW    = vbus_mV * shunt / 10000  (int32; negativo -> 0)
 *   por minuto do galpão: e_real += p_mW; e_timer += P100_mW se sp_prog > 0;
 *   e_fixo += P100_mW * dff / 1000  (todas em mW*min, uint32) */
#ifndef INA219_H
#define INA219_H

#include <stdint.h>

/* Partida (app_init): escreve INA219_CONFIG (0x17FF) no reg 0x00.
 * Devolve 1 se o INA219 respondeu. */
uint8_t ina219_init(void);

/* Bloco de 100 ms (app, por último): lê shunt (0x01) e barramento (0x02),
 * timeout I2C_TIMEOUT_MS. Em erro: mantém a última leitura válida,
 * ina219_ok() = 0 e chama i2c1_recupera(). */
void ina219_le(void);

uint8_t  ina219_ok(void);        /* 1 = última leitura válida */
uint16_t ina219_vbus_mV(void);   /* mV */
int16_t  ina219_i_mA(void);      /* mA (pode ser levemente negativo em zero) */
uint32_t ina219_p_mW(void);      /* mW, >= 0 */

/* Chamada pelo app uma vez por minuto do galpão, com o setpoint do
 * programa no minuto que terminou (sp_prog, %) e o D_ff dele (permil,
 * controle_dff). Soma as três energias com a última potência válida. */
void ina219_soma_minuto(uint8_t sp_prog, uint16_t dff);

uint32_t ina219_e_real(void);    /* mW*min */
uint32_t ina219_e_timer(void);   /* mW*min */
uint32_t ina219_e_fixo(void);    /* mW*min */

/* Economias em permil: 1000 - e_real*1000/e_x (conta em uint64_t).
 * Devolve 0 se e_x == 0; satura em -999..1000. */
int16_t ina219_econ_timer(void);
int16_t ina219_econ_sensor(void);

/* Recuperação do I2C1 (também usada pelo ssd1306): HAL_I2C_DeInit +
 * HAL_I2C_Init; se o barramento seguir BUSY, 9 pulsos manuais no SCL
 * (PB6) antes de reiniciar. Não usar dentro de interrupção. */
void i2c1_recupera(void);

#endif /* INA219_H */
