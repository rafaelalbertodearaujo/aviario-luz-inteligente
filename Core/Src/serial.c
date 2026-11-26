/* serial.c: USART1 + HC-05: comandos, resumo periódico e avisos.
 * Parte original: Victor (FW8). Textos de 19/11 (Rafael): frases curtas
 * que se explicam sozinhas no celular (manual: BLUETOOTH_E_TELA.md).
 * Dono de HAL_UART_RxCpltCallback, HAL_UART_TxCpltCallback e HAL_UART_ErrorCallback.
 * RX por interrupção (1 byte) -> fila circular de 32; tratamento no laço.
 * TX por interrupção a partir de uma fila de 512. Linhas de até ~40
 * caracteres (cabem na largura do celular em pé). */
#include "serial.h"
#include "usart.h"
#include "config.h"
#include "programa.h"
#include "tela.h"
#include <stdio.h>

#define FILA_TAM 32u
#define TX_TAM   512u                       /* potência de 2 (máscara) */
#define TX_MASK  (TX_TAM - 1u)

static uint8_t rx_byte;                     /* destino do HAL_UART_Receive_IT */
static volatile uint8_t fila[FILA_TAM];
static volatile uint8_t f_head;             /* escrito só pela ISR */
static volatile uint8_t f_tail;             /* escrito só pelo laço */

/* TX por interrupção: bloqueante custava ~50 ms por linha e atrasava o
 * bloco de 100 ms. Índices de 16 bits: leitura atômica no M3. */
static uint8_t tx_buf[TX_TAM];
static volatile uint16_t tx_head;           /* escrito só pelo laço */
static volatile uint16_t tx_tail;           /* escrito só no TxCplt */
static volatile uint16_t tx_n;              /* bytes em envio; 0 = UART parada */

static modo_t   modo = MODO_AUTO;
static uint16_t duty_manual;
static uint8_t  resumo_ligado = 1u;         /* comando T */
static uint8_t  resumo_cont;                /* segundos desde o último resumo */
static uint8_t  painel;                     /* comando P: 1 linha por segundo para o painel do notebook */

/* Começa a enviar o trecho contíguo pendente. Chamar com a IRQ da USART1
 * mascarada (laço) ou dentro dela (TxCplt). */
static void tx_inicia(void)
{
    uint16_t t = tx_tail;
    uint16_t h = tx_head;
    uint16_t n;

    if (tx_n != 0u || t == h) {
        return;
    }
    n = (h > t) ? (uint16_t)(h - t) : (uint16_t)(TX_TAM - t);
    if (HAL_UART_Transmit_IT(&huart1, &tx_buf[t], n) == HAL_OK) {
        tx_n = n;
    }
}

/* Envia uma string terminada em zero: copia na fila de TX e volta logo.
 * Fila cheia: o resto da string é descartado (não bloqueia). */
void serial_envia(const char *s)
{
    uint16_t h = tx_head;
    while (*s != '\0') {
        uint16_t prox = (uint16_t)((h + 1u) & TX_MASK);
        if (prox == tx_tail) {
            break;
        }
        tx_buf[h] = (uint8_t)*s++;
        h = prox;
    }
    __DMB();                            /* dados antes de publicar */
    tx_head = h;                        /* publica uma só vez */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    tx_inicia();
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void serial_init(void)
{
    f_head = 0u;
    f_tail = 0u;
    tx_head = 0u;
    tx_tail = 0u;
    tx_n = 0u;
    resumo_cont = 0u;
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1u);
    serial_envia("Aviario Luz ligado. Mande ? para ajuda\r\n");
}

void serial_restaura(modo_t m, uint16_t d)
{
    modo = m;
    duty_manual = (d > DUTY_MAX) ? (uint16_t)DUTY_MAX : d;
}

modo_t serial_get_modo(void)
{
    return modo;
}

uint16_t serial_get_duty_manual(void)
{
    return duty_manual;
}

/* ---------- formatação ---------- */

/* permil como "45.6" (com sinal) */
static void fmt_permil(char *dst, size_t n, int16_t v)
{
    int32_t a = (v < 0) ? -(int32_t)v : (int32_t)v;
    (void)snprintf(dst, n, "%s%ld.%ld", (v < 0) ? "-" : "", (long)(a / 10), (long)(a % 10));
}

