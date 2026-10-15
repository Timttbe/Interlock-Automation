#pragma once
// Lógica da portaria (PORTEIRO). Ver porteiro.cpp.

#include "papel.h"
#ifdef PAPEL_PORTEIRO

#include <Arduino.h>
#include "entradas.h"
#include "intertravamento.h"
#include "protocolo.h"

// Último pedido ou comando de abertura de uma porta (para a página).
struct ResultadoComando {
  bool existe = false;
  bool liberou = false;
  bool aguardando = false;  // a portaria liberou e espera a porta confirmar
  Motivo motivo = Motivo::LIBERADO;
  Origem origem = Origem::DESCONHECIDA;
  unsigned long quando = 0;
};

// O que a portaria sabe de uma porta (último ESTADO recebido dela).
struct PortaNaPortaria {
  Papel papel = Papel::PORTA_A;
  Noticia noticia;
  bool liberada = false, aberta = false, alarme = false, comBypass = false;
  uint32_t restanteMs = 0, abertaHaS = 0;
  char modo = 'I';
  Origem origem = Origem::DESCONHECIDA;
  bool emLiberacao = false;  // a portaria liberou e a porta ainda não confirmou
  unsigned long emLiberacaoDesde = 0;
  bool buzzer = false;
  ResultadoComando resultado;
};

const PortaNaPortaria& portaNaPortaria(Papel porta);
VisaoPorta visaoDaPorta(Papel porta, unsigned long agora);
// Se a porta pode abrir agora, pelo intertravamento (sem abrir nada).
Motivo avaliarAbertura(Papel porta, unsigned long agora);
// Botões e página da portaria: confere o intertravamento e manda a porta abrir.
Motivo abrirPelaPortaria(Papel porta, Origem origem, unsigned long agora);

bool bypassAtivo();
bool bypassDaPagina();
void definirBypassDaPagina(bool ligado);

const Entrada& botaoPortaA();
const Entrada& botaoPortaB();
const Entrada& chaveBypass();
const Saida& buzzerDaPorta(Papel porta);

#endif
