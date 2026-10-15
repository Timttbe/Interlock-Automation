#pragma once
// Wi-Fi da placa.
//
// Normal: conectada no roteador (IP automático ou fixo). É por ele que as placas conversam.
// Rede própria ("Interlock-Portaria", "Interlock-PortaA"...; página em http://192.168.4.1) liga:
//   - quando o Wi-Fi não está configurado;
//   - depois de AP_SEM_WIFI_MS (30 s) sem conexão com o roteador;
//   - nos primeiros AP_NO_BOOT_MS depois de ligar, se configurado (de fábrica, não);
//   - e, se o Wi-Fi voltar com alguém conectado nela, continua por até AP_EM_USO_MAX_MS (2 min).
// Sem conexão, a placa continua tentando o roteador, mesmo com a rede própria ligada: muitas vezes
// foi só o roteador que reiniciou. IP fixo errado (conecta, mas a placa fica inacessível): botão BOOT.

#include <Arduino.h>
#include <IPAddress.h>

void iniciarWifi();
void atualizarWifi(unsigned long agora);

bool wifiConectado();
bool redePropriaLigada();
uint8_t clientesRedePropria();
IPAddress ipRedePropria();
const char* nomeHost();         // "interlock-porta-a" (página também em http://interlock-porta-a.local)
const char* nomeRedePropria();  // "Interlock-PortaA"