/* mW como "1,25 W" */
static void fmt_watts(char *dst, size_t n, uint32_t mw)
{
    (void)snprintf(dst, n, "%lu,%02lu W", (unsigned long)(mw / 1000u),
                   (unsigned long)((mw % 1000u) / 10u));
}

/* Envia uma linha formatada com \r\n no fim. */
static void linha(const char *s)
{
    serial_envia(s);
    serial_envia("\r\n");
}

/* ---------- comandos ---------- */

static void cmd_ajuda(void)
{
    linha("Comandos (letra + enviar):");
    linha("A  automatico: programa + sensor");
    linha("M  manual: lampada fixa");
    linha("+  -  manual: lampada +10% / -10%");
    linha("S  numeros brutos (calibracao)");
    linha("V  relogio real / acelerado");
    linha("F  congela / retoma o relogio");
    linha("L  lampada 100% (timer comum)");
    linha("T  liga/desliga o resumo de 5 s");
    linha("P  painel do notebook (1 linha/s)");
    linha("1 2 3 4  tela: resumo, controle,");
    linha("         energia, comandos");
    linha("?  esta ajuda");
}

static void cmd_status(const estado_t *e)
{
    char b[64];                         /* energia: pior caso ~60 caracteres */
    char ect[10];
    char esn[10];

    linha("Calibracao (numeros brutos):");
    (void)snprintf(b, sizeof b, "LDR raw=%u (0-4095) luz=%d%% alvo=%u%%",
                   (unsigned)e->ldr_raw, (int)e->luz, (unsigned)e->sp);
    linha(b);
    (void)snprintf(b, sizeof b, "PWM duty=%u/1000 modo=%c relogio=%c",
                   (unsigned)e->duty,
                   e->inspecao ? 'I' : ((modo == MODO_MANUAL) ? 'M' : 'A'),
                   e->demo ? 'D' : 'R');
    linha(b);
    (void)snprintf(b, sizeof b, "INA219 ok=%u %u mV %d mA %lu mW",
                   (unsigned)e->ina_ok, (unsigned)e->vbus_mV, (int)e->i_mA,
                   (unsigned long)e->p_mW);
    linha(b);
    (void)snprintf(b, sizeof b, "Energia mWh real=%lu timer=%lu fixo=%lu",
                   (unsigned long)(e->e_real / 60u), (unsigned long)(e->e_timer / 60u),
                   (unsigned long)(e->e_fixo / 60u));
    linha(b);
    fmt_permil(ect, sizeof ect, e->econ_timer);
    fmt_permil(esn, sizeof esn, e->econ_sensor);
    (void)snprintf(b, sizeof b, "Economia timer=%s%% sensor=%s%%", ect, esn);
    linha(b);
    (void)snprintf(b, sizeof b, "Entradas=%u resets=%u",
                   (unsigned)e->entradas, (unsigned)e->resets);
    linha(b);
}

