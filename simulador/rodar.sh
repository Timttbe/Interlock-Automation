#!/bin/sh
# Compila e roda o simulador: o firmware real das 3 placas numa rede simulada, com relógio,
# pinos e perda de pacotes controlados (ver sim.cpp para a lista de cenários).
#
# Precisa de um g++ no PATH (MSYS2/MinGW, ou o que vem com o Questa do Quartus:
#   export PATH="/c/altera_lite/25.1std/questa_fse/gcc-7.4.0-mingw64vc16/bin:$PATH").
#
# Uso, no Git Bash, na pasta do projeto:
#   sh simulador/rodar.sh          todos os cenários
#   sh simulador/rodar.sh 7 -v     só o cenário 7, mostrando o registro de eventos das placas
set -e
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
# Compila numa pasta sem acentos: o g++ do Windows se perde com caminhos como "Códigos".
TMP="${TMPDIR:-/tmp}/interlock-simulador"
rm -rf "$TMP"
mkdir -p "$TMP/proj"
cp -r "$RAIZ/src" "$RAIZ/include" "$TMP/proj/"
cp -r "$RAIZ/simulador/stubs" "$RAIZ/simulador/dispositivo.inc" "$RAIZ"/simulador/*.cpp "$TMP/"
cd "$TMP"

echo "Compilando..."
OBJS=""
for n in porteiro porteiro_r porta_a porta_a_r porta_b; do
  g++ -std=gnu++11 -O1 -Wall -Wextra -Wno-unused-parameter -Istubs -Iproj/include -Iproj/src -c "dev_$n.cpp" -o "dev_$n.o"
  OBJS="$OBJS dev_$n.o"
done
g++ -std=gnu++11 -O1 -Wall -Istubs -Iproj/include -c sim.cpp -o sim.o
g++ sim.o $OBJS -static -o simulador

if [ -n "$1" ]; then
  ./simulador "$@"
  exit $?
fi
N=$(./simulador)
FALHAS=0
i=0
while [ "$i" -lt "$N" ]; do
  ./simulador "$i" || FALHAS=$((FALHAS + 1))
  i=$((i + 1))
done
echo
if [ "$FALHAS" -eq 0 ]; then echo "Todos os $N cenários passaram."; else echo "$FALHAS de $N cenários com falha."; exit 1; fi
