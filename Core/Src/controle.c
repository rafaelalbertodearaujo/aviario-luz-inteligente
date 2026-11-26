/* controle.c: feedforward + P (+ I com anti-windup; KI = 0 desliga).
 * Parte original: Rafael (FW6, controle).
 * Tudo em inteiros: duty em permil (0..1000), sp e luz em pontos %.
 * Estabilidade (planta estática G = SP_DIA / D_FIXO, em % por permil, com
 * um período de atraso): basta KP / KP_DIV * G < 1; alvo ~0,5.
 *
 * 25/11 (pedido do Rafael): o alvo passa por um filtro de ~0,5 s antes de entrar na conta. Na demonstração
 * o relógio anda 1 minuto do galpão por segundo e o alvo da rampa sobe 2 pontos de uma vez a cada segundo;
 * o feedforward (reta) e o P davam um degrau de duty maior que o da luz de verdade, a luz passava do alvo e o
 * controle recuava: a fita "piscava" a cada segundo no amanhecer e no anoitecer. Com o alvo suavizado o degrau
 * vira uma subida de ~0,5 s. Com o alvo parado (dia pleno) a conta é idêntica à de antes. */
#include "controle.h"
#include "config.h"

#if KP_DIV <= 0
#error "KP_DIV deve ser >= 1"
#endif
#if KI_DIV <= 0
#error "KI_DIV deve ser >= 1"
#endif

#define ESC         16                                          /* alvo suavizado e erro em 1/16 de ponto */
#define SP_FILT_DIV 5                                           /* cada chamada (100 ms) anda 1/5 do que falta */
#define INTEG_MAX   ((int32_t)DUTY_MAX * (int32_t)KI_DIV * ESC) /* |I| <= 1000 permil */

static int32_t integ = 0;   /* I = integ / (ESC * KI_DIV) */
static int32_t spf = 0;     /* alvo suavizado, em 1/ESC de ponto */
static uint8_t spf_ok = 0u; /* 0: a próxima conta parte direto do alvo, sem suavizar */

/* Satura em 0..DUTY_MAX. */
static uint16_t satura(int32_t d)
{
    if (d < 0) {
        return 0u;
    }
    if (d > (int32_t)DUTY_MAX) {
        return (uint16_t)DUTY_MAX;
    }
    return (uint16_t)d;
}

uint16_t controle_dff(uint8_t sp)
{
    return satura(((int32_t)D_FIXO * (int32_t)sp) / (int32_t)SP_DIA);
}

uint16_t controle_calcula(uint8_t sp, int16_t luz)
{
    int32_t alvo;
    int32_t delta;
    int32_t passo;
    int32_t erro;   /* 1/ESC de ponto */
    int32_t base;   /* D_ff + P */
    int32_t u;

    if (sp == 0u) {             /* noite: fita apagada, sem memória */
        integ = 0;
        spf_ok = 0u;
        return 0u;
    }

    alvo = (int32_t)sp * ESC;
    if (spf_ok == 0u) {         /* primeira conta depois da noite, do manual ou da inspeção */
        spf = alvo;
        spf_ok = 1u;
    } else {
        delta = alvo - spf;
        passo = delta / SP_FILT_DIV;
        if ((passo == 0) && (delta != 0)) {
            passo = (delta > 0) ? 1 : -1;
        }
        spf += passo;
    }

    erro = spf - (int32_t)luz * ESC;
    /* ganho proporcional ao alvo abaixo do dia pleno: perto do escuro 1 permil de duty muda muito mais a luz
     * (teste 9: 0 a 10% de duty dão 52% de luz) e o KP 1 do dia fazia a fita piscar no começo da rampa */
    if (spf < (int32_t)SP_DIA * ESC) {
        erro = (erro * spf) / ((int32_t)SP_DIA * ESC);
    }
    base = ((int32_t)D_FIXO * spf) / ((int32_t)SP_DIA * ESC)
         + ((int32_t)KP * erro) / ((int32_t)KP_DIV * ESC);

    if (KI != 0) {
        /* anti-windup: só integra se a saída não está presa no sentido do erro */
        u = base + integ / ((int32_t)KI_DIV * ESC);
        if (!((u >= (int32_t)DUTY_MAX && erro > 0) || (u <= 0 && erro < 0))) {
            integ += (int32_t)KI * erro;
            if (integ > INTEG_MAX) {
                integ = INTEG_MAX;
            } else if (integ < -INTEG_MAX) {
                integ = -INTEG_MAX;
            }
        }
    }

    u = base + integ / ((int32_t)KI_DIV * ESC);
    return satura(u);
}

void controle_reset(void)
{
    integ = 0;
    spf_ok = 0u;
}