static void trata_cmd(char c, const estado_t *e)
{
    char b[48];
    char orig = c;                      /* como veio, para a mensagem de erro */

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    switch (c) {
    case 'A':
        modo = MODO_AUTO;
        linha("OK A: automatico, segue o programa de luz");
        break;
    case 'M':
        if (modo != MODO_MANUAL) {
            duty_manual = (e->duty > DUTY_MAX) ? (uint16_t)DUTY_MAX : e->duty;
        }
        modo = MODO_MANUAL;
        (void)snprintf(b, sizeof b, "OK M: manual, lampada %u%%. Use + e -",
                       (unsigned)(duty_manual / 10u));
        linha(b);
        break;
    case '+':
    case '-':
        if (modo != MODO_MANUAL) {
            (void)snprintf(b, sizeof b, "ERRO %c: mande M antes (+ e - no manual)", c);
        } else {
            if (c == '+') {
                duty_manual = (uint16_t)(duty_manual + PASSO_MANUAL);
                if (duty_manual > DUTY_MAX) {
                    duty_manual = (uint16_t)DUTY_MAX;
                }
            } else {
                duty_manual = (duty_manual > PASSO_MANUAL) ? (uint16_t)(duty_manual - PASSO_MANUAL) : 0u;
            }
            (void)snprintf(b, sizeof b, "OK %c: lampada %u%%", c, (unsigned)(duty_manual / 10u));
        }
        linha(b);
        break;
    case 'S':
        cmd_status(e);
        break;
    case 'V':
        programa_alterna_demo();
        linha(programa_get_demo() ? "OK V: relogio acelerado (demonstracao)"
                                  : "OK V: relogio real");
        break;
    case 'F':
        linha(programa_alterna_congela() ? "OK F: relogio parado" : "OK F: relogio andando");
        break;
    case 'L':
        modo = MODO_MANUAL;
        duty_manual = (uint16_t)DUTY_MAX;
        linha("OK L: manual, lampada 100% (timer comum)");
        break;
    case 'P':
        painel = 1u;                    /* idempotente: o painel manda P ao conectar */
        linha("OK P: painel ligado (T volta ao resumo)");
        break;
    case 'T':
        if (painel) {                   /* saindo do painel: volta ao resumo */
            painel = 0u;
            resumo_ligado = 1u;
        } else {
            resumo_ligado = (resumo_ligado != 0u) ? 0u : 1u;
        }
        resumo_cont = 0u;
        linha(resumo_ligado ? "OK T: resumo de 5 s ligado"
                            : "OK T: resumo desligado (avisos seguem)");
        break;
    case '?':
    case 'H':
        cmd_ajuda();
        break;
    case '1':
    case '2':
    case '3':
    case '4':
        (void)snprintf(b, sizeof b, "OK %c: tela %s", c, tela_pagina((uint8_t)(c - '0')));
        linha(b);
        break;
    case 'W':
        linha("OK W: travando para testar o watchdog");
        for (;;) {
            /* trava: o IWDG reinicia (T10) */
        }
    default:
        if (orig > ' ' && orig <= '~') {
            (void)snprintf(b, sizeof b, "ERRO %c: comando desconhecido, mande ?", orig);
            linha(b);
        } else {
            linha("ERRO: comando desconhecido, mande ?");
        }
        break;
    }
}

/* Consome a fila; ignora \r, \n e espaço. */
void serial_processa(const estado_t *e)
{
    while (f_tail != f_head) {
        char c = (char)fila[f_tail];
        f_tail = (uint8_t)((f_tail + 1u) % FILA_TAM);
        if (c == '\r' || c == '\n' || c == ' ') {
            continue;
        }
        trata_cmd(c, e);
    }
}

/* ---------- resumo periódico (chamado a cada 1 s) ---------- */

