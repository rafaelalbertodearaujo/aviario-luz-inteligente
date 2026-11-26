# Manual de uso: celular, tela do OLED e luz acima de 100%

Versão de 25/11/2026, para o firmware gravado na placa em 25/11 (commits `01cac6f` e `118d3ec` do repositório `aviario_luz`: comandos `F` e `L`, inspeção gradual do PIR, `KI` 2 e a calibração final; textos de 19/11, páginas do OLED e comando `P`; `Core/Src/serial.c` e `Core/Src/tela.c`). Serve para o grupo, para a apresentação e para o relatório. Os textos foram conferidos rodando o código real no PC (`testes_pc/rodar.sh`, programa `mostra_telas`); na placa, em 20/11, conferidos pelo Bluetooth do notebook os comandos `?`, `2`, `P`, `T` e `1`; em 25/11, conferidos por Bluetooth os comandos `?`, `T`, `M`, `+` (e o erro "mande M antes"), `S`, `A`, as telas 1 a 3, `F` e `L`. **PIR à noite testado na placa em 25/11** (firmware final, commit `6836dcb`), com o PIR solto e o relógio congelado por `F`: a fita sobe a 15% em ~3 s, cada passada reinicia a contagem e a fita apaga ~15 s depois da última; **a repetir depois de fixar o PIR** em cima da porta (números no documento 25, seção 4.7). **Não testados na placa:** o PIR fixado na posição final, a tela 4 e o painel com a placa real.

## 1. Ligar o celular

1. Ligue a placa (ST-Link no USB). O LED do HC-05 (módulo Bluetooth) pisca rápido: ninguém conectado.
2. No celular, procure dispositivos Bluetooth e pareie com o módulo. De fábrica o nome é `HC-05`; depois da configuração da seção 8, é `AVIARIO_LUZ`. PIN de fábrica: 1234 (às vezes 0000).
3. Abra um aplicativo de terminal Bluetooth (no Android, por exemplo, o "Serial Bluetooth Terminal") e conecte. O LED do HC-05 passa a dar **2 piscadas lentas a cada 2 s**: conectado.
4. Digite uma letra e envie. Maiúscula ou minúscula tanto faz; o fim de linha que o aplicativo põe e os espaços são ignorados.
5. **Comece mandando `?`**: a placa responde com a lista de comandos.

O HC-05 aceita uma conexão por vez: com o celular conectado, o notebook não conecta, e vice-versa.

## 2. Comandos que você pode mandar

| Letra | O que faz | O que a placa responde |
|---|---|---|
| `?` (ou `H`) | Mostra a lista de comandos | `Comandos (letra + enviar):` e uma linha por comando |
| `A` | **Automático:** a placa segue o programa de luz e corrige a fita pela luz que o LDR (sensor de luz) mede | `OK A: automatico, segue o programa de luz` |
| `M` | **Manual:** congela a lâmpada no valor que ela tinha | `OK M: manual, lampada 64%. Use + e -` |
| `+` / `-` | No manual, sobe ou desce a lâmpada 10% | `OK +: lampada 74%`. No automático: `ERRO +: mande M antes (+ e - no manual)` |
| `V` | Troca o relógio do galpão entre **acelerado** (demonstração: um dia em ~5 min) e **real** | `OK V: relogio acelerado (demonstracao)` ou `OK V: relogio real` |
| `F` | **Congela** e retoma o relógio do galpão (apresentar sem pressa; segurar a noite para o PIR). Parado, o relógio não acumula tempo; mandar `F` de novo retoma | `OK F: relogio parado` ou `OK F: relogio andando` |
| `L` | **Timer comum:** vai para o manual com a lâmpada a 100% (fita no máximo, como um timer que não olha o sol). `A` volta ao automático | `OK L: manual, lampada 100% (timer comum)` |
| `T` | Liga ou desliga o **resumo de 5 s** (para digitar sem a tela rolando). Os avisos continuam. No painel (`P`), `T` sai do painel e volta ao resumo ligado | `OK T: resumo desligado (avisos seguem)` ou `OK T: resumo de 5 s ligado` |
| `S` | **Números brutos** para a calibração (seção 3.3) | 7 linhas, começando por `Calibracao (numeros brutos):` |
| `P` | **Painel do notebook:** troca o resumo de 5 s por uma linha por segundo no formato que o painel do hub lê (`hora=12:40 modo=AUTO demo=1 alvo=60 luz=59 lamp=19 mW=241 mA=51 eco=35 ent=7 insp=0`; `insp` = segundos que faltam da luz de inspeção). O painel manda `P` sozinho ao conectar. `T` volta ao resumo | `OK P: painel ligado (T volta ao resumo)` |
| `1` `2` `3` `4` | Troca a **página do OLED** (seção 4): 1 resumo, 2 controle, 3 energia, 4 comandos. Na troca aparece um rosto feliz por ~1 s. A página escolhida fica até a próxima troca | `OK 2: tela CONTROLE` |
| `W` | Trava a placa de propósito para testar o watchdog (IWDG, o vigia que reinicia a placa se o programa travar). **Não aparece no `?` e não deve ser usado na apresentação** | `OK W: travando para testar o watchdog`; a placa reinicia em ~2 s |
| outra letra | nada | `ERRO x: comando desconhecido, mande ?` |

