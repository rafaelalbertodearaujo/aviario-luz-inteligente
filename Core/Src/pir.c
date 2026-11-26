/* pir.c: PIR no PA1 (EXTI1), aquecimento, trava, contagem e inspeção.
 * Parte original: Victor (FW3).
 * Dono de HAL_GPIO_EXTI_Callback.
 * Tempos em chamadas de 100 ms de pir_atualiza(). */
#include "pir.h"
#include "main.h"
#include "config.h"

#define N_AQUEC   (PIR_AQUECIMENTO_MS / 100u)
#define N_TRAVA   (PIR_TRAVA_MS / 100u)
#define N_INSPEC  (INSPECAO_MS / 100u)

static volatile uint8_t borda;      /* escrita na ISR, limpa no laço */
static uint8_t  nivel_ant;          /* nível do pino na chamada anterior */
static uint16_t aquec;              /* chamadas que faltam de aquecimento */
static uint16_t trava;              /* chamadas que faltam da trava */
static uint16_t inspecao;           /* chamadas que faltam da inspeção */
static uint16_t contagem;

void pir_init(void)
{
    borda = 0u;
    nivel_ant = 0u;
    aquec = N_AQUEC;
    trava = 0u;
    inspecao = 0u;
    contagem = 0u;
}

void pir_atualiza(uint8_t noite)
{
    uint8_t alto = (HAL_GPIO_ReadPin(PIR_EXTI_GPIO_Port, PIR_EXTI_Pin) == GPIO_PIN_SET) ? 1u : 0u;
    uint8_t evento;

    /* consome a borda da ISR (leitura e limpeza curtas) */
    evento = borda;
    borda = 0u;
    /* borda perdida: nível alto agora e baixo antes */
    if (alto && !nivel_ant) {
        evento = 1u;
    }
    nivel_ant = alto;

    /* temporizadores correm a cada chamada */
    if (inspecao > 0u) {
        inspecao--;
    }
    if (trava > 0u) {
        trava--;
    }

    if (aquec > 0u) {
        aquec--;
        return;                 /* aquecimento: descarta */
    }
    if (!evento || trava > 0u) {
        return;                 /* sem evento ou dentro da trava */
    }

    trava = (uint16_t)N_TRAVA;
    if (contagem < 0xFFFFu) {
        contagem++;
    }
    if (noite) {
        inspecao = (uint16_t)N_INSPEC;  /* liga ou reinicia */
    }
}

uint8_t pir_inspecao_ativa(void)
{
    return (inspecao > 0u) ? 1u : 0u;
}

uint8_t pir_inspecao_restante_s(void)
{
    return (uint8_t)((inspecao + 9u) / 10u);
}

uint16_t pir_get_contagem(void)
{
    return contagem;
}

void pir_zera_contagem(void)
{
    contagem = 0u;
}

/* EXTI1 (PA1, borda de subida): só marca a borda. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == PIR_EXTI_Pin) {
        borda = 1u;
    }
}
