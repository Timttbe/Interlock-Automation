// Lógica de uma porta (PORTA_A ou PORTA_B).
//
// Entradas: sensor da porta, leitor facial e botão. Saídas (relés): eletroímã e sinaleira
// puxe/empurre (acesa enquanto a porta está liberada).
//
// Facial, botão e o botão da página pedem a abertura à portaria, que decide pelo intertravamento
// e responde. Com a portaria sem comunicação (ou acabando de ligar), a porta decide sozinha com o
// que sabe da outra porta, que avisa o próprio estado direto para esta. Se a portaria está no ar
// mas a resposta não chega em PRAZO_PEDIDO_MS, a abertura é negada (ver atualizarLogica).
// A portaria também pode mandar abrir (botões e página dela).
//
// Depois de liberada, a porta trava de novo quando o tempo de abertura acaba ou, antes disso,
// quando ela abre e fecha (TRAVAR_AO_FECHAR). Aberta por mais que o tempo de alarme, avisa a
// portaria, que liga o buzzer até a porta fechar.

#include "papel.h"
#ifdef PAPEL_E_PORTA

#include "porta.h"
#include "config.h"
#include "configuracao.h"
#include "logica.h"
#include "protocolo.h"
#include "registro.h"
#include "sistema.h"

namespace {

constexpr Papel OUTRA = MEU_PAPEL == Papel::PORTA_A ? Papel::PORTA_B : Papel::PORTA_A;
constexpr bool SOU_A = MEU_PAPEL == Papel::PORTA_A;

Entrada sensor(PINO_SENSOR, SENSOR_ABERTO_EM, DEBOUNCE_SENSOR_MS);
Entrada facial(PINO_FACIAL, LOW, DEBOUNCE_MS);
Entrada botao(PINO_BOTAO, LOW, DEBOUNCE_MS);
Saida ima(PINO_IMA, RELE_ATIVO_EM);  // relé ligado = eletroímã sem energia = porta destravada
Saida sinaleira(PINO_SINALEIRA, RELE_ATIVO_EM);

EstadoPorta est;
bool abriuNaLiberacao = false;  // o sensor abriu durante a liberação: trava quando fechar

// Pedido esperando a resposta da portaria
struct {
  bool ativo = false;
  uint32_t id = 0;
  Origem origem = Origem::DESCONHECIDA;
  unsigned long inicio = 0, ultimoEnvio = 0;
} pedido;

// Último estado recebido da outra porta e da portaria
struct {
  Noticia noticia;
  bool liberada = false, aberta = false;
} infoOutra;

struct {
  Noticia noticia;
  bool bypass = false;
} infoPortaria;

RespostaGuardada respostaLiberar;  // último LIBERAR da portaria
AgendaEstado agenda;
char modoAnterior = 0;

VisaoPorta visaoOutra(unsigned long agora) {
  VisaoPorta v;
  v.comunicando = infoOutra.noticia.recente(agora);
  v.liberada = infoOutra.liberada;
  v.aberta = infoOutra.aberta;
  return v;
}

bool portariaComunicando(unsigned long agora) { return infoPortaria.noticia.recente(agora); }
bool bypassConhecido(unsigned long agora) { return portariaComunicando(agora) && infoPortaria.bypass; }

char calcularModo(unsigned long agora) {
  if (portariaComunicando(agora)) return 'C';
  if (visaoOutra(agora).comunicando) return 'P';
  return 'I';
}

const char* textoModo(char modo) {
  switch (modo) {
    case 'C': return "com a portaria";
    case 'P': return "sem a portaria, decidindo com a outra porta";
    default: return "isolada (sem a portaria e sem a outra porta)";
  }
}

void liberar(Origem origem, bool comBypass, unsigned long agora) {
  bool renovacao = est.liberada;
  est.liberada = true;
  est.liberadaEm = agora;
  est.duracaoMs = cfgOperacao.tempoAberturaS * 1000UL;
  est.comBypass = comBypass;
  est.origem = origem;
  abriuNaLiberacao = sensor.ativa();  // se já estava aberta, trava assim que fechar
  ima.ligar(true);
  sinaleira.ligar(true);
  agenda.mudou();
  registrar("%s (%s%s)", renovacao ? "Liberação renovada" : "Porta liberada", textoOrigem(origem),
            comBypass ? ", bypass" : "");
}

void travar(const char* motivo) {
  if (!est.liberada) return;
  est.liberada = false;
  ima.ligar(false);
  sinaleira.ligar(false);
  agenda.mudou();
  registrar("Porta travada: %s", motivo);
}

void anotarResultado(bool liberou, Motivo motivo, Origem origem, bool pelaPortaria, unsigned long agora) {
  est.resultado.existe = true;
  est.resultado.liberou = liberou;
  est.resultado.motivo = motivo;
  est.resultado.origem = origem;
  est.resultado.pelaPortaria = pelaPortaria;
  est.resultado.quando = agora;
  if (!liberou) {
    registrar("Abertura negada (%s)%s: %s", textoOrigem(origem), pelaPortaria ? "" : " sem a portaria",
              textoMotivo(motivo));
  }
}

// Conflito que esta porta enxerga e a portaria talvez ainda não (a outra porta acabou de abrir).
Motivo vetoLocal(bool comBypass, unsigned long agora) {
  if (comBypass) return Motivo::LIBERADO_BYPASS;
  VisaoPorta o = visaoOutra(agora);
  if (o.comunicando && o.liberada) return Motivo::OUTRA_LIBERADA;
  if (o.comunicando && o.aberta) return Motivo::OUTRA_ABERTA;
  return Motivo::LIBERADO;
}

// Portaria sem comunicação: decide com o que sabe da outra porta.
void decidirSemPortaria(Origem origem, unsigned long agora) {
  Motivo m = iniciando() ? Motivo::INICIANDO
                         : decidirAbertura(visaoOutra(agora), false, bypassConhecido(agora),
                                           cfgOperacao.liberarSemComunicacao);
  bool ok = motivoLibera(m);
  if (ok) liberar(origem, m == Motivo::LIBERADO_BYPASS, agora);
  anotarResultado(ok, m, origem, false, agora);
}

void enviarPedido(unsigned long agora) {
  String dados = "id=" + String(pedido.id) + ";ori=" + codigoOrigem(pedido.origem);
  enviarPara(Papel::PORTEIRO, MSG_PEDIDO, dados);
  pedido.ultimoEnvio = agora;
}

void avisarEstado(unsigned long agora, bool direto) {
  char dados[112];
  unsigned long abertaHaS = sensor.ativa() ? (agora - sensor.mudouEm()) / 1000 : 0;
  snprintf(dados, sizeof(dados), "lib=%d;abe=%d;alm=%d;rest=%lu;ha=%lu;lbp=%d;ori=%c;modo=%c", est.liberada,
           sensor.ativa(), est.alarme, restanteLiberacaoMs(agora), abertaHaS, est.comBypass,
           codigoOrigem(est.origem), est.modo);
  enviarEstado(dados, direto);
}

// Resposta da portaria a um pedido desta porta.
void tratarResposta(const char* dados, unsigned long agora) {
  if (!pedido.ativo || campoId(dados) != pedido.id) return;  // resposta atrasada de um pedido já resolvido
  pedido.ativo = false;
  bool ok = campoLogico(dados, "ok");
  Motivo motivo = campoMotivo(dados);
  bool comBypass = campoLogico(dados, "byp");
  if (motivo == Motivo::INICIANDO) {
    // A portaria acabou de ligar e ainda não sabe o estado das portas: decide com a outra porta.
    decidirSemPortaria(pedido.origem, agora);
    return;
  }
  if (ok) {
    Motivo veto = vetoLocal(comBypass, agora);
    if (!motivoLibera(veto)) {
      ok = false;
      motivo = veto;
    }
  }
  if (ok) liberar(pedido.origem, comBypass, agora);
  anotarResultado(ok, motivo, pedido.origem, true, agora);
}

// A portaria mandou abrir (botão ou página da portaria).
void tratarLiberar(const char* dados, unsigned long agora) {
  uint32_t id = campoId(dados);
  if (id == 0 || respostaLiberar.repetida(Papel::PORTEIRO, id)) return;
  bool comBypass = campoLogico(dados, "byp");
  Origem origem = campoOrigem(dados);
  Motivo motivo = vetoLocal(comBypass, agora);
  bool ok = motivoLibera(motivo);
  if (ok) liberar(origem, comBypass, agora);
  respostaLiberar.responder(Papel::PORTEIRO, id, motivo);
  anotarResultado(ok, motivo, origem, true, agora);
}

}  // namespace

