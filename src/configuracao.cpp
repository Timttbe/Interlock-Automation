#include "configuracao.h"
#include <Preferences.h>
#include "config.h"

ConfigRede cfgRede;
ConfigOperacao cfgOperacao;

namespace {

const char* const ESPACO = "interlock";

// Muda sempre que o Wi-Fi de fábrica do config.h muda. Assim, gravar o firmware com outro Wi-Fi
// no config.h faz esse Wi-Fi valer de novo, mesmo que outro tenha sido salvo pela página.
uint32_t idDaFabrica() {
  const char* texto = WIFI_SSID_PADRAO "|" WIFI_SENHA_PADRAO;
  uint32_t h = 2166136261u;
  for (const char* c = texto; *c; c++) h = (h ^ static_cast<uint8_t>(*c)) * 16777619u;
  return h;
}

void redeDeFabrica() {
  memset(&cfgRede, 0, sizeof(cfgRede));
  strlcpy(cfgRede.ssid, WIFI_SSID_PADRAO, sizeof(cfgRede.ssid));
  strlcpy(cfgRede.senha, WIFI_SENHA_PADRAO, sizeof(cfgRede.senha));
  cfgRede.dhcp = true;
  cfgRede.grupo = 1;
}

}  // namespace

bool mascaraValida(uint32_t mascara) {
  uint32_t zeros = ~__builtin_bswap32(mascara);  // IPAddress guarda os bytes na ordem da rede
  return mascara != 0 && (zeros & (zeros + 1)) == 0;
}

bool redeValida(uint32_t ip, uint32_t gateway, uint32_t mascara) {
  return mascaraValida(mascara) && ip != gateway && (ip & mascara) == (gateway & mascara) &&
         (ip & ~mascara) != 0 && (ip | mascara) != 0xFFFFFFFF;
}

void carregarConfiguracao() {
  Preferences p;
  p.begin(ESPACO, false);

  redeDeFabrica();
  if (p.getUInt("idFab", 0) == idDaFabrica()) {
    p.getString("ssid", cfgRede.ssid, sizeof(cfgRede.ssid));
    p.getString("senha", cfgRede.senha, sizeof(cfgRede.senha));
    cfgRede.dhcp = p.getBool("dhcp", true);
    cfgRede.ip = p.getUInt("ip", 0);
    cfgRede.mascara = p.getUInt("mascara", 0);
    cfgRede.gateway = p.getUInt("gateway", 0);
    cfgRede.dns = p.getUInt("dns", 0);
    cfgRede.grupo = p.getUChar("grupo", 1);
  }
  if (cfgRede.grupo < 1 || cfgRede.grupo > GRUPO_MAX) cfgRede.grupo = 1;
  if (!cfgRede.dhcp && !redeValida(cfgRede.ip, cfgRede.gateway, cfgRede.mascara)) cfgRede.dhcp = true;

  uint16_t abertura = p.getUShort("tAbertura", TEMPO_ABERTURA_PADRAO_S);
  uint16_t alarme = p.getUShort("tAlarme", TEMPO_ALARME_PADRAO_S);
  cfgOperacao.tempoAberturaS = constrain(abertura, TEMPO_ABERTURA_MIN_S, TEMPO_ABERTURA_MAX_S);
  cfgOperacao.tempoAlarmeS = constrain(alarme, TEMPO_ALARME_MIN_S, TEMPO_ALARME_MAX_S);
  cfgOperacao.liberarSemComunicacao = p.getBool("semCom", LIBERAR_SEM_COMUNICACAO_PADRAO);
  p.end();
}

void salvarRede() {
  Preferences p;
  p.begin(ESPACO, false);
  p.putUInt("idFab", idDaFabrica());
  p.putString("ssid", cfgRede.ssid);
  p.putString("senha", cfgRede.senha);
  p.putBool("dhcp", cfgRede.dhcp);
  p.putUInt("ip", cfgRede.ip);
  p.putUInt("mascara", cfgRede.mascara);
  p.putUInt("gateway", cfgRede.gateway);
  p.putUInt("dns", cfgRede.dns);
  p.putUChar("grupo", cfgRede.grupo);
  p.end();
}

void salvarOperacao() {
  Preferences p;
  p.begin(ESPACO, false);
  p.putUShort("tAbertura", cfgOperacao.tempoAberturaS);
  p.putUShort("tAlarme", cfgOperacao.tempoAlarmeS);
  p.putBool("semCom", cfgOperacao.liberarSemComunicacao);
  p.end();
}

void restaurarRedeDeFabrica() {
  redeDeFabrica();
  salvarRede();
}
