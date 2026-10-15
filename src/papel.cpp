#include "papel.h"

const char* nomeExibicao(Papel p) {
  switch (p) {
    case Papel::PORTEIRO: return "Portaria";
    case Papel::PORTA_A: return "Porta A";
    case Papel::PORTA_B: return "Porta B";
  }
  return "?";
}

char codigoPapel(Papel p) {
  switch (p) {
    case Papel::PORTEIRO: return 'P';
    case Papel::PORTA_A: return 'A';
    case Papel::PORTA_B: return 'B';
  }
  return '?';
}

bool papelDoCodigo(char codigo, Papel& papel) {
  switch (codigo) {
    case 'P': papel = Papel::PORTEIRO; return true;
    case 'A': papel = Papel::PORTA_A; return true;
    case 'B': papel = Papel::PORTA_B; return true;
    default: return false;
  }
}

Papel outraPorta(Papel porta) { return porta == Papel::PORTA_A ? Papel::PORTA_B : Papel::PORTA_A; }
