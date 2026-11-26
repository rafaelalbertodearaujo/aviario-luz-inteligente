# SPEC do firmware: Programa de Luz Inteligente para Aviários

Onda 0, 11/11/2026 (onda 2: escalonador, TX por IT, OLED por página, `KP_DIV`). Fontes: hub "micro", documentos 05 (§2, 4, 5, 6, 8, 9, 12), 06 (§6.3 a 6.5, 7, 8) e 11 (corrente, `P100_mW`, INA219). Onde 05 e 06 discordam vale o 06. Código congela em 17/11; banca em 25/11.
Este arquivo, os `.h` em `Core/Inc` e o `config.h` são a única referência para quem implementa. Constantes só no `config.h` (`/* CALIBRAR */` = sai da calibração; `/* GRUPO */` = decisão dos três).

## 1. Plataforma (pronta, não mexer no `.ioc`)

STM32F103C8T6, 72 MHz (HSE 8 MHz). TIM2 a 100 Hz com IT (1 tick = 10 ms). TIM3 CH1 no PA6, 1 kHz, ARR 999 (duty 0..1000). ADC1 IN0 no PA0, 12 MHz, 239,5 ciclos, conversão única. PA1 = EXTI1 subida, pull-down (PIR). USART1 PA9/PA10 9600 8N1 com IT (HC-05). I2C1 PB6/PB7 100 kHz (INA219 0x40, OLED 0x3C; no HAL `<< 1`). PC13 = LED (acende em nível baixo). IWDG ligado pelo CubeMX (LSI 40 kHz, prescaler 32, reload 2499, ~2 s, sem janela).
Regras: código só entre `USER CODE BEGIN/END` nos arquivos gerados; sem `HAL_Delay` fora da partida; todo I2C/UART com timeout curto, nunca `HAL_MAX_DELAY`; comentários curtos em português; inteiros (sem `float`).
Rótulos no `.ioc`: `LDR_ADC` (PA0), `PIR_EXTI` (PA1), `PWM_FITA` (PA6), `BT_TX`/`BT_RX` (PA9/PA10), `I2C_SCL`/`I2C_SDA` (PB6/PB7), `LED` (PC13); PA2 e PA3 livres. No F103 o canal do ADC é sinal compartilhado: o `.ioc` precisa de `PA0-WKUP.Signal=ADCx_IN0`, `SH.ADCx_IN0.0=ADC1_IN0,IN0` e `ADC1.master=1`, senão o CubeMX descarta o ADC1.
Gerar sem janela: `%LOCALAPPDATA%\Programs\STM32CubeMX\STM32CubeMX.exe -q <script>` (script: `config load <ioc>` / `project generate` / `exit`). Compilar sem abrir o CubeIDE: `testes_hwancada.ps1 -Teste 0 -SoCompilar` (usa o `stm32cubeidec.exe` em modo headless).

## 2. Quem fez o quê (divisão de 15/10; cada um explica o seu na banca)

| Parte | Arquivos | Dono original |
|---|---|---|
| FW1 base, ticks, escalonador | `app.c/.h` | Victor (integração do controle: Rafael) |
| FW2 LDR | `ldr.c/.h` | Nicolas |
| FW3 PIR | `pir.c/.h` | Victor |
| FW4 luz/PWM | `luz.c/.h` | Rafael |
| FW5 INA219 e economia | `ina219.c/.h` | Rafael |
| FW6 controle | `controle.c/.h` | Rafael |
| FW7 OLED | `ssd1306.c/.h`, `tela.c/.h` | Nicolas |
| FW8 serial | `serial.c/.h` | Victor |
| FW9 BKP (+ IWDG) | `bkp.c/.h` | Victor |
| Relógio do galpão | `programa.c` (relógio, `V`) | Victor |
| `sp_programa()` | `programa.c` | Nicolas |
| Config. do HC-05 | `hc05_cfg.c/.h` | Rafael (15/10); o 06 passou ao Victor |
| Constantes | `config.h` | todos |

