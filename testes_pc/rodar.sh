#!/bin/sh
# Compila programa.c e controle.c REAIS com o MinGW e roda o teste
# (o ldr.c entra por #include dentro do teste_logica.c).
# Argumento opcional: pasta com um config.h alternativo (ex.: cfg_ki, KI ligado).
# Sem argumento, roda tambem o mostra_telas (tela.c e serial.c reais: o que
# aparece no OLED e no celular, com conferencia da largura das linhas).
cd "$(dirname "$0")"
CC=/c/MinGW/bin/gcc
EXTRA=""
[ -n "$1" ] && EXTRA="-I$1"
$CC -std=gnu99 -Wall -Wextra $EXTRA -Istub -I../Core/Inc teste_logica.c ../Core/Src/programa.c ../Core/Src/controle.c -o teste_logica.exe && ./teste_logica.exe || exit 1
[ -n "$1" ] && exit 0
$CC -std=gnu99 -Wall -Wextra -Istub -I../Core/Inc mostra_telas.c ../Core/Src/tela.c ../Core/Src/serial.c ../Core/Src/programa.c -o mostra_telas.exe && ./mostra_telas.exe
