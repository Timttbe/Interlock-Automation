#pragma once
// Página da placa e as rotas comuns às 3 placas (login e permissões: acesso.h).
//
//   GET  /                 página (diferente para porta e portaria: web_porta.cpp / web_porteiro.cpp);
//                          sem login, ela mostra a tela de login
//   GET  /logo-*.png       logo (sem login, para a tela de login)
//   POST /api/entrar       login; POST /api/sair: sair
//   GET  /api/estado       tudo o que a página mostra, em JSON (a página consulta a cada segundo)
//   POST /api/reiniciar
//   POST /api/rede         admin: salva Wi-Fi, IP e grupo, e reinicia
//   GET  /api/redes        admin: procura as redes Wi-Fi ao alcance (?novo=1 começa uma procura nova)
//   POST /api/atualizar    admin: firmware novo (atualizacao.cpp)

#include <Arduino.h>
#include <WebServer.h>
#include "entradas.h"
#include "json.h"

extern WebServer servidor;

void iniciarWeb();
void atualizarWeb();

void responderOk(const String& mensagem);
void responderErro(const String& erro, int codigo = 400);
// Envia a página em pedaços que estão na flash (sem montar tudo na memória).
void enviarPagina(const char* const* partes, size_t quantidade);

// Uma entrada ou saída na lista "Entradas e saídas" da página. 'podeTravar': avisar se ficar
// acionada por mais de ENTRADA_TRAVADA_MS (fio em curto ou contato preso).
void entradaJson(JsonEscritor& j, const char* nome, const Entrada& e, const char* textoAtiva, const char* textoInativa,
                 bool podeTravar, unsigned long agora);
void saidaJson(JsonEscritor& j, const char* nome, const Saida& s, const char* textoLigada, const char* textoDesligada);
