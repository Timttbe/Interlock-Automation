#pragma once
#include <string.h>
#include "md.h"

inline int mbedtls_pkcs5_pbkdf2_hmac(mbedtls_md_context_t*, const unsigned char* senha, size_t tamSenha,
                                     const unsigned char* sal, size_t tamSal, unsigned int, uint32_t tamSaida,
                                     unsigned char* saida) {
  unsigned char resumo[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), sal, tamSal, senha, tamSenha, resumo);
  memcpy(saida, resumo, tamSaida < 32 ? tamSaida : 32);
  return 0;
}
