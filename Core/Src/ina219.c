/* ina219.c: INA219 (I2C1, 0x40), energia e economias.
 * Parte original: Rafael (FW5). */
#include "ina219.h"
#include "i2c.h"
#include "config.h"

#define INA_REG_CONFIG  0x00u
#define INA_REG_SHUNT   0x01u
#define INA_REG_BUS     0x02u

static uint8_t  ok = 0u;
static uint16_t vbus = 0u;      /* mV */
static int16_t  i_ma = 0;       /* mA */
static uint32_t p_mw = 0u;      /* mW */
static uint32_t e_real = 0u;    /* mW*min */
static uint32_t e_timer = 0u;
static uint32_t e_fixo = 0u;

/* Escreve um registrador de 16 bits (big-endian). 1 = ok. */
static uint8_t ina_escreve(uint8_t reg, uint16_t val)
{
    uint8_t b[2];

    b[0] = (uint8_t)(val >> 8);
    b[1] = (uint8_t)(val & 0xFFu);
    return (HAL_I2C_Mem_Write(&hi2c1, INA219_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                              b, 2u, I2C_TIMEOUT_MS) == HAL_OK) ? 1u : 0u;
}

/* Lê um registrador de 16 bits (big-endian). 1 = ok. */
static uint8_t ina_le_reg(uint8_t reg, uint16_t *val)
{
    uint8_t b[2];

    if (HAL_I2C_Mem_Read(&hi2c1, INA219_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                         b, 2u, I2C_TIMEOUT_MS) != HAL_OK) {
        return 0u;
    }
    *val = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
    return 1u;
}

/* Partida: grava a configuração fixa 0x17FF. */
uint8_t ina219_init(void)
{
    ok = ina_escreve(INA_REG_CONFIG, INA219_CONFIG);
    if (!ok) {
        i2c1_recupera();
    }
    return ok;
}

/* Bloco de 100 ms: lê shunt e barramento; em erro mantém a última leitura. */
void ina219_le(void)
{
    uint16_t r_shunt, r_bus;
    int16_t shunt;
    int32_t p;

    if (!ina_le_reg(INA_REG_SHUNT, &r_shunt) ||
        !ina_le_reg(INA_REG_BUS, &r_bus)) {
        ok = 0u;
        i2c1_recupera();
        return;
    }
    ok = 1u;
    vbus = (uint16_t)((r_bus >> 3) * 4u);
    shunt = (int16_t)r_shunt;                       /* LSB 10 uV */
    i_ma = (int16_t)(((int32_t)shunt * 10) / (int32_t)R_SHUNT_MOHM);
    p = ((int32_t)vbus * (int32_t)shunt) / 10000;   /* máx. ~2,56e8 */
    p_mw = (p > 0) ? (uint32_t)p : 0u;
}

uint8_t ina219_ok(void)
{
    return ok;
}

uint16_t ina219_vbus_mV(void)
{
    return vbus;
}

int16_t ina219_i_mA(void)
{
    return i_ma;
}

uint32_t ina219_p_mW(void)
{
    return p_mw;
}

/* Uma vez por minuto do galpão, com a última potência válida. */
void ina219_soma_minuto(uint8_t sp_prog, uint16_t dff)
{
    e_real += p_mw;
    if (sp_prog > 0u) {
        e_timer += P100_mW;
    }
    e_fixo += (P100_mW * (uint32_t)dff) / 1000u;
}

uint32_t ina219_e_real(void)
{
    return e_real;
}

uint32_t ina219_e_timer(void)
{
    return e_timer;
}

uint32_t ina219_e_fixo(void)
{
    return e_fixo;
}

/* Economia em permil contra a referência e_x; satura em -999..1000. */
static int16_t economia(uint32_t e_x)
{
    int64_t r;

    if (e_x == 0u) {
        return 0;
    }
    r = 1000 - (int64_t)(((uint64_t)e_real * 1000u) / e_x);
    if (r > 1000) {
        r = 1000;
    } else if (r < -999) {
        r = -999;
    }
    return (int16_t)r;
}

int16_t ina219_econ_timer(void)
{
    return economia(e_timer);
}

int16_t ina219_econ_sensor(void)
{
    return economia(e_fixo);
}

/* Atraso curto (~5 us a 72 MHz) para os pulsos do SCL, sem HAL_Delay. */
static void atraso_scl(void)
{
    for (volatile uint32_t n = 0u; n < 150u; n++) {
    }
}

/* Recupera o I2C1: DeInit + Init; se BUSY, 9 pulsos no SCL (PB6) antes. */
void i2c1_recupera(void)
{
    uint8_t ocupado = (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET) ? 1u : 0u;
    GPIO_InitTypeDef g = {0};

    (void)HAL_I2C_DeInit(&hi2c1);

    if (ocupado) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin = GPIO_PIN_6;                 /* SCL como GPIO open-drain */
        g.Mode = GPIO_MODE_OUTPUT_OD;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &g);
        g.Pin = GPIO_PIN_7;                 /* SDA livre (pull-up externo) */
        g.Mode = GPIO_MODE_OUTPUT_OD;
        HAL_GPIO_Init(GPIOB, &g);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
        atraso_scl();
        for (uint8_t i = 0u; i < 9u; i++) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
            atraso_scl();
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
            atraso_scl();
        }
        /* STOP: SDA sobe com SCL alto */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
        atraso_scl();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        atraso_scl();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
        atraso_scl();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
    }
    (void)HAL_I2C_Init(&hi2c1);
}