O `?` responde exatamente assim:

```
Comandos (letra + enviar):
A  automatico: programa + sensor
M  manual: lampada fixa
+  -  manual: lampada +10% / -10%
S  numeros brutos (calibracao)
V  relogio real / acelerado
F  congela / retoma o relogio
L  lampada 100% (timer comum)
T  liga/desliga o resumo de 5 s
P  painel do notebook (1 linha/s)
1 2 3 4  tela: resumo, controle,
         energia, comandos
?  esta ajuda
```

A placa lembra o modo (automático ou manual), a lâmpada do manual e a hora se o watchdog a reiniciar (registradores de backup). Ao tirar da tomada, volta ao automático, às 12:00, com o relógio acelerado.

## 3. O que chega no celular

### 3.1 Resumo a cada 5 s

```
--- 05:51 dia, relogio acelerado ---
AUTO: segue o programa de luz
Luz: alvo 60%, medida 32%
Lampada 64% (1,25 W, 250 mA)
Economia contra timer: 35%
Entradas no PIR hoje: 3
Escurece as 22:30
```

| Linha | O que quer dizer |
|---|---|
| `--- 05:51 dia, relogio acelerado ---` | Hora do galpão; `dia` ou `noite` (pelo programa de luz); relógio `acelerado` (demonstração) ou `real` |
| `AUTO: segue o programa de luz` | O modo e o que ele está fazendo. Outras: `AUTO: noite, fita apagada`; `MANUAL: lampada fixa (+ e - mudam)`; `INSPECAO: PIR viu alguem, faltam 12 s` |
| `Luz: alvo 60%, medida 32%` | **Alvo:** a luz que o programa quer agora (sobe e desce nas rampas). **Medida:** o que o LDR vê. 100% = fita no máximo com a maquete fechada (seção 7) |
| `Lampada 64% (1,25 W, 250 mA)` | Quanto a placa manda a fita acender (duty do PWM, a fração do tempo em que a fita fica ligada) e a potência e a corrente que o INA219 (sensor de corrente) mede. Sem o sensor: `(INA219 sem leitura)` |
| `Economia contra timer: 35%` | Quanto de energia o sistema gastou a menos que um timer comum, que deixaria a fita a 100% durante todo o horário de luz do programa (05:00 às 23:00), sem olhar o sol. `ainda sem medida` logo depois de ligar |
| `Entradas no PIR hoje: 3` | Quantas vezes o PIR (sensor de presença da antessala) viu alguém desde a meia-noite do galpão |
| `Escurece as 22:30` | O próximo passo do programa: `Acende as 05:00`, `Sobe ate 05:30`, `Escurece as 22:30` ou `Apaga as 23:00` |

### 3.2 Avisos na hora

Saem assim que acontecem, com `>>` na frente, mesmo com o resumo desligado (`T`):

