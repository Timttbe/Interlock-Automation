#pragma once
// Página da placa e as rotas comuns às 3 placas. Todas pedem usuário "admin" e a SENHA_ADMIN.
//
//   GET  /             página (diferente para porta e portaria: web_porta.cpp / web_porteiro.cpp)
//   GET  /api/estado   tudo o que a página mostra, em JSON (a página consulta a cada segundo)
//   POST /api/rede     salva Wi-Fi, IP e grupo, e reinicia
//   GET  /api/redes    procura as redes Wi-Fi ao alcance (?novo=1 começa uma procura nova)
//   POST /api/reiniciar
//   POST /api/atualizar  firmware novo (atualizacao.cpp)

#include <Arduino.h>
#include <WebServer.h>
#include "entradas.h"
#include "json.h"

extern WebServer servidor;

void iniciarWeb();
void atualizarWeb();

// Confere usuário e senha; se estiverem errados, já pede de novo ao navegador e devolve false.
bool autorizado();
void responderOk(const String& mensagem);
void responderErro(const String& erro, int codigo = 400);
// Envia a página em pedaços que estão na flash (sem montar tudo na memória).
void enviarPagina(const char* const* partes, size_t quantidade);

// Uma entrada ou saída na lista "Entradas e saídas" da página. 'podeTravar': avisar se ficar
// acionada por mais de ENTRADA_TRAVADA_MS (fio em curto ou contato preso).
void entradaJson(JsonEscritor& j, const char* nome, const Entrada& e, const char* textoAtiva, const char* textoInativa,
                 bool podeTravar, unsigned long agora);
void saidaJson(JsonEscritor& j, const char* nome, const Saida& s, const char* textoLigada, const char* textoDesligada);
