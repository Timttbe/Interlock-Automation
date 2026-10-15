#include "entradas.h"

void Entrada::iniciar() {
  pinMode(pino_, INPUT_PULLUP);
  delayMicroseconds(50);  // tempo do pull-up carregar a fiação antes da primeira leitura
  ativa_ = leituraAtiva_ = digitalRead(pino_) == nivelAtivo_;
  leituraMudouEm_ = mudouEm_ = millis();
}

bool Entrada::atualizar(unsigned long agora) {
  bool leitura = digitalRead(pino_) == nivelAtivo_;
  if (leitura != leituraAtiva_) {
    leituraAtiva_ = leitura;
    leituraMudouEm_ = agora;
  }
  if (leituraAtiva_ == ativa_ || agora - leituraMudouEm_ < debounceMs_) return false;
  ativa_ = leituraAtiva_;
  mudouEm_ = agora;
  if (ativa_) acionamentos_++;
  return true;
}

void Saida::iniciar() {
  if (pino_ < 0) return;
  ligar(false);  // o nível de desligado vai antes de o pino virar saída: o relé não dá pulso
  pinMode(pino_, OUTPUT);
  ligar(false);
}

void Saida::ligar(bool ligada) {
  ligada_ = ligada;
  if (pino_ < 0) return;
  uint8_t desligado = nivelAtivo_ == HIGH ? LOW : HIGH;
  digitalWrite(pino_, ligada ? nivelAtivo_ : desligado);
}