| Aviso | Quando |
|---|---|
| `>> Amanheceu: luz sobe ate 05:30` | 05:00: começa a rampa de subida (a luz sobe aos poucos por 30 min do galpão) |
| `>> Dia pleno: alvo 60%` | 05:30: fim da rampa; alvo fixo |
| `>> Anoitecer: luz desce, apaga 23:00` | 22:30: começa a rampa de descida |
| `>> Noite: fita apagada ate 05:00` | 23:00: noite, fita apagada |
| `>> PIR: entrada (4 hoje), inspecao 15 s` | O PIR viu alguém à noite, no automático: a fita sobe até 15% em ~3 s, fica por 15 s (cada nova passada reinicia a contagem) e desce em ~3 s até apagar. De dia ou no manual, só conta: `>> PIR: entrada (4 hoje)` |
| `>> PIR: fim da luz de inspecao` | Acabaram os 15 s (ou amanheceu, ou alguém passou para o manual) |

O PIR ignora o primeiro minuto depois de ligar (aquecimento do sensor) e espera 2 s entre uma entrada e outra.

### 3.3 Comando `S`: números brutos (calibração)

```
Calibracao (numeros brutos):
LDR raw=2150 (0-4095) luz=32% alvo=60%
PWM duty=640/1000 modo=A relogio=D
INA219 ok=1 4810 mV 250 mA 1250 mW
Energia mWh real=650 timer=1000 fixo=800
Economia timer=35.0% sensor=18.7%
Entradas=3 resets=0
```

- `raw`: leitura do ADC (conversor analógico-digital) no LDR, de 0 a 4095. É dela que saem `RAW_ESCURO` e `RAW_CLARO` do `config.h`.
- `duty`: a lâmpada em milésimos (640 = 64,0%). `modo`: A automático, M manual, I inspeção. `relogio`: D demonstração, R real.
- `INA219`: `ok=1` lendo; tensão, corrente e potência da fita.
- `Energia mWh`: energia gasta de verdade (`real`), a que um timer gastaria (`timer`) e a que o programa gastaria sem o sensor de luz (`fixo`).
- `Economia`: contra o timer e a do sensor (quanto o LDR economizou sozinho, aproveitando a luz da janela; pode dar negativa).
- `resets`: quantas vezes o watchdog reiniciou a placa.

## 4. A tela do OLED

Oito linhas de 21 letras; as duas de cima ficam na faixa amarela e são iguais em todas as páginas. Sem acentos (a fonte só tem ASCII). O celular escolhe a página com `1` a `4` (ou os botões OLED do painel). O `1/4` no canto da primeira linha é o **número da tela aberta**: tela 1 de 4 (1 resumo, 2 controle, 3 energia, 4 comandos). A tela só muda por esses comandos, não troca sozinha. Na troca, a tela mostra um rosto feliz e o nome da página por ~1 s.

**Página 1, resumo** (a que liga):

```
05:51  DIA   1/4 DEMO
AUTO: segue programa
Alvo 60%  Medida 32%
Lampada 64%   1,25 W
Economia 35% vs timer
Entradas PIR hoje: 3
Escurece as 22:30

```

A última linha (a 8) fica vazia: o LDR bruto está na página 2 e os comandos, na página 4.

| Linha | O que mostra |
|---|---|
| 1 | Hora do galpão, `DIA` ou `NOITE`, o número da tela aberta (`1/4`: tela 1 de 4) e `DEMO` com o relógio acelerado |
| 2 | O modo e o que ele faz: `AUTO: segue programa`, `NOITE: fita apagada`, `MANUAL: +/- no cel` ou `INSPECAO PIR: 12 s` (contagem regressiva da luz de inspeção) |
| 3 | Alvo do programa e luz medida pelo LDR, em % |
| 4 | Lâmpada (quanto a fita está acesa) e a potência medida pelo INA219, em W; `sem INA` se o sensor não responde |
| 5 | Economia contra um timer comum; `--` logo depois de ligar. Se ficar negativa (gastou mais que o timer), aparece `Econ. -20% vs timer` |
| 6 | Entradas na antessala desde a meia-noite (PIR) |
| 7 | O próximo passo do programa de luz e a hora |