Módulos da onda 1 (em paralelo): A = `luz` + `ina219`; B = `ldr` + `programa`; C = `pir` + `serial`; D = `ssd1306` + `tela`. Depois: `controle.c` + integração no `app.c`; e `bkp.c` + IWDG + `hc05_cfg.c`.

## 3. Arquitetura (05 §6.1)

- `HAL_TIM_PeriodElapsedCallback` (só em `app.c`): `if TIM2: ticks++`. Ninguém mais escreve `ticks`; leitura por `app_ticks()`.
- `app_loop()` (uma volta do `while(1)` do `main.c`): lê `ticks` uma vez; `dt = agora - t_100ms`; se `dt >= 10`: `t_100ms += dt - dt % 10` e **um** `bloco_100ms()` (atraso > 1 período pula os perdidos, sem rajada, e soma em `perdidos_100ms`, visível no depurador); idem 1 s com 100; depois `while (programa_atualiza()) minuto_virou();` (o relógio conta ticks direto e não perde minutos); no fim, `HAL_IWDG_Refresh(&hiwdg)` (único refresh, sob `#ifdef HAL_IWDG_MODULE_ENABLED`).
- **Bloco de 100 ms, nesta ordem:** `serial_processa` → `ldr_le` → `sp = sp_programa(min)`, `noite = (sp == 0)` → duty (MANUAL: `serial_get_duty_manual()`; AUTO com `pir_inspecao_ativa()` e noite: `DUTY_INSPECAO`; AUTO: `controle_calcula(sp, luz)`) → `luz_set_duty` → `pir_atualiza(noite)` → `ina219_le` → monta o retrato `estado_t` → se os bits de estado mudaram, `bkp_salva` → `tela_envia_pagina()` (uma página do OLED). Com `TESTE_RAMPA_PWM = 1` o duty vem de `luz_rampa_teste()`. Inspeção que passa do amanhecer: o PIR segue contando, mas o app só aplica `DUTY_INSPECAO` com noite; o controle assume no mesmo bloco, com o integrador zerado.
- **Bloco de 1 s:** inverte PC13 → `tela_atualiza(&estado)` (só desenha o buffer e marca 8 páginas) → `serial_telemetria(&estado)` (vai para a fila de TX).
- **Orçamento do pior bloco de 100 ms** (o 10º, que coincide com o de 1 s): ADC 0,4 ms + INA219 ~1 ms + página do OLED ~12,5 ms + desenho/`snprintf`/cópias < 1 ms ≈ 15 ms. Barramento I2C preso: +25 ms por transação (espera de BUSY fixa no HAL) ≈ 55 ms. Antes (OLED inteiro + TX bloqueante): ~125 ms, e o `S` sozinho ~160 ms.
- **Virada de minuto do galpão:** `ina219_soma_minuto(sp_ant, controle_dff(sp_ant))` com o minuto que terminou; `bkp_salva(min, bits)`; se `min == 0`, `pir_zera_contagem()`.
- **Partida (`app_init`):** (`MODO_CONFIG_HC05` → `hc05_cfg_executa()`, não volta) → `__HAL_DBGMCU_FREEZE_IWDG()` (antes de tudo) → `bkp_init` → `bkp_restaura(&min, &bits)` (padrão: `HORA_INICIAL_MIN`, demo = `MODO_DEMO_PADRAO`) → `luz_init` → `ldr_init` → `ina219_init` → `tela_init` → `pir_init` → `programa_init` → `serial_restaura` → `serial_init` → `bkp_salva` → `HAL_TIM_Base_Start_IT(&htim2)`.
- **Callbacks HAL, um dono cada:** TIM → `app.c`; `HAL_GPIO_EXTI_Callback` → `pir.c`; `HAL_UART_RxCpltCallback`, `HAL_UART_TxCpltCallback` e `HAL_UART_ErrorCallback` → `serial.c`. `hc05_cfg.c` usa só UART por polling (não chama `serial_init`).
- **Variáveis com ISR:** `ticks` (uint32, leitura atômica no M3); filas RX/TX da serial com índices de 1 byte, um escritor cada; `borda` do PIR (corrida de leitura/limpeza coberta pela detecção por nível, o pulso do HC-SR501 dura segundos).
- **Troca de estado:** getters de cada módulo; serial e tela recebem `const estado_t *` (definido em `app.h`). Sem globais compartilhadas.

