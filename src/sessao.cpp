#include "sessao.h"

namespace {

// Janela deslizante de 32 mensagens (a mesma ideia do anti-replay do IPsec).
bool aceitarSeq(SessaoPar& par, uint32_t seq) {
  if (seq > par.ultimoSeq) {
    uint32_t avanco = seq - par.ultimoSeq;
    par.janela = avanco >= 32 ? 0 : par.janela << avanco;
    par.janela |= 1;
    par.ultimoSeq = seq;
    return true;
  }
  uint32_t atraso = par.ultimoSeq - seq;
  if (atraso >= 32) return false;
  uint32_t bit = 1u << atraso;
  if (par.janela & bit) return false;
  par.janela |= bit;
  return true;
}

}  // namespace

Aceite avaliarMensagem(SessaoPar& par, uint32_t minhaSessao, uint32_t sessao, uint32_t seq, uint32_t ecoParaMim) {
  if (sessao == 0) return Aceite::SEM_ECO;  // nenhuma placa usa a sessão 0
  for (uint32_t antiga : par.antigas) {
    if (antiga == sessao) return Aceite::SESSAO_ANTIGA;
  }

  if (sessao == par.sessao) {
    if (!aceitarSeq(par, seq)) return Aceite::REPETIDA;
    par.sessaoVista = sessao;
    return Aceite::ACEITA;
  }

  // Sessão diferente da aceita. O próximo eco para essa placa leva a sessão dela; quando ela
  // devolver o eco com a nossa, a sessão nova é aceita.
  par.sessaoVista = sessao;
  if (ecoParaMim != minhaSessao) return Aceite::SEM_ECO;

  if (par.sessao != 0) {
    par.antigas[par.proximaAntiga] = par.sessao;
    par.proximaAntiga = (par.proximaAntiga + 1) % SESSOES_ANTIGAS;
  }
  par.sessao = sessao;
  par.ultimoSeq = seq;
  // Tudo antes desta mensagem conta como já recebido. Sem isso, depois que esta placa reinicia,
  // uma mensagem gravada pouco antes do reinício (na mesma sessão da outra placa) ainda cairia
  // dentro da janela e seria aceita.
  par.janela = 0xFFFFFFFFu;
  return Aceite::NOVA_SESSAO;
}