void pedirAbertura(Origem origem, unsigned long agora) {
  if (pedido.ativo) return;  // já tem um pedido esperando a portaria
  if (portariaComunicando(agora)) {
    pedido.ativo = true;
    pedido.id = novoId();
    pedido.origem = origem;
    pedido.inicio = agora;
    enviarPedido(agora);
  } else {
    decidirSemPortaria(origem, agora);
  }
}

void iniciarLogica() {
  ima.iniciar();  // relés desligados antes de qualquer outra coisa: porta travada
  sinaleira.iniciar();
  sensor.iniciar();
  facial.iniciar();
  botao.iniciar();
}

void atualizarLogica(unsigned long agora) {
  if (facial.atualizar(agora) && facial.ativa()) pedirAbertura(Origem::FACIAL, agora);
  if (botao.atualizar(agora) && botao.ativa()) pedirAbertura(Origem::BOTAO, agora);

  if (sensor.atualizar(agora)) {
    agenda.mudou();
    if (sensor.ativa()) {
      if (est.liberada) {
        abriuNaLiberacao = true;
      } else {
        registrar("Porta abriu sem estar liberada (forçada, ou eletroímã sem força?)");
      }
    } else {
      if (est.alarme) registrar("Porta fechada: alarme encerrado");
      est.alarme = false;
      if (TRAVAR_AO_FECHAR && est.liberada && abriuNaLiberacao) travar("a porta abriu e fechou");
    }
  }

  if (est.liberada && agora - est.liberadaEm >= est.duracaoMs) travar("tempo de abertura acabou");

  if (!est.alarme && sensor.ativa() && agora - sensor.mudouEm() >= cfgOperacao.tempoAlarmeS * 1000UL) {
    est.alarme = true;
    agenda.mudou();
    registrar("ALARME: porta aberta há mais de %u s", cfgOperacao.tempoAlarmeS);
  }

  if (est.liberada && deveCancelarLiberacao(SOU_A, sensor.ativa(), est.comBypass, visaoOutra(agora))) {
    travar("a outra porta também foi liberada ao mesmo tempo (conflito resolvido)");
  }

  if (pedido.ativo) {
    if (agora - pedido.inicio >= PRAZO_PEDIDO_MS) {
      pedido.ativo = false;
      if (portariaComunicando(agora)) {
        // A portaria está no ar (avisou o estado há pouco) e só a resposta não veio. Decidir
        // sozinha agora poderia liberar junto com uma porta que ela acabou de liberar e cujo
        // aviso também se perdeu: melhor negar, e a pessoa tenta de novo.
        anotarResultado(false, Motivo::PORTARIA_SEM_RESPOSTA, pedido.origem, true, agora);
      } else {
        registrar("A portaria parou de responder: decidindo sem ela");
        decidirSemPortaria(pedido.origem, agora);
      }
    } else if (agora - pedido.ultimoEnvio >= REENVIO_COMANDO_MS) {
      enviarPedido(agora);
    }
  }

  est.modo = calcularModo(agora);
  if (est.modo != modoAnterior) {
    if (modoAnterior) registrar("Funcionando %s", textoModo(est.modo));
    modoAnterior = est.modo;
    agenda.mudou();
  }

  bool direto;
  if (agenda.hora(agora, direto)) avisarEstado(agora, direto);
}

