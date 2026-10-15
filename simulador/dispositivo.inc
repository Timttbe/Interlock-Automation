// Uma placa simulada: o firmware real inteiro dentro do namespace INSTANCIA.
#define JUNTA2(a, b) a##b
#define JUNTA(a, b) JUNTA2(a, b)
namespace INSTANCIA {
#include "proj/src/papel.cpp"
#include "proj/src/intertravamento.cpp"
#include "proj/src/sessao.cpp"
#include "proj/src/entradas.cpp"
#include "proj/src/json.cpp"
#include "proj/src/registro.cpp"
#include "proj/src/configuracao.cpp"
#include "proj/src/sistema.cpp"
#include "proj/src/protocolo.cpp"
#include "proj/src/porta.cpp"
#include "proj/src/porteiro.cpp"
#include "proj/src/main.cpp"
// rede_wifi.h: o Wi-Fi é controlado pelo simulador
void iniciarWifi() {}
void atualizarWifi(unsigned long) {}
bool wifiConectado() { return sim::host()->wifiConectado(sim::dispositivo()); }
bool redePropriaLigada() { return false; }
uint8_t clientesRedePropria() { return 0; }
IPAddress ipRedePropria() { return IPAddress(192, 168, 4, 1); }
const char* nomeHost() { return "sim"; }
const char* nomeRedePropria() { return "sim"; }
// web.h: sem página no simulador
void iniciarWeb() {}
void atualizarWeb() {}
}  // namespace INSTANCIA
void JUNTA(INSTANCIA, _setup)() { INSTANCIA::setup(); }
void JUNTA(INSTANCIA, _loop)() { INSTANCIA::loop(); }
void JUNTA(INSTANCIA, _regra)(int liberar) { INSTANCIA::cfgOperacao.liberarSemComunicacao = liberar != 0; }
