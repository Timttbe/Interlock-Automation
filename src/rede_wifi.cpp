#include "rede_wifi.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include "config.h"
#include "configuracao.h"
#include "papel.h"
#include "registro.h"

namespace {

char host[24];
char ssidRedePropria[24];
bool conectadoAntes = false;
unsigned long semWifiDesde = 0;
unsigned long conectadoDesde = 0;
unsigned long ultimaTentativa = 0;
unsigned long ultimaConferencia = 0;  // da rede própria
bool apLigado = false;
uint8_t clientesAP = 0;
bool erroAPAvisado = false;
unsigned long ultimoErroAP = 0;
bool janelaDoBootAcabou = AP_NO_BOOT_MS == 0;
uint8_t canal = 1;  // canal do roteador na última conexão: a rede própria usa o mesmo
IPAddress ipAP(192, 168, 4, 1);
bool horaPedida = false;

bool temWifi() { return cfgRede.ssid[0] != 0; }

void conectar() {
  if (!temWifi()) return;
  if (cfgRede.dhcp) {
    WiFi.config(IPAddress(static_cast<uint32_t>(0)), IPAddress(static_cast<uint32_t>(0)),
                IPAddress(static_cast<uint32_t>(0)));  // tudo zero = IP automático (DHCP)
  } else {
    uint32_t dns = cfgRede.dns ? cfgRede.dns : cfgRede.gateway;
    WiFi.config(IPAddress(cfgRede.ip), IPAddress(cfgRede.gateway), IPAddress(cfgRede.mascara), IPAddress(dns));
  }
  WiFi.begin(cfgRede.ssid, cfgRede.senha);
}

void ligarRedePropria(bool conectado) {
  // A rede própria não pode ficar na mesma faixa de IP do roteador.
  IPAddress ip(192, 168, 4, 1);
  IPAddress local = WiFi.localIP();
  if (conectado && local[0] == 192 && local[1] == 168 && local[2] == 4) ip = IPAddress(192, 168, 5, 1);

  WiFi.mode(WIFI_AP_STA);
  // Com o roteador conectado, o ESP32 é obrigado a usar o canal dele também na rede própria.
  bool ok = WiFi.softAP(ssidRedePropria, SENHA_ADMIN, conectado ? WiFi.channel() : canal, 0, 4) &&
            WiFi.waitStatusBits(AP_STARTED_BIT, 2000) != 0;
  // Trocar o IP desliga e religa o servidor de DHCP da rede própria; se algo falhar no meio, a rede
  // aparece mas ninguém consegue entrar. Então só troca quando precisa (o padrão já é 192.168.4.1).
  if (ok && WiFi.softAPIP() != ip) ok = WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  if (!ok) {
    // Tenta de novo na próxima conferência; avisa no máximo uma vez por minuto.
    if (!erroAPAvisado || millis() - ultimoErroAP >= 60000) {
      registrar("ERRO: não consegui ligar a rede própria \"%s\"", ssidRedePropria);
      erroAPAvisado = true;
      ultimoErroAP = millis();
    }
    WiFi.softAPdisconnect(true);
    return;
  }
  ipAP = ip;
  apLigado = true;
  registrar("Rede própria \"%s\" ligada: página em http://%s (senha: a SENHA_ADMIN)", ssidRedePropria,
            ip.toString().c_str());
}

void desligarRedePropria() {
  WiFi.softAPdisconnect(true);
  apLigado = false;
  registrar("Rede própria desligada");
}

}  // namespace

void iniciarWifi() {
  const char* sufixoHost = MEU_PAPEL == Papel::PORTEIRO ? "portaria" : MEU_PAPEL == Papel::PORTA_A ? "porta-a" : "porta-b";
  const char* sufixoRede = MEU_PAPEL == Papel::PORTEIRO ? "Portaria" : MEU_PAPEL == Papel::PORTA_A ? "PortaA" : "PortaB";
  snprintf(host, sizeof(host), "interlock-%s", sufixoHost);
  snprintf(ssidRedePropria, sizeof(ssidRedePropria), "Interlock-%s", sufixoRede);

  WiFi.persistent(false);        // a configuração fica só nas Preferences deste firmware
  WiFi.setAutoReconnect(false);  // a reconexão é feita em atualizarWifi(), com intervalo
  WiFi.setHostname(host);        // precisa vir antes do WiFi.mode()
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // sem economia de energia: menos atraso e nenhuma mensagem perdida

  semWifiDesde = ultimaTentativa = millis();
  if (temWifi()) {
    registrar("Conectando no Wi-Fi \"%s\" (%s)...", cfgRede.ssid, cfgRede.dhcp ? "IP automático" : "IP fixo");
    conectar();
  } else {
    registrar("Wi-Fi não configurado");
  }

  if (MDNS.begin(host)) MDNS.addService("http", "tcp", 80);
}

void atualizarWifi(unsigned long agora) {
  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado && !conectadoAntes) {
    conectadoDesde = agora;
    canal = WiFi.channel();
    registrar("Wi-Fi conectado em \"%s\": IP %s, sinal %d dBm, canal %u", cfgRede.ssid,
              WiFi.localIP().toString().c_str(), WiFi.RSSI(), canal);
    if (!horaPedida) {
      // Hora certa no registro de eventos, se o roteador tiver internet (se não tiver, tudo funciona igual).
      configTime(0, 0, "pool.ntp.org", "a.st1.ntp.br");
      horaPedida = true;
    }
  } else if (!conectado && conectadoAntes) {
    semWifiDesde = agora;
    ultimaTentativa = agora - INTERVALO_RECONEXAO_MS + 1000;  // primeira tentativa já daqui a 1 s
    registrar("Wi-Fi caiu");
  }
  conectadoAntes = conectado;

  // Sem conexão: tenta de novo, para sempre. Cada tentativa procura o roteador em todos os canais,
  // e nesses ~2 s a rede própria fica instável: com alguém usando ela, tenta com menos frequência.
  unsigned long intervalo = clientesAP > 0 ? INTERVALO_RECONEXAO_COM_AP_MS : INTERVALO_RECONEXAO_MS;
  if (!conectado && temWifi() && agora - ultimaTentativa >= intervalo) {
    ultimaTentativa = agora;
    WiFi.disconnect();
    conectar();
  }

  // A rede própria não precisa ser conferida a cada volta do loop.
  if (agora - ultimaConferencia < 500) return;
  ultimaConferencia = agora;

#if AP_NO_BOOT_MS > 0
  if (!janelaDoBootAcabou && agora >= AP_NO_BOOT_MS) janelaDoBootAcabou = true;
#endif
  clientesAP = apLigado ? WiFi.softAPgetStationNum() : 0;
  bool muitoTempoSemWifi = !conectado && agora - semWifiDesde >= AP_SEM_WIFI_MS;
  bool emUso = clientesAP > 0 && (!conectado || agora - conectadoDesde < AP_EM_USO_MAX_MS);
  bool precisa = !temWifi() || muitoTempoSemWifi || !janelaDoBootAcabou || emUso;
  if (precisa && !apLigado) {
    ligarRedePropria(conectado);
  } else if (!precisa && apLigado) {
    desligarRedePropria();
    clientesAP = 0;
  }
}

bool wifiConectado() { return WiFi.status() == WL_CONNECTED; }
bool redePropriaLigada() { return apLigado; }
uint8_t clientesRedePropria() { return apLigado ? WiFi.softAPgetStationNum() : 0; }
IPAddress ipRedePropria() { return ipAP; }
const char* nomeHost() { return host; }
const char* nomeRedePropria() { return ssidRedePropria; }