## 4. Módulos

**FW4 luz (Rafael).** `luz_init`: duty 0 e `HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1)`. `luz_set_duty(d)`: satura em 1000, `__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, d)` (CCR 1000 > ARR 999 = 100%).

**FW5 INA219 (Rafael).** Partida: escreve `0x17FF` no reg 0x00 (16 V, ÷4 = ±160 mV ≈ 1,6 A, 128 amostras, contínuo; não muda). Sem registrador de calibração. Leitura no bloco de 100 ms (o ciclo do INA é ~136 ms; pega o mais recente): reg 0x01 (shunt) e 0x02 (barramento), 2 bytes big-endian, `HAL_I2C_Mem_Read` com `I2C_TIMEOUT_MS`.
- `vbus_mV = (reg2 >> 3) * 4` (uint16). `shunt = (int16_t)reg1`, LSB 10 µV. `i_mA = shunt * 10 / R_SHUNT_MOHM` (= shunt/10). `p_mW = (int32_t)vbus_mV * shunt / 10000` (máx. 16000 × 16000 = 2,56e8, cabe em int32); negativo → 0. Fita COB: 0,4 a 0,9 A, `P100_mW` esperado 2000 a 4500 mW (uint32 em tudo).
- Energia em **mW·min** (uint32), uma vez por minuto do galpão, com a última potência válida: `e_real += p_mW`; `e_timer += P100_mW` se `sp_prog > 0`; `e_fixo += P100_mW * dff / 1000` (dff em permil). mWh = valor / 60. Um dia a 4500 mW = 6,5e6: sem estouro por ~660 dias.
- Economias em permil: `1000 - (uint64_t)e_real * 1000 / e_x` (0 se `e_x == 0`; satura −999..1000). Contra o timer = teto (número principal); do sensor = só o ganho do LDR + controle (05 §5).
- Erro de I2C: mantém a última leitura, `ina219_ok() = 0`, `i2c1_recupera()` (`HAL_I2C_DeInit` + `HAL_I2C_Init`; se BUSY, 9 pulsos manuais no SCL PB6 como GPIO open-drain e reinit). O OLED usa a mesma função.

**FW2 LDR (Nicolas).** Partida: `HAL_ADCEx_Calibration_Start(&hadc1)`. Bloco de 100 ms: 16 × (`HAL_ADC_Start`, `HAL_ADC_PollForConversion` com 2 ms, `HAL_ADC_GetValue`), soma em uint32, média (≈ 0,34 ms no total). Timeout: mantém o anterior. `luz = (raw - RAW_ESCURO) * 100 / (RAW_CLARO - RAW_ESCURO)` em int32, satura em 0..`LUZ_MAX_PCT` (200). O raw sobe com a luz.

**Relógio + `sp_programa` (Victor + Nicolas).** `min_galpao` 0..1439. `ticks_por_minuto()`: real 6000; demo 17, ou 100 se `programa_em_rampa(min)`. `programa_atualiza()`: se `app_ticks() - ref >= tpm` → `ref += tpm`, `min = (min+1) % 1440`, devolve 1 (um minuto por chamada). `V` alterna real/demo e põe `ref = app_ticks()`. `sp_programa(min)` (função pura, testável no PC): fora de [`LUZ_INICIO_MIN`, `LUZ_FIM_MIN`) → 0; subida, `k = min - INICIO < RAMPA_MIN` → `SP_DIA*(k+1)/RAMPA_MIN`; descida, `k = FIM - min <= RAMPA_MIN` → `SP_DIA*k/RAMPA_MIN`; resto → `SP_DIA`. Dentro do fotoperíodo é sempre > 0. Demo: cada rampa 30 min × 100 ticks = 30 s; o dia ≈ 1380 × 17 + 60 × 100 ticks ≈ 5 min.

