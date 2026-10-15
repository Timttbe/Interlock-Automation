#include "json.h"

void JsonEscritor::prefixo(const char* chave) {
  if (nivel_ > 0 && nivel_ <= MAX_NIVEL) {
    if (!primeiro_[nivel_ - 1]) s_ += ',';
    primeiro_[nivel_ - 1] = false;
  }
  if (chave) {
    s_ += '"';
    escapar(chave);
    s_ += "\":";
  }
}

void JsonEscritor::abrirObjeto(const char* chave) {
  prefixo(chave);
  s_ += '{';
  if (nivel_ < MAX_NIVEL) primeiro_[nivel_] = true;
  nivel_++;
}

void JsonEscritor::fecharObjeto() {
  nivel_--;
  s_ += '}';
}

void JsonEscritor::abrirLista(const char* chave) {
  prefixo(chave);
  s_ += '[';
  if (nivel_ < MAX_NIVEL) primeiro_[nivel_] = true;
  nivel_++;
}

void JsonEscritor::fecharLista() {
  nivel_--;
  s_ += ']';
}

void JsonEscritor::escapar(const char* texto) {
  for (const char* p = texto; *p; p++) {
    unsigned char c = static_cast<unsigned char>(*p);
    switch (c) {
      case '"': s_ += "\\\""; break;
      case '\\': s_ += "\\\\"; break;
      case '\n': s_ += "\\n"; break;
      case '\r': s_ += "\\r"; break;
      case '\t': s_ += "\\t"; break;
      default:
        if (c < 0x20) {
          char codigo[8];
          snprintf(codigo, sizeof(codigo), "\\u%04x", c);
          s_ += codigo;
        } else {
          s_ += static_cast<char>(c);  // UTF-8 passa direto
        }
    }
  }
}

void JsonEscritor::texto(const char* chave, const char* valor) {
  prefixo(chave);
  s_ += '"';
  escapar(valor ? valor : "");
  s_ += '"';
}

void JsonEscritor::numero(const char* chave, long valor) {
  prefixo(chave);
  s_ += String(valor);
}

void JsonEscritor::numeroU(const char* chave, unsigned long valor) {
  prefixo(chave);
  s_ += String(valor);
}

void JsonEscritor::logico(const char* chave, bool valor) {
  prefixo(chave);
  s_ += valor ? "true" : "false";
}

void JsonEscritor::nulo(const char* chave) {
  prefixo(chave);
  s_ += "null";
}