**Página 2, controle** (o laço da luz e os números da calibração):

```
12:40  DIA   2/4
AUTO: segue programa
CONTROLE DA LUZ
Alvo 60%  Medida 60%
Erro 0 pontos
Lampada 16,6%
KP 1  KI 2  Dfix 14%
LDR 1969 (4-3280)
```

`Erro` é alvo menos medida, em pontos de %. `KP`, `KI` e `Dfix` (o `D_FIXO`, ponto de partida da lâmpada) são os ganhos do `config.h`; a última linha é o LDR bruto (0 a 4095: serve para calibrar e para ver na hora se o fio do LDR soltou, porque cai para 0) e a faixa calibrada (`RAW_ESCURO` a `RAW_CLARO`). O exemplo usa as constantes finais de 25/11 e a lâmpada medida no automático (16,6%); o valor do LDR (1969) é a conta da fórmula para 60% de luz, não uma leitura da placa.

**Página 3, energia** (INA219): tensão, corrente e potência da fita, energia gasta hoje em mWh e a economia contra o timer. Sem o sensor, a linha da tensão vira `INA219 sem leitura`.

**Página 4, comandos:** a lista curta dos comandos do celular, para quem está olhando a maquete.

A tela é redesenhada a cada 1 s e enviada uma linha a cada 100 ms, para não atrasar o controle da luz.

## 5. As luzes da placa

| Luz | O que quer dizer |
|---|---|
| LED da Blue Pill (PC13), firmware normal | Pisca devagar (1 s aceso, 1 s apagado): o programa está rodando. Parado aceso ou apagado: travou (o watchdog reinicia em ~2 s) |
| PC13 nos testes de bancada | Aceso fixo: rodando; piscando 1 vez por segundo: OK; piscando rápido: FALHA (`testes_hw/RESULTADOS.md`) |
| LED do HC-05 | Pisca rápido: sem celular; 2 piscadas a cada 2 s: conectado |

## 6. Se acontecer...

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Nada chega no celular | Não conectado (LED do HC-05 piscando rápido) | Conectar de novo no aplicativo; só um aparelho por vez |
| Chega texto estranho | Aplicativo em outra velocidade | Usar o Bluetooth clássico (SPP), 9600 |
| A tela rola demais para digitar | Resumo de 5 s | Mandar `T` |
| `ERRO +: mande M antes` | `+` e `-` só valem no manual | Mandar `M`, depois `+` ou `-`; `A` volta ao automático |
| Linha 8 da página 2 (controle) mostra `LDR    0` com luz | Fio do LDR solto (mau contato) | Firmar a emenda; no automático o controle acha que está escuro e acende a fita demais |
| `sem INA` na tela ou `INA219 sem leitura` | Sensor de corrente sem resposta | Conferir os fios do I2C (PB6 e PB7) e reiniciar |
| Tela apagada | OLED sem resposta na partida | Conferir os fios e reiniciar (o resto funciona sem a tela) |
| A luz não muda ao abrir a janela | Modo manual ou noite | Ver a linha 2 da tela; mandar `A` |

## 7. A luz medida passou de 100%: não é erro

A escala da luz é a da própria maquete: **0%** = LDR tapado (`RAW_ESCURO`); **100%** = fita a 100% dentro da maquete fechada (`RAW_CLARO`). A conta é `luz = (leitura − RAW_ESCURO) × 100 / (RAW_CLARO − RAW_ESCURO)`. O sol pela janela ou a lanterna do celular dão mais luz que a fita, e o número passa de 100% (até 200%, `LUZ_MAX_PCT`). É exatamente o caso em que o sistema deve apagar a fita: com a luz acima do alvo, o controle reduz a lâmpada até 0%.

