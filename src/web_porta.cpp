// Página e rotas da porta:
//   POST /api/abrir      igual ao botão da porta (pede à portaria)
//   POST /api/operacao   tempo de abertura, tempo do alarme e regra sem comunicação

#include "papel.h"
#ifdef PAPEL_E_PORTA

#include "acesso.h"
#include "config.h"
#include "configuracao.h"
#include "json.h"
#include "logica.h"
#include "pagina_comum.h"
#include "pagina_porta.h"
#include "porta.h"
#include "registro.h"
#include "web.h"

namespace {

// Sem login: a página só tem o código; os dados vêm de /api/estado, que pede login.
void tratarPagina() {
  static const char* const partes[] = {PAGINA_INICIO, CSS_COMUM, PAGINA_CABECALHO, CORPO_PORTA, SECOES_COMUNS,
                                       PAGINA_SCRIPT, JS_COMUM, JS_PORTA, PAGINA_FIM};
  enviarPagina(partes, sizeof(partes) / sizeof(partes[0]));
}

void tratarAbrir() {
  String usuario;
  if (!exigirLogin(&usuario)) return;
  registrar("%s pediu pela página para abrir a porta", usuario.c_str());
  unsigned long agora = millis();
  pedirAbertura(Origem::PAGINA_PORTA, agora);
  const EstadoPorta& e = estadoPorta(agora);
  if (e.pedidoPendente) return responderOk("Pedido enviado à portaria");
  // A portaria está sem comunicação: a porta decidiu na hora.
  if (e.resultado.liberou) return responderOk("Porta liberada (decidido sem a portaria)");
  responderErro(String("Abertura negada: ") + textoMotivo(e.resultado.motivo));
}

void tratarOperacao() {
  String usuario;
  if (!exigirLogin(&usuario)) return;
  long abertura = servidor.arg("tempoAbertura").toInt();
  long alarme = servidor.arg("tempoAlarme").toInt();
  if (abertura < TEMPO_ABERTURA_MIN_S || abertura > TEMPO_ABERTURA_MAX_S) {
    return responderErro("Tempo de abertura: de 1 a 60 segundos.");
  }
  if (alarme < TEMPO_ALARME_MIN_S || alarme > TEMPO_ALARME_MAX_S) {
    return responderErro("Tempo do alarme: de 5 a 3600 segundos.");
  }
  cfgOperacao.tempoAberturaS = abertura;
  cfgOperacao.tempoAlarmeS = alarme;
  cfgOperacao.liberarSemComunicacao = servidor.arg("semComunicacao") != "0";
  salvarOperacao();
  registrar("Funcionamento alterado (%s): abertura %ld s, alarme %ld s, sem comunicação: %s", usuario.c_str(),
            abertura, alarme, cfgOperacao.liberarSemComunicacao ? "libera" : "bloqueia");
  responderOk("Salvo. Já está valendo.");
}

}  // namespace

void logicaJson(JsonEscritor& j, unsigned long agora) {
  const EstadoPorta& e = estadoPorta(agora);
  const Entrada& sensor = sensorDaPorta();
  j.abrirObjeto("porta");
  j.logico("liberada", e.liberada);
  j.numeroU("restante", restanteLiberacaoMs(agora));
  j.texto("origem", textoOrigem(e.origem));
  j.logico("comBypass", e.comBypass);
  j.logico("aberta", sensor.ativa());
  j.numeroU("abertaHa", sensor.ativa() ? (agora - sensor.mudouEm()) / 1000 : 0);
  j.logico("alarme", e.alarme);
  j.logico("sinaleira", sinaleiraDaPorta().ligada());
  j.texto("modo", String(e.modo));
  j.logico("pedido", e.pedidoPendente);
  j.logico("portaria", e.portariaComunicando);
  j.logico("bypass", e.bypassPortaria);

  j.abrirObjeto("outra");
  j.texto("nome", nomeExibicao(outraPorta(MEU_PAPEL)));
  j.logico("comunicando", e.outra.comunicando);
  j.logico("liberada", e.outra.liberada);
  j.logico("aberta", e.outra.aberta);
  j.fecharObjeto();

  if (e.resultado.existe) {
    j.abrirObjeto("resultado");
    j.logico("liberou", e.resultado.liberou);
    j.texto("motivo", textoMotivo(e.resultado.motivo));
    j.texto("origem", textoOrigem(e.resultado.origem));
    j.texto("decidiu", e.resultado.pelaPortaria ? "pela portaria" : "pela própria porta");
    j.numeroU("ha", (agora - e.resultado.quando) / 1000);
    j.fecharObjeto();
  } else {
    j.nulo("resultado");
  }

  j.abrirLista("entradas");
  entradaJson(j, "Sensor da porta", sensor, "aberta", "fechada", false, agora);
  entradaJson(j, "Facial", facialDaPorta(), "acionado", "em repouso", true, agora);
  entradaJson(j, "Botão", botaoDaPorta(), "apertado", "solto", true, agora);
  j.fecharLista();

  j.abrirLista("saidas");
  saidaJson(j, "Relé do eletroímã", imaDaPorta(), "ligado (ímã solto)", "desligado (ímã travando)");
  saidaJson(j, "Relé da sinaleira", sinaleiraDaPorta(), "ligado (acesa)", "desligado (apagada)");
  j.fecharLista();

  j.abrirObjeto("cfg");
  j.numero("tempoAbertura", cfgOperacao.tempoAberturaS);
  j.numero("tempoAlarme", cfgOperacao.tempoAlarmeS);
  j.logico("liberarSemComunicacao", cfgOperacao.liberarSemComunicacao);
  j.fecharObjeto();
  j.fecharObjeto();
}

void registrarRotasLogica(WebServer& s) {
  s.on("/", HTTP_GET, tratarPagina);
  s.on("/api/abrir", HTTP_POST, tratarAbrir);
  s.on("/api/operacao", HTTP_POST, tratarOperacao);
}

#endif
