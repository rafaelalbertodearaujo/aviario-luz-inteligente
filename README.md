# Programa de Luz Inteligente para Aviários

Projeto final de Microprocessadores (Engenharia da Computação, Unoesc Joaçaba, 2026/2), de Rafael, Victor e Nicolas.

Um STM32F103C8T6 (Blue Pill) segue o programa de luz de um galpão de frango de corte, com amanhecer e anoitecer graduais, e complementa a luz que entra pela janela com um controle feedforward + P + I, lendo um LDR. Mede a energia da fita LED com um INA219 e compara com um timer comum. Tem tela OLED de 4 páginas, comandos e resumo por Bluetooth (HC-05) e um sensor de presença (PIR) que acende uma luz baixa de inspeção à noite. A demonstração usa uma maquete 1:25.

## Pastas

| Pasta | O que tem |
|---|---|
| `Core/` | Firmware (STM32CubeIDE, HAL): `app`, `controle`, `programa`, `luz`, `ldr`, `ina219`, `pir`, `serial`, `tela`, `ssd1306`, `bkp`, `config.h` |
| `Drivers/` | HAL e CMSIS da ST |
| `testes_pc/` | Testes da lógica no computador, sem a placa (`sh testes_pc/rodar.sh`, precisa do gcc) |
| `testes_hw/` | Scripts de teste de bancada e `RESULTADOS.md` |
| `simulacao/` | Circuito de potência no Falstad |
| `fotos/` | Montagem, maquete e tela |
| `BLUETOOTH_E_TELA.md` | Manual de uso: comandos, telas do OLED e roteiro da demonstração |
| `SPEC_FIRMWARE.md` | Especificação do firmware |
| `aviario_luz.ioc` | Configuração do STM32CubeMX |

## Compilar e gravar

Abra a pasta como projeto no STM32CubeIDE (Import > Existing Projects into Workspace), compile e grave pelo ST-Link (SWD). Todas as constantes de calibração estão em `Core/Inc/config.h`.

## Testes do computador

```
sh testes_pc/rodar.sh            # 63 testes com o config.h real (KI = 2)
sh testes_pc/rodar.sh cfg_ki     # integrador ligado
```

O firmware gravado na placa na apresentação (25/11/2026) corresponde ao código deste repositório.