**FW6 controle (Rafael).** `duty = D_ff(sp) + P + I`, saturado em 0..1000 (permil). `D_ff = D_FIXO * sp / SP_DIA` (int32, satura). `erro = sp - luz`; `P = KP * erro / KP_DIV` (`KP_DIV = 1` padrão; serve para ganho fracionário). Estabilidade no papel: planta ≈ estática com ganho `G = SP_DIA / D_FIXO` (% por permil; 0,12 com os provisórios) e um período de atraso; basta `KP/KP_DIV × G < 1` (vale com qualquer atraso do LDR), alvo ~0,5 (`KP ≈ 0,5 × D_FIXO / SP_DIA` ≈ 4). Só P deixa erro `(perturbação)/(1 + KP·G)`. `I = integ / KI_DIV`; `integ += KI * erro` só se a saída não estiver saturada no sentido do erro (anti-windup); `|integ| <= 1000 * KI_DIV`. `sp == 0` (noite) → zera `integ`, duty 0. `KI = 0` é o núcleo do 06 (feedforward + P); o integrador entra se estabilizar até 13/11. Se o T4 mostrar atraso nas rampas, o `D_ff` vira tabela (SP 15/30/45/60).

**FW3 PIR (Victor).** Callback EXTI: se `GPIO_Pin == PIR_EXTI_Pin`, marca `borda = 1`. `pir_atualiza(noite)` a cada 100 ms: evento = borda OU (pino alto agora e baixo na chamada anterior). Descarta nos primeiros 60 s (600 chamadas) e dentro da trava de 2 s (20 chamadas). Válido: `entradas++`; à noite, inspeção por INSPECAO_MS (15 s desde 25/11, 150 chamadas; era 60 s; novo acionamento reinicia). De dia só conta. A contagem zera às 00:00 do galpão. Jumper H no módulo: movimento contínuo = um acionamento.

**FW8 serial (Victor).** RX: `HAL_UART_Receive_IT` de 1 byte → fila circular de 32 bytes (índices volatile, ISR só escreve); rearma no `RxCplt` e no `Error`. TX (onda 2): `serial_envia` copia para uma fila de 512 bytes e volta; `HAL_UART_Transmit_IT` por trechos contíguos, o `TxCplt` manda o próximo; fila cheia descarta o excesso. `M` copia o duty aplicado no bloco anterior (inclusive o da inspeção): é o "sem salto" da seção 9. Comandos (aceita minúscula; ignora `\r`, `\n`, espaço):

Comandos `A M + - S V T ? W`, resumo de 5 s (`RESUMO_S`, comando `T`), avisos na hora (fase do programa, PIR, fim da inspeção) e o formato do `S`: **manual em `BLUETOOTH_E_TELA.md`** (textos de 19/11, linhas de até ~40 caracteres; conferidos no PC pelo `testes_pc/mostra_telas.c`). Fila de TX de 512 bytes (índices de 16 bits). O `M` copia o duty aplicado no bloco anterior.
Extras (fora das interfaces, cortáveis): `G` (segurança), `L`/`D`, `P60`, `R`, `E`.

