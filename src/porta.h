#pragma once
// Lógica de uma porta (PORTA_A ou PORTA_B). Ver porta.cpp.

#include "papel.h"
#ifdef PAPEL_E_PORTA

#include <Arduino.h>
#include "entradas.h"
#include "intertravamento.h"

// Último pedido de abertura decidido (para a página).
struct ResultadoAbertura {
  bool existe = false;
  bool liberou = false;
  Motivo motivo = Motivo::LIBERADO;
  Origem origem = Origem::DESCONHECIDA;
  bool pelaPortaria = false;  // quem decidiu: a portaria, ou a própria porta (portaria sem comunicação)
  unsigned long quando = 0;
};

struct EstadoPorta {
  bool liberada = false;  // eletroímã solto
  unsigned long liberadaEm = 0;
  unsigned long duracaoMs = 0;
  bool comBypass = false;  // liberada com o bypass ligado
  Origem origem = Origem::DESCONHECIDA;
  bool alarme = false;  // aberta há mais que o tempo de alarme
  char modo = 'I';      // C = com a portaria, P = sem a portaria (decide com a outra porta), I = isolada
  bool pedidoPendente = false;
  ResultadoAbertura resultado;
  // O que esta porta sabe das outras placas
  bool portariaComunicando = false;
  bool bypassPortaria = false;
  VisaoPorta outra;
};

// Pede para abrir, igual ao facial e ao botão (usado pelo botão da página).
void pedirAbertura(Origem origem, unsigned long agora);

const EstadoPorta& estadoPorta(unsigned long agora);
unsigned long restanteLiberacaoMs(unsigned long agora);
const Entrada& sensorDaPorta();
const Entrada& facialDaPorta();
const Entrada& botaoDaPorta();
const Saida& imaDaPorta();
const Saida& sinaleiraDaPorta();

#endif
