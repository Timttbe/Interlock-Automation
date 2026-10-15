#pragma once
// Papel desta placa no intertravamento. O mesmo código vai nas 3 placas; o papel vem do
// ambiente escolhido no platformio.ini (porteiro, porta_a ou porta_b).

#include <stdint.h>

#if defined(PAPEL_PORTEIRO) + defined(PAPEL_PORTA_A) + defined(PAPEL_PORTA_B) != 1
#error "Grave usando um dos ambientes do platformio.ini: porteiro, porta_a ou porta_b"
#endif

enum class Papel : uint8_t { PORTEIRO = 0, PORTA_A = 1, PORTA_B = 2 };
constexpr uint8_t NUM_PAPEIS = 3;

#if defined(PAPEL_PORTEIRO)
#define NOME_PAPEL "PORTEIRO"
#define AMBIENTE_PIO "porteiro"
constexpr Papel MEU_PAPEL = Papel::PORTEIRO;
#elif defined(PAPEL_PORTA_A)
#define NOME_PAPEL "PORTA_A"
#define AMBIENTE_PIO "porta_a"
#define PAPEL_E_PORTA 1
constexpr Papel MEU_PAPEL = Papel::PORTA_A;
#else
#define NOME_PAPEL "PORTA_B"
#define AMBIENTE_PIO "porta_b"
#define PAPEL_E_PORTA 1
constexpr Papel MEU_PAPEL = Papel::PORTA_B;
#endif

inline uint8_t indice(Papel p) { return static_cast<uint8_t>(p); }
const char* nomeExibicao(Papel p);  // "Portaria", "Porta A", "Porta B"
char codigoPapel(Papel p);          // 'P', 'A', 'B' (usado nas mensagens entre as placas)
bool papelDoCodigo(char codigo, Papel& papel);
Papel outraPorta(Papel porta);      // PORTA_A <-> PORTA_B