**FW7 OLED (Nicolas).** Driver próprio, buffer de 1024 bytes, fonte 6×8 ASCII embutida (21 colunas × 8 linhas), envio por página (0xB0+p, colunas 0, prefixo de dados 0x40), timeout 20 ms por página; erro → `i2c1_recupera()`. Tela a cada 1 s, 8 linhas (textos de 19/11 no `BLUETOOTH_E_TELA.md`, seção 4). Adotado (onda 2): `tela_atualiza` só desenha; `tela_envia_pagina()` manda uma página por bloco de 100 ms (8 páginas em 0,8 s); falha numa página desiste até o próximo desenho. `i2c1_recupera()` (em `ina219.h`) serve aos dois: só no laço, nunca em ISR; o INA219 guarda a configuração e cada página do OLED reenvia o endereço de página e coluna, então nenhum fica inconsistente.

**FW9 BKP (Victor).** `bkp_init`: `__HAL_RCC_PWR_CLK_ENABLE`, `__HAL_RCC_BKP_CLK_ENABLE`, `HAL_PWR_EnableBkUpAccess`. `bkp_restaura`: se `__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)` e `BKP->DR2 == 0xA5A5` → `min = DR1`, `bits = DR3`, `DR4++`, devolve 1; senão 0 (hora fixa). Sempre `__HAL_RCC_CLEAR_RESET_FLAGS()`; só restaura se também DR1 < 1440. Acesso direto a `BKP->DRx` (CMSIS), pois o HAL de RTC está desligado. DR3: bit0 MANUAL, bit1 DEMO, bit2 reservado (G), bits 8..15 = duty manual/10. Botão, queda ou tomada → hora fixa. Energia não vai para a BKP (não usar `W` no meio do T12). IWDG ~2 s nominal (1 a 3 s) é iniciado em `MX_IWDG_Init` (antes do `app_init`: a partida, ~0,3 s com o `HAL_Delay(100)` do OLED, cabe com folga), refresh só no fim do `app_loop`; `__HAL_DBGMCU_FREEZE_IWDG()` para depurar. Medir se o VBAT da Blue Pill está no 3,3 V.

**Config. do HC-05.** `MODO_CONFIG_HC05 = 1`, gravar uma vez: EN/KEY em 3,3 V antes de energizar (nunca 5 V); USART1 a 38400; `AT`, `AT+NAME=<HC05_NOME>`, `AT+PSWD=<HC05_PIN>`, cada um com `\r\n`, polling com timeout; respostas no OLED; depois para piscando o PC13. O laço dá refresh do IWDG (ele não para depois de ligado); o baud é trocado em tempo de execução (`HAL_UART_DeInit`/`Init`), sem tocar no `.ioc`; resposta lida até `
` ou `HC05_TIMEOUT_MS`. Voltar a 0 e regravar.

## 5. Calibração (05 §6.2; sessão 1 do Rafael, 11 a 13/11)

Caminho atual: teste de bancada 9 (`testes_hw/RESULTADOS.md`), que varre a fita de 10 em 10% e sugere `RAW_ESCURO`, `RAW_CLARO`, `D_FIXO`, `KP` e `P100_mW`. Roteiro manual original: 1. LDR tapado → `RAW_ESCURO`. 2. Fita 100%, maquete fechada → `RAW_CLARO` e `P100_mW` (medido no circuito). 3. Cortina fechada, controle estável em `SP_DIA` → duty = `D_FIXO`. 4. Repetir em SP 15, 30, 45: fora da reta → `D_ff` em tabela. `SP_DIA` até ~70% (60 padrão). Usar `S` (campos `raw`, `mW`, `d`).

## 6. Testes de bancada

