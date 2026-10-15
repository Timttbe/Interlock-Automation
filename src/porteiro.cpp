// Lógica da portaria (PORTEIRO).
//
// Entradas: botão da porta A, botão da porta B e chave do bypass. Saídas: buzzer de cada porta.
//
// A portaria é quem decide o intertravamento: as portas pedem para abrir (facial, botão) e ela
// responde; os botões e a página dela mandam a porta abrir. Como tudo passa por um lugar só, dois
// pedidos quase simultâneos nunca liberam as duas portas: assim que libera uma, a portaria marca
// essa porta como "em liberação" e segura a outra até a liberada confirmar.
//
// O buzzer de cada porta toca enquanto ela avisa que está aberta há mais que o tempo de alarme.

#include "papel.h"
#ifdef PAPEL_PORTEIRO

#include "porteiro.h"
#include "config.h"
#include "configuracao.h"
#include "logica.h"
#include "protocolo.h"
#include "registro.h"
#include "sistema.h"

namespace {

Entrada botaoA(PINO_BOTAO_A, LOW, DEBOUNCE_MS);
Entrada botaoB(PINO_BOTAO_B, LOW, DEBOUNCE_MS);
Entrada chave(PINO_CHAVE_BYPASS, LOW, DEBOUNCE_CHAVE_MS);
Saida buzzerA(PINO_BUZZER_A, BUZZER_ATIVO_EM);
Saida buzzerB(PINO_BUZZER_B, BUZZER_ATIVO_EM);
bool bypassPagina = false;  // não fica salvo: some se a portaria reiniciar

PortaNaPortaria portas[2];

// Comando LIBERAR esperando a porta confirmar (repetido até a resposta chegar)
struct Comando {
  bool ativo = false;
  uint32_t id = 0;
  Origem origem = Origem::DESCONHECIDA;
  bool bypass = false;
  unsigned long inicio = 0, ultimoEnvio = 0;
};
Comando comandos[2];

RespostaGuardada respostasPedido[2];  // último PEDIDO de cada porta

bool conflitoAvisado = false;
AgendaEstado agenda;

uint8_t idx(Papel porta) { return porta == Papel::PORTA_A ? 0 : 1; }
Saida& buzzer(uint8_t i) { return i == 0 ? buzzerA : buzzerB; }

void marcarEmLiberacao(Papel porta, unsigned long agora) {
  PortaNaPortaria& p = portas[idx(porta)];
  p.emLiberacao = true;
  p.emLiberacaoDesde = agora;
}

void novoResultado(PortaNaPortaria& p, bool liberou, Motivo motivo, Origem origem, unsigned long agora) {
  p.resultado = ResultadoComando();
  p.resultado.existe = true;
  p.resultado.liberou = liberou;
  p.resultado.motivo = motivo;
  p.resultado.origem = origem;
  p.resultado.quando = agora;
  if (!liberou) {
    registrar("%s: abertura negada (%s): %s", nomeExibicao(p.papel), textoOrigem(origem), textoMotivo(motivo));
  }
}

void enviarLiberar(uint8_t i, unsigned long agora) {
  Comando& c = comandos[i];
  String dados = "id=" + String(c.id) + ";byp=" + (c.bypass ? "1" : "0") + ";ori=" + codigoOrigem(c.origem);
  enviarPara(portas[i].papel, MSG_LIBERAR, dados);
  c.ultimoEnvio = agora;
}

void tratarEstadoPorta(uint8_t i, const char* dados, unsigned long agora) {
  PortaNaPortaria& p = portas[i];
  bool liberada = campoLogico(dados, "lib");
  bool alarme = campoLogico(dados, "alm");
  bool comBypass = campoLogico(dados, "lbp");
  Origem origem = campoOrigem(dados);
  char modo[4];
  campoTexto(dados, "modo", modo, sizeof(modo));

  if (p.noticia.recebida) {
    if (liberada && !p.liberada) {
      registrar("%s liberada (%s%s)", nomeExibicao(p.papel), textoOrigem(origem), comBypass ? ", bypass" : "");
      // O comando tinha ficado sem resposta, mas a porta abriu: só a confirmação se perdeu.
      if (p.resultado.existe && p.resultado.motivo == Motivo::SEM_RESPOSTA && agora - p.resultado.quando < 3000) {
        p.resultado.liberou = true;
        p.resultado.motivo = Motivo::LIBERADO;
      }
    }
    if (alarme && !p.alarme) registrar("ALARME: %s aberta há muito tempo (buzzer ligado)", nomeExibicao(p.papel));
    if (!alarme && p.alarme) registrar("%s fechada: alarme encerrado", nomeExibicao(p.papel));
  }

  p.noticia.chegou(agora);
  p.liberada = liberada;
  p.aberta = campoLogico(dados, "abe");
  p.alarme = alarme;
  p.comBypass = comBypass;
  p.restanteMs = campoNumero(dados, "rest", 0);
  p.abertaHaS = campoNumero(dados, "ha", 0);
  p.modo = modo[0] ? modo[0] : 'I';
  p.origem = origem;
  if (liberada) p.emLiberacao = false;  // a própria porta já avisa que está liberada
}

// O intertravamento visto pela portaria. 'conferirPorta': exigir notícia recente da própria porta
// (comando da portaria); num pedido não precisa, a porta acabou de mostrar que está comunicando.
Motivo avaliar(Papel porta, bool conferirPorta, unsigned long agora) {
  if (iniciando()) return Motivo::INICIANDO;  // num pedido, a porta então decide com a outra porta
  if (conferirPorta && !visaoDaPorta(porta, agora).comunicando) return Motivo::PORTA_SEM_COMUNICACAO;
  Papel outra = outraPorta(porta);
  return decidirAbertura(visaoDaPorta(outra, agora), portas[idx(outra)].emLiberacao, bypassAtivo(),
                         cfgOperacao.liberarSemComunicacao);
}

void tratarPedido(Papel porta, const char* dados, unsigned long agora) {
  uint8_t i = idx(porta);
  uint32_t id = campoId(dados);
  if (id == 0 || respostasPedido[i].repetida(porta, id)) return;
  Motivo motivo = avaliar(porta, false, agora);
  bool ok = motivoLibera(motivo);
  if (ok) marcarEmLiberacao(porta, agora);
  respostasPedido[i].responder(porta, id, motivo);
  novoResultado(portas[i], ok, motivo, campoOrigem(dados), agora);
}

void tratarResposta(uint8_t i, const char* dados, unsigned long agora) {
  Comando& c = comandos[i];
  if (!c.ativo || campoId(dados) != c.id) return;  // resposta atrasada de um comando já resolvido
  c.ativo = false;
  PortaNaPortaria& p = portas[i];
  // Só mexe no resultado mostrado se ele ainda for o deste comando (um pedido da própria porta
  // pode ter chegado no meio e ficado no lugar).
  bool resultadoDoComando = p.resultado.aguardando;
  p.resultado.aguardando = false;
  if (!campoLogico(dados, "ok")) {
    Motivo motivo = campoMotivo(dados);
    p.emLiberacao = false;
    if (resultadoDoComando) {
      p.resultado.liberou = false;
      p.resultado.motivo = motivo;
      p.resultado.quando = agora;
    }
    registrar("%s recusou abrir: %s", nomeExibicao(p.papel), textoMotivo(motivo));
  }
}

}  // namespace

