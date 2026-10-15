#include "intertravamento.h"

bool motivoLibera(Motivo m) {
  return m == Motivo::LIBERADO || m == Motivo::LIBERADO_BYPASS || m == Motivo::LIBERADO_SEM_COMUNICACAO;
}

const char* textoMotivo(Motivo m) {
  switch (m) {
    case Motivo::LIBERADO: return "liberada";
    case Motivo::LIBERADO_BYPASS: return "liberada pelo bypass";
    case Motivo::LIBERADO_SEM_COMUNICACAO: return "liberada sem notícia da outra porta";
    case Motivo::OUTRA_LIBERADA: return "a outra porta está destravada";
    case Motivo::OUTRA_ABERTA: return "a outra porta está aberta";
    case Motivo::OUTRA_EM_LIBERACAO: return "a outra porta está sendo liberada";
    case Motivo::OUTRA_SEM_COMUNICACAO: return "sem notícia da outra porta (regra: bloquear)";
    case Motivo::PORTA_SEM_COMUNICACAO: return "a porta está sem comunicação";
    case Motivo::INICIANDO: return "placa acabou de ligar, aguarde alguns segundos";
    case Motivo::SEM_RESPOSTA: return "a porta não respondeu";
    case Motivo::PORTARIA_SEM_RESPOSTA: return "a portaria não respondeu, tente de novo";
    case Motivo::DESCONHECIDO: break;
  }
  return "motivo desconhecido (firmwares de versões diferentes?)";
}

Motivo motivoDoNumero(long numero) {
  if (numero < 0 || numero > MOTIVO_MAXIMO) return Motivo::DESCONHECIDO;
  return static_cast<Motivo>(numero);
}

char codigoOrigem(Origem o) {
  switch (o) {
    case Origem::FACIAL: return 'F';
    case Origem::BOTAO: return 'B';
    case Origem::PAGINA_PORTA: return 'W';
    case Origem::BOTAO_PORTARIA: return 'P';
    case Origem::PAGINA_PORTARIA: return 'Q';
    case Origem::DESCONHECIDA: break;
  }
  return '?';
}

Origem origemDoCodigo(char codigo) {
  switch (codigo) {
    case 'F': return Origem::FACIAL;
    case 'B': return Origem::BOTAO;
    case 'W': return Origem::PAGINA_PORTA;
    case 'P': return Origem::BOTAO_PORTARIA;
    case 'Q': return Origem::PAGINA_PORTARIA;
    default: return Origem::DESCONHECIDA;
  }
}

const char* textoOrigem(Origem o) {
  switch (o) {
    case Origem::FACIAL: return "facial";
    case Origem::BOTAO: return "botão";
    case Origem::PAGINA_PORTA: return "página da porta";
    case Origem::BOTAO_PORTARIA: return "botão da portaria";
    case Origem::PAGINA_PORTARIA: return "página da portaria";
    case Origem::DESCONHECIDA: break;
  }
  return "origem desconhecida";
}

Motivo decidirAbertura(const VisaoPorta& outra, bool outraEmLiberacao, bool bypass, bool liberarSemComunicacao) {
  if (bypass) return Motivo::LIBERADO_BYPASS;
  if (outraEmLiberacao) return Motivo::OUTRA_EM_LIBERACAO;
  if (!outra.comunicando) {
    return liberarSemComunicacao ? Motivo::LIBERADO_SEM_COMUNICACAO : Motivo::OUTRA_SEM_COMUNICACAO;
  }
  if (outra.liberada) return Motivo::OUTRA_LIBERADA;
  if (outra.aberta) return Motivo::OUTRA_ABERTA;
  return Motivo::LIBERADO;
}

bool deveCancelarLiberacao(bool souPortaA, bool minhaPortaAberta, bool liberacaoComBypass, const VisaoPorta& outra) {
  if (liberacaoComBypass || minhaPortaAberta || !outra.comunicando) return false;
  if (outra.aberta) return true;      // a outra já abriu de fato: esta, ainda fechada, trava
  if (outra.liberada) return !souPortaA;  // as duas liberadas e fechadas: a B cede
  return false;
}
