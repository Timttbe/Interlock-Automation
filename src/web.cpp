#include "web.h"
#include <WiFi.h>
#include <time.h>
#include "atualizacao.h"
#include "config.h"
#include "configuracao.h"
#include "json.h"
#include "logica.h"
#include "papel.h"
#include "protocolo.h"
#include "rede_wifi.h"
#include "registro.h"
#include "sistema.h"

WebServer servidor(80);

namespace {

String ipTexto(uint32_t ip) { return ip ? IPAddress(ip).toString() : String(); }

void enviarJson(const String& json) {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json", json);
}

void tratarEstado() {
  if (!autorizado()) return;
  unsigned long agora = millis();
  bool conectado = wifiConectado();
  String s;
  s.reserve(8192);
  JsonEscritor j(s);
  j.abrirObjeto();

  j.texto("papel", NOME_PAPEL);
  j.texto("nome", nomeExibicao(MEU_PAPEL));
  j.texto("env", AMBIENTE_PIO);
  j.texto("fw", FW_VERSAO);
  j.texto("compilado", __DATE__ " " __TIME__);
  j.numeroU("up", agora / 1000);
  j.texto("reinicio", motivoDoReinicio());
  j.numeroU("heap", ESP.getFreeHeap());
  j.logico("senhaPadrao", strcmp(SENHA_ADMIN, SENHA_ADMIN_EXEMPLO) == 0 || strcmp(CHAVE_REDE, CHAVE_REDE_EXEMPLO) == 0);
  j.texto("conflito", conflitoDePapel());
  j.numeroU("rejeitadas", mensagensRecusadas());

  j.abrirObjeto("wifi");
  j.logico("conectado", conectado);
  j.texto("ssid", conectado ? WiFi.SSID() : String());
  j.texto("ip", conectado ? WiFi.localIP().toString() : String());
  j.texto("mascara", conectado ? WiFi.subnetMask().toString() : String());
  j.texto("gateway", conectado ? WiFi.gatewayIP().toString() : String());
  j.texto("dns", conectado ? WiFi.dnsIP().toString() : String());
  j.numero("rssi", conectado ? WiFi.RSSI() : 0);
  j.numero("canal", conectado ? WiFi.channel() : 0);
  j.texto("mac", WiFi.macAddress());
  j.texto("host", nomeHost());
  j.fecharObjeto();

  j.abrirObjeto("ap");
  j.logico("ligado", redePropriaLigada());
  j.texto("nome", nomeRedePropria());
  j.texto("ip", ipRedePropria().toString());
  j.numero("clientes", clientesRedePropria());
  j.fecharObjeto();

  j.abrirObjeto("cfgRede");
  j.texto("ssid", cfgRede.ssid);
  j.logico("dhcp", cfgRede.dhcp);
  j.texto("ip", ipTexto(cfgRede.ip));
  j.texto("mascara", ipTexto(cfgRede.mascara));
  j.texto("gateway", ipTexto(cfgRede.gateway));
  j.texto("dns", ipTexto(cfgRede.dns));
  j.numero("grupo", cfgRede.grupo);
  j.fecharObjeto();

  j.abrirLista("pares");
  for (uint8_t i = 0; i < NUM_PAPEIS; i++) {
    Papel p = static_cast<Papel>(i);
    j.abrirObjeto();
    j.texto("nome", nomeExibicao(p));
    if (p == MEU_PAPEL) {
      j.logico("eu", true);
      j.logico("online", true);
      j.texto("ip", conectado              ? WiFi.localIP().toString()
                    : redePropriaLigada() ? ipRedePropria().toString()
                                          : String());
      j.numero("rssi", conectado ? WiFi.RSSI() : 0);
      j.texto("fw", FW_VERSAO);
      j.numeroU("up", agora / 1000);
    } else {
      const InfoPar& par = infoPar(p);
      j.logico("eu", false);
      j.logico("online", parOnline(p));
      j.logico("conhecido", par.conhecida);
      j.texto("ip", par.conhecida ? par.ip.toString() : String());
      j.numero("rssi", par.rssi);
      j.texto("fw", par.fw);
      j.numeroU("up", par.ligadaS);
    }
    j.fecharObjeto();
  }
  j.fecharLista();

  j.abrirLista("registro");
  registroJson(j);
  j.fecharLista();

  logicaJson(j, agora);
  j.fecharObjeto();
  enviarJson(s);
}

void tratarSalvarRede() {
  if (!autorizado()) return;
  String ssid = servidor.arg("ssid");
  String senha = servidor.arg("senha");
  bool dhcp = servidor.hasArg("dhcp");
  long grupo = servidor.arg("grupo").toInt();
  IPAddress ip, mascara, gateway, dns;

  if (ssid.length() == 0 || ssid.length() > 32) return responderErro("Nome do Wi-Fi inválido (de 1 a 32 caracteres).");
  if (senha.length() > 0 && (senha.length() < 8 || senha.length() > 63)) {
    return responderErro("A senha do Wi-Fi precisa ter de 8 a 63 caracteres.");
  }
  if (grupo < 1 || grupo > GRUPO_MAX) return responderErro("Grupo inválido (de 1 a 99).");
  if (!dhcp) {
    if (!ip.fromString(servidor.arg("ip")) || !mascara.fromString(servidor.arg("mascara")) ||
        !gateway.fromString(servidor.arg("gateway"))) {
      return responderErro("IP, máscara ou gateway em formato errado (ex.: 192.168.0.50). Nada foi salvo.");
    }
    if (!mascaraValida(mascara)) return responderErro("Máscara inválida (o normal é 255.255.255.0). Nada foi salvo.");
    if (!redeValida(ip, gateway, mascara)) {
      return responderErro(
          "O IP tem que estar na mesma faixa do gateway e ser diferente dele "
          "(ex.: gateway 192.168.0.1, placa 192.168.0.50). Nada foi salvo.");
    }
    String textoDns = servidor.arg("dns");
    textoDns.trim();
    if (textoDns.length() && !dns.fromString(textoDns)) return responderErro("DNS em formato errado. Nada foi salvo.");
  }

  // Senha em branco mantém a atual; mas, se o nome do Wi-Fi mudou, é uma rede sem senha.
  bool mesmaRede = ssid == cfgRede.ssid;
  strlcpy(cfgRede.ssid, ssid.c_str(), sizeof(cfgRede.ssid));
  if (senha.length() || !mesmaRede) strlcpy(cfgRede.senha, senha.c_str(), sizeof(cfgRede.senha));
  cfgRede.dhcp = dhcp;
  if (!dhcp) {
    cfgRede.ip = ip;
    cfgRede.mascara = mascara;
    cfgRede.gateway = gateway;
    cfgRede.dns = dns;
  }
  cfgRede.grupo = grupo;
  salvarRede();
  registrar("Rede salva pela página: \"%s\", %s, grupo %u", cfgRede.ssid,
            dhcp ? "IP automático" : ("IP fixo " + ip.toString()).c_str(), cfgRede.grupo);

  String s;
  JsonEscritor j(s);
  j.abrirObjeto();
  j.logico("ok", true);
  j.texto("msg", "Rede salva. A placa vai reiniciar.");
  if (!dhcp) j.texto("novoIp", ip.toString());
  j.fecharObjeto();
  enviarJson(s);
  agendarReinicio("rede alterada pela página");
}

// A procura roda em segundo plano; a página pergunta de novo até a lista ficar pronta.
void tratarRedes() {
  if (!autorizado()) return;
  int16_t n = WiFi.scanComplete();
  if (servidor.hasArg("novo") || n == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    n = WIFI_SCAN_RUNNING;
  }
  String s;
  JsonEscritor j(s);
  j.abrirObjeto();
  if (n < 0) {
    j.logico("procurando", true);
  } else {
    j.abrirLista("redes");
    for (int16_t i = 0; i < n; i++) {
      String nome = WiFi.SSID(i);
      if (nome.isEmpty()) continue;  // rede oculta
      bool repetida = false;         // o mesmo nome em mais de um roteador: a lista já vem do mais forte
      for (int16_t k = 0; k < i && !repetida; k++) repetida = WiFi.SSID(k) == nome;
      if (repetida) continue;
      j.abrirObjeto();
      j.texto("ssid", nome);
      j.numero("rssi", WiFi.RSSI(i));
      j.logico("aberta", WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      j.fecharObjeto();
    }
    j.fecharLista();
    WiFi.scanDelete();
  }
  j.fecharObjeto();
  enviarJson(s);
}

void tratarReiniciar() {
  if (!autorizado()) return;
  responderOk("A placa vai reiniciar.");
  agendarReinicio("pedido pela página");
}

}  // namespace

bool autorizado() {
  if (servidor.authenticate("admin", SENHA_ADMIN)) return true;
  servidor.requestAuthentication(BASIC_AUTH, "Interlock");
  return false;
}

void responderOk(const String& mensagem) {
  String s;
  JsonEscritor j(s);
  j.abrirObjeto();
  j.logico("ok", true);
  j.texto("msg", mensagem);
  j.fecharObjeto();
  enviarJson(s);
}

void responderErro(const String& erro, int codigo) {
  String s;
  JsonEscritor j(s);
  j.abrirObjeto();
  j.logico("ok", false);
  j.texto("erro", erro);
  j.fecharObjeto();
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(codigo, "application/json", s);
}

void entradaJson(JsonEscritor& j, const char* nome, const Entrada& e, const char* textoAtiva, const char* textoInativa,
                 bool podeTravar, unsigned long agora) {
  j.abrirObjeto();
  j.texto("nome", nome);
  j.numero("pino", e.pino());
  j.logico("ativa", e.ativa());
  j.texto("textoAtiva", textoAtiva);
  j.texto("textoInativa", textoInativa);
  j.numeroU("ha", (agora - e.mudouEm()) / 1000);
  j.numeroU("cont", e.acionamentos());
  j.logico("travada", podeTravar && e.ativa() && agora - e.mudouEm() > ENTRADA_TRAVADA_MS);
  j.fecharObjeto();
}

void saidaJson(JsonEscritor& j, const char* nome, const Saida& s, const char* textoLigada, const char* textoDesligada) {
  j.abrirObjeto();
  j.texto("nome", nome);
  j.numero("pino", s.pino());
  j.logico("ativa", s.ligada());
  j.texto("textoAtiva", textoLigada);
  j.texto("textoInativa", textoDesligada);
  j.fecharObjeto();
}

void enviarPagina(const char* const* partes, size_t quantidade) {
  size_t total = 0;
  for (size_t i = 0; i < quantidade; i++) total += strlen(partes[i]);
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.setContentLength(total);
  servidor.send(200, "text/html; charset=utf-8", "");
  for (size_t i = 0; i < quantidade; i++) servidor.sendContent(partes[i], strlen(partes[i]));
}

void iniciarWeb() {
  servidor.on("/api/estado", HTTP_GET, tratarEstado);
  servidor.on("/api/rede", HTTP_POST, tratarSalvarRede);
  servidor.on("/api/redes", HTTP_GET, tratarRedes);
  servidor.on("/api/reiniciar", HTTP_POST, tratarReiniciar);
  registrarRotaAtualizacao();
  registrarRotasLogica(servidor);
  servidor.on("/favicon.ico", HTTP_GET, [] { servidor.send(204); });
  servidor.onNotFound([] { servidor.send(404, "text/plain; charset=utf-8", "não encontrado"); });
  servidor.begin();
}

void atualizarWeb() { servidor.handleClient(); }
