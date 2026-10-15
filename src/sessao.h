#pragma once
// Proteção contra mensagens repetidas: alguém que grave da rede uma mensagem "abrir" (que é
// assinada, ver protocolo.cpp) não consegue mandá-la de novo depois.
//
// Cada placa sorteia um número de sessão ao ligar e numera as mensagens que envia (seq).
// Quem recebe:
//  - na sessão já aceita, aceita cada seq uma vez só (janela de 32 para tolerar pacotes fora
//    de ordem), e nunca um seq anterior ao da mensagem que estabeleceu a sessão;
//  - numa sessão nova (a outra placa reiniciou), só aceita se a mensagem trouxer de volta ("eco")
//    a sessão atual de quem recebe, prova de que foi criada depois que ela ligou;
//  - nunca volta para uma sessão que já foi substituída.
//
// Só lógica pura (sem rede), testada em test/test_intertravamento.

#include <stdint.h>

constexpr uint8_t SESSOES_ANTIGAS = 8;

struct SessaoPar {
  uint32_t sessao = 0;       // sessão aceita (0 = nenhuma ainda)
  uint32_t sessaoVista = 0;  // última sessão vista dessa placa: vai no eco para ela
  uint32_t ultimoSeq = 0;
  uint32_t janela = 0;       // bit n ligado = (ultimoSeq - n) já recebido
  uint32_t antigas[SESSOES_ANTIGAS] = {};
  uint8_t proximaAntiga = 0;
};

enum class Aceite : uint8_t {
  ACEITA,         // mensagem nova da sessão atual
  NOVA_SESSAO,    // a outra placa reiniciou (ou é o primeiro contato) e provou que a mensagem é recente
  REPETIDA,       // seq já recebido, ou velho demais
  SESSAO_ANTIGA,  // sessão que já foi substituída
  SEM_ECO,        // sessão nova sem o eco da nossa sessão: ainda não dá para saber se é recente
};

inline bool aceitou(Aceite a) { return a == Aceite::ACEITA || a == Aceite::NOVA_SESSAO; }

Aceite avaliarMensagem(SessaoPar& par, uint32_t minhaSessao, uint32_t sessao, uint32_t seq, uint32_t ecoParaMim);