const PortaNaPortaria& portaNaPortaria(Papel porta) { return portas[idx(porta)]; }

VisaoPorta visaoDaPorta(Papel porta, unsigned long agora) {
  const PortaNaPortaria& p = portas[idx(porta)];
  VisaoPorta v;
  v.comunicando = p.noticia.recente(agora);
  v.liberada = p.liberada;
  v.aberta = p.aberta;
  return v;
}

bool bypassAtivo() { return chave.ativa() || bypassPagina; }
bool bypassDaPagina() { return bypassPagina; }

void definirBypassDaPagina(bool ligado) {
  if (ligado == bypassPagina) return;
  bypassPagina = ligado;
  agenda.mudou();
  registrar("Bypass pela página %s", ligado ? "LIGADO" : "desligado");
}

Motivo avaliarAbertura(Papel porta, unsigned long agora) { return avaliar(porta, true, agora); }

Motivo abrirPelaPortaria(Papel porta, Origem origem, unsigned long agora) {
  uint8_t i = idx(porta);
  Motivo motivo = avaliarAbertura(porta, agora);
  bool ok = motivoLibera(motivo);
  novoResultado(portas[i], ok, motivo, origem, agora);
  if (!ok) return motivo;

  portas[i].resultado.aguardando = true;
  marcarEmLiberacao(porta, agora);
  Comando& c = comandos[i];
  c.ativo = true;
  c.id = novoId();
  c.origem = origem;
  c.bypass = motivo == Motivo::LIBERADO_BYPASS;
  c.inicio = agora;
  enviarLiberar(i, agora);
  return motivo;
}