| Teste | Critério |
|---|---|
| T0 | Blink nas duas C8T6 no dia da solda, foto do chip (06) |
| T1 | Três faixas repetíveis; fita 100% e maquete fechada: ADC ≥ `RAW_ESCURO` + 1000 (06) e < ~3700 (11) |
| T2 | INA219 × multímetro com a fita a 100%, erro ≤ 5% (sem multímetro: 2 × 220 Ω, ±10%); 25/50/75% só pelo INA219, para mostrar a proporcionalidade (06). o teste de bancada 8 (rampa no PA6, `TESTE_HW 8`) ajuda |
| T3 | Programa de luz com rampas no modo demo (detalhe no guia do grupo, B6) |
| T4 | Cortina aberta/fechada 5×: com PI volta a ±5 pontos em poucos segundos sem oscilar; com só P, erro medido e declarado |
| T5 | Noite: inspeção por 15 s e volta; dia: luz não muda; nos dois, registra |
| T6 | Segurança `G` (extra): 10 acionamentos à noite, alerta < 2 s no celular, OLED e PC13; a fita não pisca |
| T7, T8 | Como no guia do grupo (B6); não estão nas fontes lidas |
| T9 | Sai (bônus cortado) |
| T10 | `W` de dia e de noite no demo: reinicia em 1 a 3 s, hora continua (±1 min), luz no estado da fase. Gravação falhando com IWDG: "connect under reset" |
| T11 | Estabilidade de 1 h (corte de emergência: 15 min); detalhe no B6 |
| T12 | Um dia com a cortina fechada e outro aberta: economia contra o timer (teto) e do sensor, coerentes |

## 7. Núcleo e cortes (06 §7)

Núcleo em 17/11: ticks sem `HAL_Delay`; LDR + calibração; PWM → IRLZ44N; programa com rampas (demo); feedforward + P; INA219 com energia e as duas economias; `A M + - S V W` + telemetria (textos e `T`/`?` revistos em 19/11); PIR com inspeção e contagem; OLED simples; IWDG + BKP. Cortes, nesta ordem: 3D; `L`/`D`/`P60`/`R`/`E`; lista dos 10 acionamentos; OLED completo; `G`. Emergência: T11 15 min; inspeção vira só contagem; BKP sai.

## 8. Diferenças

| Assunto | O que as fontes dizem | Adotado e por quê |
|---|---|---|
| Driver do OLED | 05 §6.5: biblioteca afiskon, "sem escrever driver" | Driver mínimo próprio com fonte 6×8 (plano da onda 0): nada a baixar, menos Flash, só o que a tela simples usa |
| Bônus / HC-05 nº 2 / USART2 | 05 tem o bônus e a USART2; 06 o corta (T9 sai) | Sem USART2 nem protocolo `$AVI` (06) |
| Integrador | 05 §6.2/§8: PI, plano B até 10/11; 06 §7: núcleo é feedforward + P, I decidido em 13/11 | `KI = 0`; o código do I com anti-windup fica pronto atrás do `KI` (06) |
| Ordem dos cortes | 05 §8 ≠ 06 §7 (06 põe 3D primeiro e `G` por último) | 06 |
| Corrente e `P100_mW` | 05: fita ~1 A; 11: COB 0,5 a 0,8 A (pior 1,0), `P100_mW` 2000 a 4500; plano inicial: 0,4 a 0,9 A | `P100_mW = 3000` provisório (CALIBRAR), tudo em uint32; `0x17FF` fixo (11 e plano inicial; a troca para `0x1FFF` do 05 só valia acima de 1,5 A) |
| Luz de inspeção | 05: "sobe para 15%"; 11: duty mais baixo com a COB | `DUTY_INSPECAO = 150` permil aplicado direto no PWM (CALIBRAR) |
| T1 | 06: ≥ 1000 contagens acima do escuro; 11: também < ~3700 | Os dois critérios |
| T2 | 05: 25/50/75/100% com multímetro; 06: multímetro só a 100%; 11: a escala de 10 A tem ~9% de incerteza e propõe medir mV no shunt | 06; a proposta do 11 vai para o grupo |
| T10 | 05: reinicia em ≈ 2 s; 06: 1 a 3 s | 06 |
| Unidade da energia | 05 §5: soma `P_mW × 60` (mW·s) | Soma `P_mW` por minuto (mW·min): mesmas razões, uint32 sem estouro por ~660 dias (mW·s estoura em ~11 dias a 4,5 W) |
| Telemetria | 05/06 só citam `ent=3`, `rst=N` e ~40 bytes | Formato da seção 4 (≤ 50 bytes, ≈ 52 ms) |
| Comando `G` | 05 §4 lista com os comandos; plano inicial e 06 §6.4 deixam para o fim | Fora das interfaces; bit 2 da DR3 reservado |
| Dono do relógio | 06 §6.4: "passa do Rafael para o Victor"; plano inicial: Victor já em 15/10 | Victor nos cabeçalhos |
| Dono da config. do HC-05 | Plano inicial (15/10): Rafael; 06 §6.4: Victor | Cabeçalho cita os dois |
| `hc05_cfg.h` | O plano inicial lista só `hc05_cfg.c` | Criado o `.h` com o protótipo (evita declaração solta no `app.c`) |
| `estado_t` em `app.h` | Não previsto no plano inicial | Retrato único para serial e tela: os módulos C e D não dependem dos getters de todos |
| Escalonador | Onda 0: `while` que recupera todos os blocos atrasados ("nenhum tique se perde") | Onda 2: um bloco por volta, atrasados pulados (sem rajada); o relógio do galpão continua sem perder minutos |
| TX da serial | 05/onda 0: bloqueante, timeout 100 ms | Onda 2: por interrupção com fila de 256 (o `S` bloqueava ~160 ms); `UART_TX_TIMEOUT_MS` ficou sem uso |
| Envio do OLED | Onda 0: tela inteira no bloco de 1 s (~75 ms) | Onda 2: uma página por bloco de 100 ms (plano B do 05 §6.5) |

