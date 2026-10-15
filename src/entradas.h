#pragma once
#include <Arduino.h>

// Entrada digital com pull-up interno e filtro de ruído (debounce).
class Entrada {
 public:
  Entrada(uint8_t pino, uint8_t nivelAtivo, uint16_t debounceMs)
      : pino_(pino), nivelAtivo_(nivelAtivo), debounceMs_(debounceMs) {}

  // Configura o pino e lê o estado inicial (sem contar como acionamento: um botão preso
  // ao ligar a placa não abre a porta).
  void iniciar();
  // Lê o pino. Devolve true quando o estado filtrado muda; ativa() diz para qual.
  bool atualizar(unsigned long agora);

  bool ativa() const { return ativa_; }
  unsigned long mudouEm() const { return mudouEm_; }  // quando o estado filtrado mudou pela última vez
  uint32_t acionamentos() const { return acionamentos_; }
  uint8_t pino() const { return pino_; }

 private:
  uint8_t pino_;
  uint8_t nivelAtivo_;
  uint16_t debounceMs_;
  bool ativa_ = false;
  bool leituraAtiva_ = false;
  unsigned long leituraMudouEm_ = 0;
  unsigned long mudouEm_ = 0;
  uint32_t acionamentos_ = 0;
};

// Saída digital (relé, buzzer, LED) com o nível que liga configurável.
class Saida {
 public:
  Saida(int8_t pino, uint8_t nivelAtivo) : pino_(pino), nivelAtivo_(nivelAtivo) {}

  // Deixa a saída desligada, sem pulso: o nível de desligado é escrito antes do pino virar saída.
  void iniciar();
  void ligar(bool ligada);
  bool ligada() const { return ligada_; }
  int8_t pino() const { return pino_; }

 private:
  int8_t pino_;
  uint8_t nivelAtivo_;
  bool ligada_ = false;
};