void iniciarLogica() {
  buzzerA.iniciar();  // buzzers desligados antes de qualquer outra coisa
  buzzerB.iniciar();
  botaoA.iniciar();
  botaoB.iniciar();
  chave.iniciar();
  if (chave.ativa()) registrar("Chave do bypass LIGADA");
  portas[0].papel = Papel::PORTA_A;
  portas[1].papel = Papel::PORTA_B;
}

void atualizarLogica(unsigned long agora) {
  if (botaoA.atualizar(agora) && botaoA.ativa()) abrirPelaPortaria(Papel::PORTA_A, Origem::BOTAO_PORTARIA, agora);
  if (botaoB.atualizar(agora) && botaoB.ativa()) abrirPelaPortaria(Papel::PORTA_B, Origem::BOTAO_PORTARIA, agora);
  if (chave.atualizar(agora)) {
    agenda.mudou();
    registrar("Chave do bypass %s", chave.ativa() ? "LIGADA" : "desligada");
  }

  for (uint8_t i = 0; i < 2; i++) {
    PortaNaPortaria& p = portas[i];
    Comando& c = comandos[i];
    if (c.ativo) {
      if (agora - c.inicio >= PRAZO_COMANDO_MS) {
        // A "em liberação" continua até PRAZO_LIBERACAO_MS: a porta pode ter aberto e só a
        // resposta ter se perdido.
        c.ativo = false;
        if (p.resultado.aguardando) {
          p.resultado.aguardando = false;
          p.resultado.liberou = false;
          p.resultado.motivo = Motivo::SEM_RESPOSTA;
          p.resultado.quando = agora;
        }
        registrar("%s não confirmou o comando de abrir", nomeExibicao(p.papel));
      } else if (agora - c.ultimoEnvio >= REENVIO_COMANDO_MS) {
        enviarLiberar(i, agora);
      }
    }
    if (p.emLiberacao && agora - p.emLiberacaoDesde >= PRAZO_LIBERACAO_MS) p.emLiberacao = false;

    p.buzzer = visaoDaPorta(p.papel, agora).comunicando && p.alarme;
    bool tocar = p.buzzer && (!BUZZER_INTERMITENTE || (agora / 500) % 2 == 0);
    buzzer(i).ligar(tocar);
  }

  // Diagnóstico: as duas portas abertas ao mesmo tempo sem bypass (regra "liberar sem
  // comunicação" em uso, porta forçada...).
  VisaoPorta a = visaoDaPorta(Papel::PORTA_A, agora), b = visaoDaPorta(Papel::PORTA_B, agora);
  bool conflito = a.comunicando && b.comunicando && (a.liberada || a.aberta) && (b.liberada || b.aberta) &&
                  !bypassAtivo() && !portas[0].comBypass && !portas[1].comBypass;
  if (conflito && !conflitoAvisado) registrar("ATENÇÃO: as duas portas abertas ao mesmo tempo sem bypass");
  conflitoAvisado = conflito;

  bool direto;
  if (agenda.hora(agora, direto)) {
    char dados[40];
    snprintf(dados, sizeof(dados), "byp=%d;chv=%d;vir=%d", bypassAtivo(), chave.ativa(), bypassPagina);
    enviarEstado(dados, direto);
  }
}

void tratarMensagem(Papel origem, char tipo, const char* dados, unsigned long agora) {
  if (origem == Papel::PORTEIRO) return;
  uint8_t i = idx(origem);
  switch (tipo) {
    case MSG_ESTADO: tratarEstadoPorta(i, dados, agora); break;
    case MSG_PEDIDO: tratarPedido(origem, dados, agora); break;
    case MSG_RESPOSTA: tratarResposta(i, dados, agora); break;
    default: break;
  }
}

void colocarEmEstadoSeguro() {
  buzzerA.ligar(false);
  buzzerB.ligar(false);
}

const Entrada& botaoPortaA() { return botaoA; }
const Entrada& botaoPortaB() { return botaoB; }
const Entrada& chaveBypass() { return chave; }
const Saida& buzzerDaPorta(Papel porta) { return porta == Papel::PORTA_A ? buzzerA : buzzerB; }

#endif
