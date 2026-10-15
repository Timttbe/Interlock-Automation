# Simulador

Roda o **firmware real** das 3 placas no PC, numa rede simulada, sem nenhuma placa ligada. Serve
para conferir o intertravamento depois de qualquer mudança no código.

Cada placa é o código de `src/` compilado dentro de um `namespace` próprio (ver `dispositivo.inc`),
com o hardware trocado por versões de mentira (`stubs/`): pinos, relógio, Wi-Fi, UDP e flash
ficam nas mãos do simulador (`sim.cpp`), que aperta botões, abre portas, derruba o Wi-Fi, perde
pacotes, reinicia placas e confere relés e buzzers.

## Rodar

Precisa de um `g++` no PATH (MSYS2/MinGW, ou o que vem com o Questa do Quartus). No Git Bash, na
pasta do projeto:

```bash
export PATH="/c/altera_lite/25.1std/questa_fse/gcc-7.4.0-mingw64vc16/bin:$PATH"
sh simulador/rodar.sh
```

`sh simulador/rodar.sh 7 -v` roda só o cenário 7 e mostra o registro de eventos das 3 placas.

## Cenários

| # | O que confere |
|---|---|
| 0 | As placas se acham na rede e começam travadas |
| 1 | Facial na A libera; B bloqueada com A liberada e com A aberta depois do tempo |
| 2 | A porta trava assim que abre e fecha |
| 3 | Alarme: buzzer na portaria depois de 60 s aberta, desliga ao fechar |
| 4 | Botões da portaria respeitam o intertravamento |
| 5 | Chave de bypass libera as duas |
| 6, 7 | Facial nas duas quase ao mesmo tempo, 60 vezes, sem e com 25% de perda de pacotes |
| 8 | Portaria desligada: as portas se respeitam sozinhas |
| 9 | Portaria acabou de cair: nega até ela ser dada como fora |
| 10 | Sem portaria, facial nas duas no mesmo instante: uma trava de novo em milissegundos |
| 11, 12 | Wi-Fi caiu em todas, regras "liberar" e "bloquear" |
| 13 | Portaria reinicia no meio do uso |
| 14 | Ataque: mensagem "abrir" gravada e reenviada, alterada, e depois de a porta reiniciar |
| 15 | Porta forçada e sensor com fio rompido |
| 16 | Wi-Fi ruim (30% de perda) no uso normal |
| 17 | Contador `millis()` estourando (placa ligada há 49,7 dias) |
| 18 | Sem portaria e com 25% de perda, facial nas duas quase juntos |
| 19 | Botão da portaria contra o facial da outra porta, com 10% de perda |

A assinatura das mensagens no simulador é um hash simples (`stubs/mbedtls/md.h`), não o HMAC-SHA256
da placa: o que se testa é que mensagem alterada ou repetida é recusada.

## Ver as páginas sem placa

```bash
node simulador/paginas/montar.js     # monta as páginas a partir dos .h e confere o JavaScript
node simulador/paginas/servidor.js   # portaria em http://localhost:8471, porta A em http://localhost:8472
```

O servidor imita a API da placa com dados de exemplo (uma porta em alarme, uma placa sem
comunicação, um botão travado) para ver todos os estados da página.
