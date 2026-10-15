#pragma once
// Contas de acesso à página.
//
// Há uma conta "admin", que pode tudo, e até MAX_CONTAS - 1 contas de usuário, criadas pelo admin.
// O usuário pode tudo, menos mudar a rede, gravar firmware e mexer nas contas dos outros.
//
// As contas são criadas e editadas só na portaria. As portas recebem uma cópia pela rede: a
// portaria anuncia a versão das contas no estado dela e a porta que tiver outra versão pede a
// lista (ver contasParaMensagem / contasDaMensagem). Sem comunicação, a porta usa a última cópia.
//
// A senha nunca fica guardada: só um resumo dela (PBKDF2-HMAC-SHA256 com sal aleatório).
// De fábrica existe só o admin, com a SENHA_ADMIN do segredos.h.

#include <Arduino.h>

class JsonEscritor;

enum class Nivel : uint8_t { USUARIO = 0, ADMIN = 1 };

constexpr uint8_t MAX_CONTAS = 10;
constexpr size_t TAM_NOME_MAX = 20;
constexpr size_t TAM_SENHA_MIN = 6;
constexpr size_t TAM_SENHA_MAX = 63;
#define NOME_ADMIN "admin"

void iniciarContas();

// Confere usuário e senha (demora uns 100 ms de propósito: dificulta adivinhar a senha).
bool conferirSenha(const char* nome, const char* senha, Nivel& nivel);
bool contaExiste(const char* nome, Nivel* nivel = nullptr);
bool senhaAdminDeFabrica();  // o admin ainda usa a SENHA_ADMIN do segredos.h

// Só na portaria. Devolvem vazio se deu certo, ou o motivo do erro.
String criarConta(const String& nome, const String& senha);
String apagarConta(const String& nome);
String definirSenha(const String& nome, const String& senha);
// Botão BOOT: o admin volta para a SENHA_ADMIN (nas portas, até a próxima cópia da portaria).
void restaurarSenhaAdmin();

// Parte "contas" de /api/estado: na portaria, a lista (para o admin); nas portas, a situação da cópia.
void contasJson(JsonEscritor& j, bool admin);

// Cópia da portaria para as portas
uint32_t versaoContas();
String contasParaMensagem();                                    // portaria
void versaoContasDaPortaria(uint32_t versao, unsigned long agora);  // porta: pede a lista se mudou
bool contasDaMensagem(const char* dados);                          // porta: grava a lista recebida