## 9. Decisões que são do grupo

| Decisão | Provisório no código |
|---|---|
| Hora ao ligar na tomada (`HORA_INICIAL_MIN`) | 12:00 (exemplo do 05) |
| Horário do programa e `SP_DIA` | 05:00 a 23:00, rampas de 30 min, SP 60% (exemplo do 05) |
| Modo ao ligar | Demonstração (`MODO_DEMO_PADRAO = 1`) |
| Inspeção: duty fixo ou setpoint 15% pelo controle; vale no MANUAL? | Duty fixo 150; só no AUTO (no MANUAL só conta) |
| Quando zera a contagem de entradas | 00:00 do galpão |
| O que a DR4 conta | Só resets pelo IWDG |
| `M` ao entrar no manual | Mantém o duty atual (sem salto) |
| Energia no MANUAL | Soma normalmente (E_timer/E_fixo seguem o programa) |
| `KP` e o integrador | `KP = 5`, `KP_DIV = 1` (até a simulação do Nicolas; no papel, 5 com `G` = 0,12 dá polo −0,6, oscila ~1 s ao ligar; ~4 é mais folgado); `KI = 0` até 13/11 |
| Energia por minuto | Uma amostra de potência por minuto do galpão: no modo real, uma inspeção curta pode ser perdida ou contada a mais; média por minuto seria o ajuste |
| `D_ff` em reta ou tabela | Reta, até a calibração em SP 15/30/45/60 |
| T2 com mV no shunt (proposta do 11) | Não adotado |
| Nome e PIN do HC-05 | `AVIARIO_LUZ` / `1234` (trocar) |
| Formato da telemetria e do `S` | Seção 4 |
| `RAW_ESCURO`, `RAW_CLARO`, `D_FIXO`, `P100_mW`, `DUTY_INSPECAO` | 200, 3200, 500, 3000, 150 (CALIBRAR) |

## 10. Limitações a declarar (05 §12)

LDR mede nível relativo, não lux; galpão aberto de dia deixa a fita apagada; não se aplica a *dark house* (vale o programa e a medição); sem controle da cortina; PIR detecta movimento, alerta só local (~10 m); protótipo em 5 V; relógio sobrevive ao watchdog, não à tomada; economia contra o timer é teto e os Wh são da maquete; consumo do controlador informado à parte.
