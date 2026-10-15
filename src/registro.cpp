#include "registro.h"
#include <Arduino.h>
#include <stdarg.h>
#include <time.h>
#include "json.h"

namespace {

constexpr uint8_t QUANTIDADE = 40;
constexpr time_t HORA_VALIDA = 1700000000;  // antes disso o relógio ainda não foi acertado pela internet

struct Evento {
  unsigned long ms;
  time_t hora;  // 0 = relógio ainda não acertado
  char texto[96];
};

Evento eventos[QUANTIDADE];
uint8_t proximo = 0;
uint8_t total = 0;

// Se o texto foi cortado no meio de uma letra acentuada (UTF-8 de 2 ou mais bytes), tira o pedaço.
void cortarLetraPelaMetade(char* texto) {
  size_t n = strlen(texto);
  size_t i = n;
  while (i > 0 && (static_cast<uint8_t>(texto[i - 1]) & 0xC0) == 0x80) i--;  // bytes de continuação
  if (i == 0) return;
  uint8_t inicio = static_cast<uint8_t>(texto[i - 1]);
  if (inicio < 0xC0) return;  // último caractere é ASCII: nada cortado
  size_t esperado = inicio >= 0xF0 ? 4 : inicio >= 0xE0 ? 3 : 2;
  if (n - (i - 1) < esperado) texto[i - 1] = 0;
}

}  // namespace

void registrar(const char* formato, ...) {
  Evento& e = eventos[proximo];
  va_list args;
  va_start(args, formato);
  int n = vsnprintf(e.texto, sizeof(e.texto), formato, args);
  va_end(args);
  if (n >= static_cast<int>(sizeof(e.texto))) cortarLetraPelaMetade(e.texto);
  e.ms = millis();
  time_t agora = time(nullptr);
  e.hora = agora > HORA_VALIDA ? agora : 0;

  proximo = (proximo + 1) % QUANTIDADE;
  if (total < QUANTIDADE) total++;
  Serial.printf("[%8.1f] %s\n", e.ms / 1000.0, e.texto);
}

void registroJson(JsonEscritor& j) {
  unsigned long agoraMs = millis();
  time_t agora = time(nullptr);
  bool horaValida = agora > HORA_VALIDA;
  for (uint8_t k = 0; k < total; k++) {
    const Evento& e = eventos[(proximo + QUANTIDADE - 1 - k) % QUANTIDADE];
    unsigned long ha = (agoraMs - e.ms) / 1000;
    // Eventos de antes do relógio ser acertado: calcula a hora pelo tempo que passou.
    time_t hora = e.hora ? e.hora : horaValida ? agora - static_cast<time_t>(ha) : 0;
    j.abrirObjeto();
    j.numeroU("ha", ha);
    j.numeroU("hora", static_cast<unsigned long>(hora));
    j.texto("m", e.texto);
    j.fecharObjeto();
  }
}
