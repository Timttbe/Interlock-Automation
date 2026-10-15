// Página e rotas da portaria:
//   POST /api/abrir      porta=A ou B (igual aos botões da portaria)
//   POST /api/bypass     ligar=1 ou 0 (bypass pela página; a chave física vale independente)
//   POST /api/operacao   regra para porta sem comunicação

#include "papel.h"
#ifdef PAPEL_PORTEIRO

#include "config.h"
#include "configuracao.h"
#include "json.h"
#include "logica.h"
#include "pagina_comum.h"
#include "pagina_porteiro.h"
#include "porteiro.h"
#include "registro.h"
#include "web.h"

namespace {

const char* textoPermissao(Motivo m) {
  switch (m) {
    case Motivo::LIBERADO: return "pode abrir";
    case Motivo::LIBERADO_BYPASS: return "pode abrir (bypass)";
    case Motivo::LIBERADO_SEM_COMUNICACAO: return "pode abrir (outra porta sem comunicação)";
    default: return textoMotivo(m);
  }
}

void portaJson(JsonEscritor& j, Papel papel, unsigned long agora) {
  const PortaNaPortaria& p = portaNaPortaria(papel);
  VisaoPorta v = visaoDaPorta(papel, agora);
  Motivo permissao = avaliarAbertura(papel, agora);
  unsigned long desde = agora - p.noticia.em;  // o tempo restante e o "aberta há" vieram nesse estado
  j.abrirObjeto();
  j.texto("letra", papel == Papel::PORTA_A ? "A" : "B");
  j.texto("nome", nomeExibicao(papel));
  j.logico("comunicando", v.comunicando);
  j.logico("liberada", p.liberada);
  j.logico("aberta", p.aberta);
  j.logico("alarme", p.alarme);
  j.numeroU("restante", p.restanteMs > desde ? p.restanteMs - desde : 0);
  j.numeroU("abertaHa", p.aberta ? p.abertaHaS + desde / 1000 : 0);
  j.texto("modo", String(p.modo));
  j.texto("origem", textoOrigem(p.origem));
  j.logico("comBypass", p.comBypass);
  j.logico("buzzer", p.buzzer);
  j.logico("podeAbrir", motivoLibera(permissao));
  j.texto("permissao", textoPermissao(permissao));
  if (p.resultado.existe) {
    j.abrirObjeto("resultado");
    j.logico("aguardando", p.resultado.aguardando);
    j.logico("liberou", p.resultado.liberou);
    j.texto("motivo", textoMotivo(p.resultado.motivo));
    j.texto("origem", textoOrigem(p.resultado.origem));
    j.numeroU("ha", (agora - p.resultado.quando) / 1000);
    j.fecharObjeto();
  } else {
    j.nulo("resultado");
  }
  j.fecharObjeto();
}

void tratarPagina() {
  if (!autorizado()) return;
  static const char* const partes[] = {PAGINA_INICIO, CSS_COMUM, PAGINA_CABECALHO, CORPO_PORTEIRO, SECOES_COMUNS,
                                       PAGINA_SCRIPT, JS_COMUM, JS_PORTEIRO, PAGINA_FIM};
  enviarPagina(partes, sizeof(partes) / sizeof(partes[0]));
}

void tratarAbrir() {
  if (!autorizado()) return;
  String porta = servidor.arg("porta");
  if (porta != "A" && porta != "B") return responderErro("Porta inválida.");
  Papel papel = porta == "A" ? Papel::PORTA_A : Papel::PORTA_B;
  Motivo m = abrirPelaPortaria(papel, Origem::PAGINA_PORTARIA, millis());
  if (motivoLibera(m)) return responderOk(String(nomeExibicao(papel)) + ": " + textoMotivo(m) + ", esperando a porta confirmar");
  responderErro(String(nomeExibicao(papel)) + ": abertura negada, " + textoMotivo(m));
}

void tratarBypass() {
  if (!autorizado()) return;
  bool ligar = servidor.arg("ligar") == "1";
  definirBypassDaPagina(ligar);
  responderOk(ligar ? "Bypass da página ligado" : "Bypass da página desligado");
}

void tratarOperacao() {
  if (!autorizado()) return;
  cfgOperacao.liberarSemComunicacao = servidor.arg("semComunicacao") != "0";
  salvarOperacao();
  registrar("Funcionamento alterado pela página: porta sem comunicação %s a outra",
            cfgOperacao.liberarSemComunicacao ? "libera" : "bloqueia");
  responderOk("Salvo. Já está valendo.");
}

}  // namespace

void logicaJson(JsonEscritor& j, unsigned long agora) {
  j.abrirObjeto("portaria");
  j.logico("bypass", bypassAtivo());
  j.logico("chave", chaveBypass().ativa());
  j.logico("virtual", bypassDaPagina());

  j.abrirLista("portas");
  portaJson(j, Papel::PORTA_A, agora);
  portaJson(j, Papel::PORTA_B, agora);
  j.fecharLista();

  j.abrirLista("entradas");
  entradaJson(j, "Botão da porta A", botaoPortaA(), "apertado", "solto", true, agora);
  entradaJson(j, "Botão da porta B", botaoPortaB(), "apertado", "solto", true, agora);
  entradaJson(j, "Chave do bypass", chaveBypass(), "ligada", "desligada", false, agora);
  j.fecharLista();

  j.abrirLista("saidas");
  saidaJson(j, "Buzzer da porta A", buzzerDaPorta(Papel::PORTA_A), "tocando", "desligado");
  saidaJson(j, "Buzzer da porta B", buzzerDaPorta(Papel::PORTA_B), "tocando", "desligado");
  j.fecharLista();

  j.abrirObjeto("cfg");
  j.logico("liberarSemComunicacao", cfgOperacao.liberarSemComunicacao);
  j.fecharObjeto();
  j.fecharObjeto();
}

void registrarRotasLogica(WebServer& s) {
  s.on("/", HTTP_GET, tratarPagina);
  s.on("/api/abrir", HTTP_POST, tratarAbrir);
  s.on("/api/bypass", HTTP_POST, tratarBypass);
  s.on("/api/operacao", HTTP_POST, tratarOperacao);
}

#endif
