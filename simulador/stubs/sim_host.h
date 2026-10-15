#pragma once
// Interface entre o firmware simulado (cada placa num namespace, ver dispositivo.inc) e o simulador.
#include <stddef.h>
#include <stdint.h>

struct HostApi {
  unsigned long (*millis)(int dispositivo);
  int (*digitalRead)(int dispositivo, int pino);
  void (*digitalWrite)(int dispositivo, int pino, int valor);
  void (*udpEnviar)(int dispositivo, uint32_t ipDestino, const uint8_t* dados, size_t n);
  int (*udpReceber)(int dispositivo, uint8_t* buf, size_t capacidade, uint32_t* ipOrigem);
  uint32_t (*aleatorio)();
  int (*wifiConectado)(int dispositivo);
  void (*log)(int dispositivo, const char* texto);
  void (*reiniciar)(int dispositivo);
};

namespace sim {
const HostApi* host();
int dispositivo();
}  // namespace sim