Calibração final de 25/11, na maquete fechada, pelo teste de bancada 9: `RAW_ESCURO` = 4 e `RAW_CLARO` = 3280 (era 3009 em 20/11), `D_FIXO` = 145, `KP` = 1, `KI` = 2 e `P100_mW` = 1426 (resultados na seção 4 do documento 25 do hub e em `testes_hw/RESULTADOS.md`). Com `KP` 1 e `KI` 2 o automático assentou em 60% de luz com duty de 16,6%, 48 mA e 245 mW. Se o LDR ou a fita mudarem de lugar, rodar o teste 9 de novo (40 s).

## 8. Configuração única do HC-05 (nome e PIN)

Só uma vez por módulo, para ele aparecer como `AVIARIO_LUZ` com um PIN do grupo e não confundir com os módulos das outras equipes.

1. Em `Core/Inc/config.h`: troque o PIN em `HC05_PIN` por um do grupo (não publique o PIN no relatório) e ponha `MODO_CONFIG_HC05` em `1`.
2. Com tudo desligado, ligue o pino EN/KEY do HC-05 no 3,3 V (**nunca 5 V**). O módulo liga em modo AT (comandos de configuração), a 38400.
3. Grave e ligue a placa. O firmware manda `AT`, `AT+NAME=AVIARIO_LUZ` e `AT+PSWD=<PIN>`, mostra as respostas no OLED e fica piscando o PC13.
4. Desligue, tire o fio do EN/KEY, volte `MODO_CONFIG_HC05` para `0` e grave de novo o firmware normal.

## 9. Roteiro técnico da demonstração (~4 min)

Base para o roteiro da apresentação (Nicolas). Rascunho de 20/11, **a ensaiar com a placa e cronometrar**.

**A ideia:** ligar (ou dar reset) bem na hora de começar. O galpão começa às **12:00** com o relógio acelerado, e o próprio programa de luz cria a sequência, sem ajustar hora: dia (~1 min 50 s), anoitecer (30 s), noite (~1 min), amanhecer (30 s). O PIR aquece em 60 s, então já está pronto quando a noite chega.

**Montagem antes de começar:**
- Notebook com o HC-05 pareado e o painel do hub aberto no Chrome ou Edge (`guia-3d/painel/`), no telão. **Conectar a placa** (porta COM11 neste notebook). O celular fica desconectado (o HC-05 aceita uma conexão por vez). Plano B sem site: PuTTY na mesma porta (seção 11).
- Maquete fechada, OLED à vista, lanterna do celular à mão, antessala livre (ninguém passando no PIR).
- No painel, o "sol" será a lanterna pela janela; ensaiar antes qual janela e a que distância o LDR, no meio do galpão, sente melhor.

| Tempo | Galpão | O que fazer | O que a turma vê (telão e maquete) |
|---|---|---|---|
| 0:00 | 12:00, dia | Reset da placa (botão RESET). Apresentar o telão: "o que o programa quer, o que o sensor mede, o que a placa coloca" | Luz desejada 60%, luz medida ~60%, a placa coloca ~17% (16,6% medido em 25/11). OLED na página 1 |
| 0:20 | dia | **Lanterna pela janela** ("sol entrando") | Luz medida sobe, **luz que a placa coloca desce**, consumo cai, economia sobe. No gráfico, a linha pontilhada desce |
| 0:50 | dia | Tirar a lanterna | A placa volta a completar sozinha |
| 1:05 | dia | Botões **Manual** e **Lâmpada +10%** duas vezes ("o tratador força a luz") | Modo Manual, a placa coloca mais, a luz medida acompanha. **Automático** para voltar |
| 1:30 | dia | Botões **OLED 2** e **OLED 3** | Rosto feliz no OLED, páginas de controle (alvo, erro, KP) e de energia (W, mA, mWh) |
| ~1:50 | 22:30, anoitecendo | Narrar o programa de luz | Luz desejada desce em rampa; a fita acompanha. Cartão da hora: "Anoitecendo: apaga às 23:00" |
| ~2:20 | 23:00, noite | Narrar: "aves dormem, fita apagada" | Luz desejada 0%, a placa coloca 0%, consumo 0 W. Modo: "noite: as aves dormem" |
| ~2:30 | noite | **Passar a mão no PIR** da antessala | Entradas +1, modo **Inspeção**, "luz de inspeção acesa: faltam 15 s" contando; fita a 15% |
| ~3:30 | 05:00, amanhecendo | Fechamento: economia contra o timer | Rampa de subida; cartão da hora "Amanhecendo" |

