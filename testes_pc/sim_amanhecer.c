/* sim_amanhecer.c: roda o controle_calcula REAL na rampa do amanhecer (demonstração: 1 minuto do galpão
 * = 10 chamadas de 100 ms) contra uma planta NÃO linear: a curva do teste 9 de 25/11 (duty -> luz) e,
 * abaixo de 10% de duty, onde o teste 9 não tem pontos, uma lei de potência do LDR (expoente 0,855,
 * ajustado entre 10 e 20%). Atraso de 1 período (a luz lida é a do duty do bloco anterior).
 * Mede quantas vezes o duty "vai e volta" com salto de 4 permil ou mais (sobe e desce em blocos seguidos),
 * que é o pisca visto na fita.
 * Uso: rodar.sh sim (compila com o controle.c do firmware). */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "config.h"
#include "controle.h"
#include "programa.h"

static const double T9_DUTY[] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
static const double T9_RAW[]  = {1698, 2300, 2617, 2811, 2948, 3043, 3120, 3181, 3234, 3280};

static double raw_de_duty(double d)
{
    int i;
    if (d <= 0.0) return (double)RAW_ESCURO;
    if (d < 100.0) {               /* LDR: R = R100 * (d/100)^-0.855, divisor com 4095 */
        double r100 = 4095.0 / 1698.0 - 1.0;
        double r = r100 * pow(d / 100.0, -0.855);
        return 4095.0 / (1.0 + r);
    }
    for (i = 1; i < 10; i++)
        if (d <= T9_DUTY[i])
            return T9_RAW[i - 1] + (T9_RAW[i] - T9_RAW[i - 1]) * (d - T9_DUTY[i - 1]) / (T9_DUTY[i] - T9_DUTY[i - 1]);
    return T9_RAW[9];
}

static int luz_de_duty(int d, double ganho)
{
    double raw = raw_de_duty(d * ganho);
    long p = (long)((raw - RAW_ESCURO) * 100.0 / (RAW_CLARO - RAW_ESCURO));
    return p < 0 ? 0 : (p > 200 ? 200 : (int)p);
}

/* roda de 04:58 a 05:40 (sobe) ou 22:20 a 23:02 (desce); devolve vai-e-volta e imprime se detalhe */
static int roda(int ini, int fim, double ganho, int detalhe, int *erro_max_dia)
{
    int m, k, d, dant = 0, sentido_ant = 0, vv = 0, luz = 0;
    controle_reset();
    *erro_max_dia = 0;
    for (m = ini; m <= fim; m++) {
        int sp = sp_programa((uint16_t)m);
        for (k = 0; k < 10; k++) {
            d = controle_calcula((uint8_t)sp, (int16_t)luz);
            if (d != dant) {
                int s = (d > dant) ? 1 : -1;
                if (sentido_ant != 0 && s != sentido_ant && abs(d - dant) >= 4 && m > ini + 1) vv++;   /* o 1o minuto e a partida da simulacao */
                sentido_ant = s;
            }
            if (detalhe && (m - ini) < 14) printf("  %02d:%02d sp=%2d luz=%3d duty=%4d\n", m / 60, m % 60, sp, luz, d);
            dant = d;
            luz = luz_de_duty(d, ganho);
        }
        if (sp == SP_DIA && abs(luz - SP_DIA) > *erro_max_dia) *erro_max_dia = abs(luz - SP_DIA);
    }
    return vv;
}

uint32_t app_ticks(void) { return 0u; }   /* o relógio real não é usado aqui */

int main(int argc, char **argv)
{
    (void)argv;
    double g;
    int e, falhas = 0;
    int detalhe = (argc > 1);
    printf("== amanhecer e anoitecer com a planta nao linear (teste 9 de 25/11)\n");
    printf("   duty 2 -> luz %d%%, 5 -> %d%%, 10 -> %d%%, 50 -> %d%%, 100 -> %d%%, 145 -> %d%%\n",
           luz_de_duty(2, 1), luz_de_duty(5, 1), luz_de_duty(10, 1), luz_de_duty(50, 1), luz_de_duty(100, 1), luz_de_duty(145, 1));
    for (g = 0.5; g <= 2.01; g += 0.5) {
        int vs = roda(298, 340, g, detalhe && g == 1.0, &e);
        int es;
        int vd = roda(1340, 1382, g, 0, &es);
        int ok = (vs == 0 && vd == 0);
        if (!ok) falhas++;
        printf("  [%s] planta %.1fx: vai-e-volta do duty amanhecer=%d anoitecer=%d, erro no dia pleno=%d\n",
               ok ? "OK    " : "FALHOU", g, vs, vd, e);
    }
    printf("%s\n", falhas ? "SIMULACAO: PISCA" : "SIMULACAO: SEM PISCA");
    return falhas ? 1 : 0;
}