void serial_telemetria(const estado_t *e)
{
    char b[48];
    char w[16];
    uint16_t px;

    if (painel) {
        /* Formato do painel do hub (guia-3d/js/painel/telemetria.js):
         * hora=05:51 modo=AUTO demo=1 alvo=60 luz=32 lamp=64 mW=1250 mA=250 eco=35 ent=3 insp=0
         * (insp = segundos que faltam da luz de inspeção; campo a mais não quebra o leitor antigo) */
        char p[112];
        char eco[8];
        if (e->e_real == 0u || e->e_timer == 0u) {
            (void)snprintf(eco, sizeof eco, "--");
        } else {
            (void)snprintf(eco, sizeof eco, "%d", (int)(e->econ_timer / 10));
        }
        (void)snprintf(p, sizeof p,
                       "hora=%02u:%02u modo=%s demo=%u alvo=%u luz=%d lamp=%u mW=%lu mA=%d eco=%s ent=%u insp=%u",
                       (unsigned)(e->min_galpao / 60u), (unsigned)(e->min_galpao % 60u),
                       e->inspecao ? "INSP" : ((modo == MODO_MANUAL) ? "MAN" : "AUTO"),
                       (unsigned)e->demo, (unsigned)e->sp, (int)(e->luz < 0 ? 0 : e->luz),
                       (unsigned)(e->duty / 10u), (unsigned long)e->p_mW,
                       (int)(e->i_mA < 0 ? 0 : e->i_mA), eco, (unsigned)e->entradas,
                       (unsigned)(e->inspecao ? e->insp_s : 0u));
        linha(p);
        return;
    }
    if (!resumo_ligado) {
        return;
    }
    if (++resumo_cont < RESUMO_S) {
        return;
    }
    resumo_cont = 0u;

    (void)snprintf(b, sizeof b, "--- %02u:%02u %s, relogio %s ---",
                   (unsigned)(e->min_galpao / 60u), (unsigned)(e->min_galpao % 60u),
                   e->noite ? "noite" : "dia", e->demo ? "acelerado" : "real");
    linha(b);

    if (e->inspecao) {
        (void)snprintf(b, sizeof b, "INSPECAO: PIR viu alguem, faltam %u s", (unsigned)e->insp_s);
        linha(b);
    } else if (modo == MODO_MANUAL) {
        linha("MANUAL: lampada fixa (+ e - mudam)");
    } else if (e->noite) {
        linha("AUTO: noite, fita apagada");
    } else {
        linha("AUTO: segue o programa de luz");
    }

    (void)snprintf(b, sizeof b, "Luz: alvo %u%%, medida %d%%", (unsigned)e->sp, (int)e->luz);
    linha(b);

    if (e->ina_ok) {
        fmt_watts(w, sizeof w, e->p_mW);
        (void)snprintf(b, sizeof b, "Lampada %u%% (%s, %d mA)",
                       (unsigned)(e->duty / 10u), w, (int)e->i_mA);
    } else {
        (void)snprintf(b, sizeof b, "Lampada %u%% (INA219 sem leitura)", (unsigned)(e->duty / 10u));
    }
    linha(b);

    if (e->e_real == 0u || e->e_timer == 0u) {
        linha("Economia contra timer: ainda sem medida");
    } else {
        (void)snprintf(b, sizeof b, "Economia contra timer: %d%%", (int)(e->econ_timer / 10));
        linha(b);
    }

    (void)snprintf(b, sizeof b, "Entradas no PIR hoje: %u", (unsigned)e->entradas);
    linha(b);

    px = programa_proxima_virada(e->min_galpao);
    (void)snprintf(b, sizeof b, "%s %02u:%02u", programa_texto_proximo(programa_fase(e->min_galpao)),
                   (unsigned)(px / 60u), (unsigned)(px % 60u));
    linha(b);
}

/* ---------- avisos na hora (o app chama quando algo muda) ---------- */

void serial_aviso_fase(uint8_t fase, uint16_t min)
{
    char b[48];
    uint16_t px = programa_proxima_virada(min);
    unsigned hh = (unsigned)(px / 60u);
    unsigned mm = (unsigned)(px % 60u);

    switch (fase) {
    case FASE_SUBINDO:
        (void)snprintf(b, sizeof b, ">> Amanheceu: luz sobe ate %02u:%02u", hh, mm);
        break;
    case FASE_DIA:
        (void)snprintf(b, sizeof b, ">> Dia pleno: alvo %u%%", (unsigned)SP_DIA);
        break;
    case FASE_DESCENDO:
        (void)snprintf(b, sizeof b, ">> Anoitecer: luz desce, apaga %02u:%02u", hh, mm);
        break;
    default:
        (void)snprintf(b, sizeof b, ">> Noite: fita apagada ate %02u:%02u", hh, mm);
        break;
    }
    linha(b);
}

void serial_aviso_pir(uint16_t entradas, uint8_t inspecao)
{
    char b[48];

    if (inspecao) {
        (void)snprintf(b, sizeof b, ">> PIR: entrada (%u hoje), inspecao %u s",
                       (unsigned)entradas, (unsigned)(INSPECAO_MS / 1000u));
    } else {
        (void)snprintf(b, sizeof b, ">> PIR: entrada (%u hoje)", (unsigned)entradas);
    }
    linha(b);
}

void serial_aviso_fim_inspecao(void)
{
    linha(">> PIR: fim da luz de inspecao");
}

/* ---------- callbacks do HAL ---------- */

/* Byte recebido: guarda na fila e rearma. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        uint8_t prox = (uint8_t)((f_head + 1u) % FILA_TAM);
        if (prox != f_tail) {           /* fila cheia: descarta o byte */
            fila[f_head] = rx_byte;
            f_head = prox;
        }
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1u);
    }
}

/* Trecho enviado: libera na fila e manda o próximo. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        tx_tail = (uint16_t)((tx_tail + tx_n) & TX_MASK);
        tx_n = 0u;
        tx_inicia();
    }
}

/* Erro (ORE etc.): o HAL já limpou a flag; só rearma a recepção. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1u);
    }
}
