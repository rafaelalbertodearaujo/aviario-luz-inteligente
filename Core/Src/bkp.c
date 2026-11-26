/* bkp.c: relógio e estado nos registradores de backup (DR1..DR4).
 * Parte original: Victor (FW9).
 * Acesso direto a BKP->DRx (CMSIS): o módulo HAL de RTC não está habilitado.
 * DR1 = min_galpao; DR2 = BKP_MAGICO; DR3 = bits de estado; DR4 = resets IWDG. */
#include "bkp.h"
#include "main.h"
#include "config.h"

void bkp_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();             /* libera a escrita no domínio de backup */
}

uint8_t bkp_restaura(uint16_t *min, uint16_t *bits)
{
    uint8_t restaurou = 0u;

    /* só restaura após reset do IWDG, com dados válidos (DR2 e minuto no intervalo) */
    if ((__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) &&
        ((uint16_t)BKP->DR2 == BKP_MAGICO) &&
        ((uint16_t)BKP->DR1 < MIN_POR_DIA)) {
        *min = (uint16_t)BKP->DR1;
        *bits = (uint16_t)BKP->DR3;
        BKP->DR4 = (uint16_t)(BKP->DR4 + 1u);
        restaurou = 1u;
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();          /* limpa os flags depois de lidos */
    return restaurou;
}

void bkp_salva(uint16_t min, uint16_t bits)
{
    BKP->DR1 = min;
    BKP->DR2 = BKP_MAGICO;
    BKP->DR3 = bits;
}

uint16_t bkp_get_resets(void)
{
    return (uint16_t)BKP->DR4;
}
