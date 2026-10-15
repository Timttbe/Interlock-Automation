#pragma once
#include <Arduino.h>

// Monta um JSON direto numa String, cuidando das vírgulas e dos caracteres especiais.
//   String s; JsonEscritor j(s);
//   j.abrirObjeto(); j.texto("nome", "Porta A"); j.abrirLista("itens"); ... j.fecharLista(); j.fecharObjeto();
class JsonEscritor {
 public:
  explicit JsonEscritor(String& saida) : s_(saida) {}

  void abrirObjeto(const char* chave = nullptr);
  void fecharObjeto();
  void abrirLista(const char* chave = nullptr);
  void fecharLista();

  void texto(const char* chave, const char* valor);
  void texto(const char* chave, const String& valor) { texto(chave, valor.c_str()); }
  void numero(const char* chave, long valor);
  void numeroU(const char* chave, unsigned long valor);
  void logico(const char* chave, bool valor);
  void nulo(const char* chave);

 private:
  static constexpr int MAX_NIVEL = 8;
  void prefixo(const char* chave);
  void escapar(const char* texto);
  String& s_;
  bool primeiro_[MAX_NIVEL] = {};
  int nivel_ = 0;
};