**Se algo sair do previsto:** sem dados no telão por mais de 3 s, o painel avisa; desconectar e conectar de novo. O `KI = 2` está gravado. Em 25/11 foi testado só com a janela meio aberta (luz de fora de +4 a +9 pontos): a luz foi de 52% a 67% e voltou a 59 a 60% em ~10 s, sem oscilar. **Lanterna encostada não foi testada:** se a luz ficar muito acima da desejada ou oscilar com a lanterna, afastar a lanterna.

**Comparativo com o timer comum (medido em 25/11):** `L` (fita a 100%, como um timer) deu 295 mA e 1,44 W; `A` (automático) deu 48 mA e 0,245 W com 60% de luz, ou seja, 17% do consumo. `F` congela o relógio para falar sem pressa. Em 24/11 o painel ainda não tinha botões para `F` e `L`: só pelo celular ou pelo PuTTY (a conferir se isso mudou).

Botão `W` (watchdog) **nunca** na apresentação.

## 10. Por que foi montado assim

- **HC-05 no USART1 (PA9 e PA10).** É a serial livre com interrupção própria. PA9 (TX da placa) vai no RXD do módulo e PA10 (RX) no TXD: a UART é cruzada. O RXD do HC-05 aceita 3,3 V, então o PA9 liga direto, sem divisor.
- **Envio por interrupção, com fila de 512 bytes.** Esperar a serial terminar custava ~50 ms por linha a 9600 baud, metade do laço de controle de 100 ms. Agora a linha vai para uma fila e a interrupção manda byte a byte, sem parar o controle.
- **Resumo a cada 5 s em frases curtas e avisos na hora.** Uma linha abreviada por segundo enchia a tela do celular e não se explicava. Linhas de até ~40 letras cabem no celular em pé.
- **Recepção por interrupção, um byte por vez**, guardado numa fila e tratado no laço principal: um comando nunca interrompe o controle no meio.
- **Tela enviada uma linha por bloco de 100 ms** (~12 ms de I2C por linha), para o OLED não atrasar o controle.
- **Só contas inteiras.** O firmware não usa `float` para formatar texto; o duty é guardado em milésimos (640 = 64,0%) e os watts saem de mW divididos por 1000.

## 11. Painel no notebook

O HC-05 é Bluetooth clássico; o Bluetooth do navegador (Web Bluetooth) só fala com o de baixo consumo (BLE). O caminho é parear o HC-05 no Windows: ele vira uma porta COM, que o Chrome ou o Edge abrem pelo Web Serial. Funciona só no computador.

1. Windows: Configurações > Bluetooth > Adicionar dispositivo > o HC-05 (aparece como `HC-05`: o nome `AVIARIO_LUZ` da seção 8 nunca foi gravado), PIN do `config.h`. Emparelhado não é conectado: o LED do módulo só para de piscar rápido quando um programa abre a porta. O Windows cria portas COM; a de saída é a certa (no notebook do Rafael, **COM11**).
2. Abrir o painel do hub (`guia-3d/painel/`) no Chrome ou no Edge e clicar em **Conectar a placa**. O painel manda `P` e os cartões e o gráfico passam a mexer a cada segundo.
3. **O HC-05 aceita uma conexão por vez:** com o notebook conectado, o celular não conecta. Os comandos (automático, manual, +/-, telas do OLED) ficam nos botões do painel.
4. **Plano B, PuTTY** (sem site e sem internet): em *Session*, *Connection type* **Serial**, *Serial line* **COM11**, *Speed* **9600** (em *Connection > Serial*: 8 bits, 1 stop, sem paridade, *Flow control* None). Salvar como `Aviario`. Mostra o mesmo texto do celular; os comandos são as letras do teclado. Não mandar `P` no PuTTY (é a linha para o site; `T` volta ao normal).
