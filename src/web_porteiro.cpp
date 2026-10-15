// Página e rotas da portaria:
//   POST /api/abrir           porta=A ou B (igual aos botões da portaria)
//   POST /api/bypass          ligar=1 ou 0 (bypass pela página; a chave física vale independente)
//   POST /api/operacao        regra para porta sem comunicação
//   POST /api/contas/criar    admin: nome, senha
//   POST /api/contas/apagar   admin: nome
//   POST /api/contas/senha    admin: nome, senha (define a senha de qualquer conta)
//   POST /api/minha-senha     atual, nova (qualquer conta, a própria senha)

#include "papel.h"
#ifdef PAPEL_PORTEIRO

#include "acesso.h"
#include "config.h"
#include "configuracao.h"
#include "contas.h"
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

// Sem login: a página só tem o código; os dados vêm de /api/estado, que pede login.
void tratarPagina() {
  static const char* const partes[] = {PAGINA_INICIO, CSS_COMUM, PAGINA_CABECALHO, CORPO_PORTEIRO, SECOES_COMUNS,
                                       PAGINA_SCRIPT, JS_COMUM, JS_PORTEIRO, PAGINA_FIM};
  enviarPagina(partes, sizeof(partes) / sizeof(partes[0]));
}

void tratarAbrir() {
  String usuario;
  if (!exigirLogin(&usuario)) return;
  String porta = servidor.arg("porta");
  if (porta != "A" && porta != "B") return responderErro("Porta inválida.");
  Papel papel = porta == "A" ? Papel::PORTA_A : Papel::PORTA_B;
  registrar("%s pediu pela página para abrir a %s", usuario.c_str(), nomeExibicao(papel));
  Motivo m = abrirPelaPortaria(papel, Origem::PAGINA_PORTARIA, millis());
  if (motivoLibera(m)) return responderOk(String(nomeExibicao(papel)) + ": " + textoMotivo(m) + ", esperando a porta confirmar");
  responderErro(String(nomeExibicao(papel)) + ": abertura negada, " + textoMotivo(m));
}

void tratarBypass() {
  String usuario;
  if (!exigirLogin(&usuario)) return;
  bool ligar = servidor.arg("ligar") == "1";
  definirBypassDaPagina(ligar, usuario.c_str());
  responderOk(ligar ? "Bypass da página ligado" : "Bypass da página desligado");
}

void tratarOperacao() {
  String usuario;
  if (!exigirLogin(&usuario)) return;
  cfgOperacao.liberarSemComunicacao = servidor.arg("semComunicacao") != "0";
  salvarOperacao();
  registrar("Funcionamento alterado (%s): porta sem comunicação %s a outra", usuario.c_str(),
            cfgOperacao.liberarSemComunicacao ? "libera" : "bloqueia");
  responderOk("Salvo. Já está valendo.");
}

// ===== Contas (as portas recebem uma cópia; ver contas.h) =====

String nomeDoPedido() {
  String nome = servidor.arg("nome");
  nome.trim();
  nome.toLowerCase();
  return nome;
}

void responder(const String& erro, const String& mensagem) {
  if (erro.isEmpty()) {
    responderOk(mensagem);
  } else {
    responderErro(erro);
  }
}

void tratarCriarConta() {
  if (!exigirAdmin()) return;
  String nome = nomeDoPedido();
  responder(criarConta(nome, servidor.arg("senha")), "Conta \"" + nome + "\" criada.");
}

void tratarApagarConta() {
  if (!exigirAdmin()) return;
  String nome = nomeDoPedido();
  String erro = apagarConta(nome);
  if (erro.isEmpty()) encerrarSessoesDe(nome.c_str(), false);
  responder(erro, "Conta \"" + nome + "\" apagada.");
}

// O admin define a senha de qualquer conta (inclusive a dele).
void tratarSenhaDaConta() {
  String admin;
  if (!exigirAdmin(&admin)) return;
  String nome = nomeDoPedido();
  String erro = definirSenha(nome, servidor.arg("senha"));
  if (erro.isEmpty()) encerrarSessoesDe(nome.c_str(), nome == admin);
  responder(erro, "Senha de \"" + nome + "\" trocada.");
}

// Qualquer conta troca a própria senha, informando a atual.
void tratarMinhaSenha() {
  String usuario;
  if (!exigirLogin(&usuario)) return;
  Nivel nivel;
  if (!conferirSenha(usuario.c_str(), servidor.arg("atual").c_str(), nivel)) {
    return responderErro("A senha atual não confere.");
  }
  String erro = definirSenha(usuario, servidor.arg("nova"));
  if (erro.isEmpty()) encerrarSessoesDe(usuario.c_str(), true);
  responder(erro, "Sua senha foi trocada.");
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
  s.on("/api/contas/criar", HTTP_POST, tratarCriarConta);
  s.on("/api/contas/apagar", HTTP_POST, tratarApagarConta);
  s.on("/api/contas/senha", HTTP_POST, tratarSenhaDaConta);
  s.on("/api/minha-senha", HTTP_POST, tratarMinhaSenha);
}

#endif