void tratarMensagem(Papel origem, char tipo, const char* dados, unsigned long agora) {
  if (tipo == MSG_ESTADO) {
    if (origem == OUTRA) {
      infoOutra.noticia.chegou(agora);
      infoOutra.liberada = campoLogico(dados, "lib");
      infoOutra.aberta = campoLogico(dados, "abe");
    } else if (origem == Papel::PORTEIRO) {
      infoPortaria.noticia.chegou(agora);
      infoPortaria.bypass = campoLogico(dados, "byp");
    }
  } else if (origem == Papel::PORTEIRO && tipo == MSG_RESPOSTA) {
    tratarResposta(dados, agora);
  } else if (origem == Papel::PORTEIRO && tipo == MSG_LIBERAR) {
    tratarLiberar(dados, agora);
  }
}

void colocarEmEstadoSeguro() {
  travar("a placa vai reiniciar ou gravar firmware");
  ima.ligar(false);
  sinaleira.ligar(false);
}

const EstadoPorta& estadoPorta(unsigned long agora) {
  est.pedidoPendente = pedido.ativo;
  est.portariaComunicando = portariaComunicando(agora);
  est.bypassPortaria = infoPortaria.bypass;
  est.outra = visaoOutra(agora);
  return est;
}

unsigned long restanteLiberacaoMs(unsigned long agora) {
  if (!est.liberada) return 0;
  unsigned long passou = agora - est.liberadaEm;
  return passou >= est.duracaoMs ? 0 : est.duracaoMs - passou;
}

const Entrada& sensorDaPorta() { return sensor; }
const Entrada& facialDaPorta() { return facial; }
const Entrada& botaoDaPorta() { return botao; }
const Saida& imaDaPorta() { return ima; }
const Saida& sinaleiraDaPorta() { return sinaleira; }

#endif
