/* controle.h: feedforward + P (+ I com anti-windup, desligado com KI = 0).
 * Parte original: Rafael (FW6, controle).
 *   sp suavizado: a cada chamada anda 1/5 do que falta até o sp (25/11; parado = igual ao sp)
 *   dff  = D_FIXO * sp / SP_DIA            (int32, satura em 0..1000)
 *   erro = sp - luz                        (pontos %)
 *   P    = KP * erro / KP_DIV
 *   I    = integ / KI_DIV; integ += KI * erro a cada chamada, só se a saída
 *          não estiver saturada no sentido do erro; |integ| <= 1000 * KI_DIV
 *   duty = dff + P + I, saturado em 0..1000 */
#ifndef CONTROLE_H
#define CONTROLE_H

#include <stdint.h>

/* Bloco de 100 ms (app, modo AUTO, sem inspeção). sp em % (0..100),
 * luz em % (0..LUZ_MAX_PCT). Com sp == 0 (noite): zera o integrador e
 * devolve 0. Devolve o duty em permil. */
uint16_t controle_calcula(uint8_t sp, int16_t luz);

/* Feedforward puro, permil (0..1000). Também usado pelo app no E_fixo. */
uint16_t controle_dff(uint8_t sp);

/* Zera o integrador (app chama no modo MANUAL e na inspeção). */
void controle_reset(void);

#endif /* CONTROLE_H */
