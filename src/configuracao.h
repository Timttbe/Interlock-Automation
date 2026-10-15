#pragma once
// Configuração que fica salva na flash da placa (Preferences/NVS) e muda pela página.

#include <Arduino.h>

struct ConfigRede {
  char ssid[33];
  char senha[65];
  bool dhcp;
  uint32_t ip, mascara, gateway, dns;  // do jeito que o IPAddress guarda (ordem da rede); dns 0 = o gateway
  uint8_t grupo;  // 1 a 99: separa conjuntos de portas diferentes na mesma rede
};

struct ConfigOperacao {
  uint16_t tempoAberturaS;
  uint16_t tempoAlarmeS;
  bool liberarSemComunicacao;
};

constexpr uint16_t TEMPO_ABERTURA_MIN_S = 1, TEMPO_ABERTURA_MAX_S = 60;
constexpr uint16_t TEMPO_ALARME_MIN_S = 5, TEMPO_ALARME_MAX_S = 3600;
constexpr uint8_t GRUPO_MAX = 99;

extern ConfigRede cfgRede;
extern ConfigOperacao cfgOperacao;

void carregarConfiguracao();
void salvarRede();
void salvarOperacao();
void restaurarRedeDeFabrica();  // volta o Wi-Fi do config.h, IP automático e grupo 1 (e salva)

// Máscara válida: todos os bits 1 seguidos e depois só 0 (ex.: 255.255.255.0).
bool mascaraValida(uint32_t mascara);
// IP fixo utilizável: na mesma faixa do gateway, diferente dele, nem endereço de rede nem broadcast.
bool redeValida(uint32_t ip, uint32_t gateway, uint32_t mascara);
