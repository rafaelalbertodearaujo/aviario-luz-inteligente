# Testes de bancada por componente

Um teste por vez, escolhido por `TESTE_HW` em `Core/Inc/config.h` (0 = firmware normal). Código em `Core/Src/teste_hw.c`; o resultado fica na variável `teste_res` da RAM.

**Pelo script** (compila, grava pela ST-Link e lê a placa; deixa o `config.h` em 0 no fim):

```
powershell -ExecutionPolicy Bypass -File testes_hw\bancada.ps1 -Teste 2
```

`-SoLer` só lê a placa já gravada; `-Segundos 120` monitora por mais tempo; `-Teste 0` grava o firmware normal. Precisa da ST-Link no USB e de nenhuma sessão de debug aberta no CubeIDE.

**Pelo CubeIDE:** troque `TESTE_HW`, grave em Debug e ponha `teste_res` no Live Expressions. Volte para 0 antes de commitar.

PC13: aceso fixo = rodando; pisca 1 Hz = OK; pisca rápido = FALHA.

| Nº | Passo do guia 3D | O que fazer durante o teste | OK quando |
|---|---|---|---|
| 1 | 6 a 8: Blue Pill + ST-Link | nada | Flash ≥ 64 kB (C8T6; o C6T6 dá 32) |
| 2 | 9: LDR no A0 | tapar o LDR com o dedo, depois a lanterna do celular | faixa (máx − mín) ≥ 1000, com mín < 1000 e máx > 2000; 5 s preso em 4095 = FALHA (curto no 3V3). Perto de 0 com o LDR tapado é normal; se não subir com luz, o LDR ou o fio está aberto ou o LDR está no GND |
| 3 | 10: PIR no A1 | esperar ~1 min de aquecimento, passar a mão 3 vezes | 3 contagens depois dos 60 s de aquecimento; o PC13 acende junto com o PIR |
| 4 | 11: HC-05 no A9/A10 | parear no celular (app de terminal Bluetooth), ver os PING e mandar uma letra | volta `ECO x` e bytes_rx > 0 |
| 5 | 12: OLED no B6/B7 | olhar a tela | 0x3C responde, texto com contador, sem falha de envio |
| 6 | 13: INA219 (dados) | nada (sem potência, Vbus e I perto de 0 é normal) | 0x40 responde e lê |
| 7 | qualquer: varredura I2C | nada | lista 0x3C e 0x40 |
| 8 | Fase 2: teste com LED | olhar o LED no PA6 | LED sobe em 4 s e apaga; com INA219, corrente sobe com o duty |
| 9 | Calibração (maquete fechada, LDR na posição final) | nada: não abrir nem mexer na maquete por 40 s. Rodar com `-Teste 9 -Segundos 40 -Intervalo 1` | a fita sobe de 10 em 10% (2,5 s cada); no fim o script mostra a curva e sugere `RAW_ESCURO`, `RAW_CLARO`, `D_FIXO`, `KP` e `P100_mW`, com avisos (saturação acima de 3700, faixa abaixo de 1000, ruído acima de 150, raw 0 com a fita acesa) |

## Resultados

| Data | Nº | Valores | Resultado | Obs. |
|---|---|---|---|---|
| 15/11/2026 | 2 | mín 55, máx 4031 (raw), faixa 3976; máx ≈ 3,25 V; s_no_extremo 0 | OK | LDR tapado ≈ 55 a 80, lanterna encostada ≈ 4000; luz ambiente ≈ 1900. Medidas 9.1 a 9.4 antes, ok |
| 15/11/2026 | 3 | contagem do pir.c = 3 aos 85 s (4 aos 89 s); bordas 22 | OK | Parado depois do aquecimento: sem borda por 14 s. Potenciômetro de tempo no mínimo. Antes dos 60 s, disparos soltos (aquecimento/movimento) |
| 15/11/2026 | 4 | pings 78, bytes_rx 10, erros_tx 0, erros_rx 0 | OK | Celular (app de terminal) viu os PING subindo e a placa recebeu os bytes enviados. Último byte 13 (CR): o app manda fim de linha junto da letra |
| 15/11/2026 | 7 | 0x3C e 0x40 (quantos 2) | OK | INA219 na 830 em a60 a a63 (VCC na 63, deslocado 2 colunas do guia; 3D ajustado) |
| 15/11/2026 | 5 | achou_0x3C 1, init_ok 1, envios_ok sobe, falha 0 | OK | Texto na tela a conferir olhando |
| 15/11/2026 | 6 | achou_0x40 1, leitura_ok 1, Vbus 868 mV, I 0 mA, falhas 0 | OK | Sem potência. Vbus 868 mV com o borne solto é entrada flutuando |
| 18/11/2026 | 8 | I a 100% ≈ 13 mA (P ≈ 69 mW, ≈ 5,3 V), a 50% ≈ 6 mA, I_max 14; corrente proporcional ao duty | OK | Passos 18 e 19 do guia 3D (LED de teste com 220 Ω, carregador no VIN+). Sem o carregador na tomada, I = 0 (esperado). Antes do reset o INA219 não respondeu (tem_INA 0): o teste só procura o 0x40 ao ligar; depois de reconectar e reiniciar, 1 |
| 18/11/2026 | 8 | Fita COB (passo 20): 100% ≈ 285 mA, 1366 mW (≈ 4,8 V); 50% ≈ 134 mA, 660 mW; 25% ≈ 61 mA; I_max 301; 2 min estável | OK | Corrente proporcional ao duty. Abaixo da estimativa de 0,4 a 0,9 A do PLANO_MONTAGEM_COB; M5 (direto no carregador) não anotado, comparação a fazer. P100_mW medido ≈ 1370 contra 3000 no config.h. OLED apagado é normal no teste 8 (não usa a tela) |
| 18/11/2026 | 0 | Firmware normal, manual, leitura por `le_estado.ps1`. Fita 100%: 303 mA, 1445 mW. LDR encostado na fita: 100% ≈ 3650 raw, 50% (coberto) ≈ 3585, apagada com luz da sala ≈ 2150, apagada coberta 0 | INCOMPLETO | Calibração provisória (moletom) interrompida: o LDR cai para exatamente 0 quando é mexido (luz da sala, coberto a 100% e afastado 10 a 15 cm a 50% deram 0). Mau contato no circuito do LDR (PA0 fica só com o 10 kΩ ao GND). Encostado na fita ele também satura: acima de ~30% de duty quase não muda |
| 20/11/2026 | 9 | Maquete fechada: raw 0% = 4, 10% = 1239, 20% = 1828, 50% = 2574, 100% = 3009; ruído 2 a 7; fita a 100% = 256 mA, 1152 mW | OK | Sugestão: RAW_ESCURO 4, RAW_CLARO 3009, D_FIXO 196, KP 3 (adotado KP 2: ganho 0,41 %/permil entre 0 e 10% de duty daria KP*G 1,23 nas rampas), P100_mW 1152. LDR firme (sem zeros). Antes: ST-Link esquentou com o GND ligado no 3,3 V dela (corrigido) |
| 20/11/2026 | 0 | AUTO calibrado, 30 s, maquete fechada, demonstração 12:27 a 15:18: luz 59 a 60% (alvo 60), duty 19,6 a 19,8%, 51 mA, 241 mW | OK | Assenta no alvo sem oscilar. Falta: perturbação (abrir a maquete) e KI = 2 |
