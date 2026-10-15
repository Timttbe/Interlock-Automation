#pragma once
// Login na página.
//
// A página tem a própria tela de login: POST /api/entrar cria uma sessão (cookie "sessao") e
// POST /api/sair encerra. A sessão some depois de 12 h sem uso, ou quando a placa reinicia.
// Para o curl dos ambientes *_ota também vale usuário e senha no pedido (HTTP Basic), só na
// rota de firmware.
//
// Depois de 5 senhas erradas em 1 minuto, o login fica bloqueado por 30 s.

#include <Arduino.h>
#include "contas.h"

void iniciarAcesso();  // antes do servidor começar: cabeçalhos e rotas de login

// Para as rotas: confere a sessão (e o nível). Se não puder, já responde 401/403 e devolve false.
bool exigirLogin(String* nome = nullptr, Nivel* nivel = nullptr);
bool exigirAdmin(String* nome = nullptr);
// Sem responder nada: a sessão é de admin, ou o pedido traz usuário e senha de admin (curl).
bool adminNoPedido();

// Encerra as sessões de uma conta (apagada, ou senha trocada). 'menosAtual': mantém a de quem pediu.
void encerrarSessoesDe(const char* nome, bool menosAtual);
