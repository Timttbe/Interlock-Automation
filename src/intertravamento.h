#pragma once
// Regras do intertravamento: quando uma porta pode abrir.
//
// Só funções puras (sem pinos, rede ou relógio), usadas pela portaria e pelas portas, e
// testadas em test/test_intertravamento.
//
// Uma porta pode abrir se a outra estiver travada E fechada:
//   - "liberada": o eletroímã está solto (o tempo de abertura está correndo);
//   - "aberta": o sensor diz que a porta está aberta. Vale mesmo depois do tempo de abertura
//     acabar: enquanto a porta não fechar, a outra não abre.
// Com o bypass ligado, as duas podem abrir ao mesmo tempo.
//
// Quem decide é a portaria (um só árbitro, então dois pedidos quase simultâneos nunca liberam as
// duas portas). Sem a portaria, cada porta decide com o que sabe da outra; se nem da outra ela
// tem notícia, vale a regra "sem comunicação" (liberar ou bloquear), configurável na página.

#include <stdint.h>

// O que uma placa sabe de uma porta.
struct VisaoPorta {
  bool comunicando = false;  // recebeu o estado dela há pouco
  bool liberada = false;     // eletroímã solto
  bool aberta = false;       // sensor indica aberta
};

// Resultado de um pedido de abertura. Os números vão nas mensagens entre as placas:
// não mude os que já existem.
enum class Motivo : uint8_t {
  LIBERADO = 0,
  LIBERADO_BYPASS = 1,
  LIBERADO_SEM_COMUNICACAO = 2,
  OUTRA_LIBERADA = 3,
  OUTRA_ABERTA = 4,
  OUTRA_EM_LIBERACAO = 5,
  OUTRA_SEM_COMUNICACAO = 6,
  PORTA_SEM_COMUNICACAO = 7,
  INICIANDO = 8,
  SEM_RESPOSTA = 9,
  DESCONHECIDO = 10,
  PORTARIA_SEM_RESPOSTA = 11,
};
constexpr long MOTIVO_MAXIMO = 11;

// Quem pediu a abertura. Os códigos vão nas mensagens entre as placas.
enum class Origem : uint8_t { FACIAL, BOTAO, PAGINA_PORTA, BOTAO_PORTARIA, PAGINA_PORTARIA, DESCONHECIDA };

bool motivoLibera(Motivo m);
const char* textoMotivo(Motivo m);
Motivo motivoDoNumero(long numero);

char codigoOrigem(Origem o);
Origem origemDoCodigo(char codigo);
const char* textoOrigem(Origem o);

// Decide se uma porta pode abrir, pelo que se sabe da outra.
//   outraEmLiberacao: a portaria acabou de mandar a outra abrir e ela ainda não confirmou.
//   liberarSemComunicacao: regra para quando não há notícia da outra porta.
Motivo decidirAbertura(const VisaoPorta& outra, bool outraEmLiberacao, bool bypass, bool liberarSemComunicacao);

// As duas portas liberadas ao mesmo tempo sem bypass (dois pedidos quase simultâneos decididos
// sem a portaria, por exemplo): a porta que ainda está fechada volta a travar. Se as duas ainda
// estão fechadas, a porta B cede. Se esta porta já foi aberta, não adianta travar: fica como está.
bool deveCancelarLiberacao(bool souPortaA, bool minhaPortaAberta, bool liberacaoComBypass, const VisaoPorta& outra);
