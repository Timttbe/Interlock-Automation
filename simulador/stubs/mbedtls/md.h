#pragma once
// Assinatura de mentira para o simulador: não é criptografia, só um hash com chave (FNV-1a
// 64 bits) para conferir que mensagem alterada ou com chave diferente é recusada.
#include <stddef.h>
#include <stdint.h>

#define MBEDTLS_MD_SHA256 6
struct mbedtls_md_info_t {
  int tipo;
};
inline const mbedtls_md_info_t* mbedtls_md_info_from_type(int) {
  static mbedtls_md_info_t info{6};
  return &info;
}
inline int mbedtls_md_hmac(const mbedtls_md_info_t*, const unsigned char* chave, size_t tamChave,
                           const unsigned char* dados, size_t tam, unsigned char* saida) {
  for (int bloco = 0; bloco < 4; bloco++) {
    uint64_t h = 1469598103934665603ULL ^ (uint64_t)bloco;
    for (size_t i = 0; i < tamChave; i++) h = (h ^ chave[i]) * 1099511628211ULL;
    for (size_t i = 0; i < tam; i++) h = (h ^ dados[i]) * 1099511628211ULL;
    for (size_t i = 0; i < tamChave; i++) h = (h ^ chave[i]) * 1099511628211ULL;
    for (int k = 0; k < 8; k++) saida[bloco * 8 + k] = (h >> (8 * k)) & 0xFF;
  }
  return 0;
}

// Contexto e PBKDF2 de mentira (contas.cpp): um hash com sal, sem as 2000 rodadas.
struct mbedtls_md_context_t {
  int nada;
};
inline void mbedtls_md_init(mbedtls_md_context_t*) {}
inline int mbedtls_md_setup(mbedtls_md_context_t*, const mbedtls_md_info_t*, int) { return 0; }
inline void mbedtls_md_free(mbedtls_md_context_t*) {}
