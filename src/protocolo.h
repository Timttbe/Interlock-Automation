#pragma once
// Mensagens entre as placas, por UDP na rede do roteador. Ver o formato em protocolo.cpp.

#include <Arduino.h>
#include <IPAddress.h>
#include "config.h"
#include "intertravamento.h"
#include "papel.h"

// Tipos de mensagem
constexpr char MSG_ESTADO = 'E';    // estado da placa: a cada INTERVALO_ESTADO_MS e sempre que muda
constexpr char MSG_PEDIDO = 'Q';    // porta -> portaria: pede para abrir (facial, botão, página)
constexpr char MSG_LIBERAR = 'L';   // portaria -> porta: abra (botão ou página da portaria)
constexpr char MSG_RESPOSTA = 'R';  // resposta a um PEDIDO ou LIBERAR
constexpr char MSG_PEDIR_CONTAS = 'G';  // porta -> portaria: manda a lista de contas (ver contas.h)
constexpr char MSG_CONTAS = 'C';        // portaria -> porta: a lista de contas

// O que se sabe de outra placa (para a lista de placas da página).
struct InfoPar {
  bool conhecida = false;  // já se comunicou desde que esta placa ligou
  IPAddress ip;
  unsigned long ultimoContato = 0;
  int rssi = 0;
  char fw[16] = "";
  uint32_t ligadaS = 0;
};

typedef void (*TratadorMensagem)(Papel origem, char tipo, const char* dados, unsigned long agora);

void iniciarProtocolo(TratadorMensagem tratador);
void atualizarProtocolo(unsigned long agora);  // recebe as mensagens e chama o tratador

bool parOnline(Papel p);  // recebeu mensagem válida dessa placa há menos de SEM_COMUNICACAO_MS
const InfoPar& infoPar(Papel p);
uint32_t mensagensRecusadas();  // assinatura errada: CHAVE_REDE diferente entre as placas?
const char* conflitoDePapel();  // IP de outra placa gravada com o mesmo papel desta, ou ""

// Envia para uma placa: direto, se ela está online; senão, por broadcast.
void enviarPara(Papel destino, char tipo, const String& dados);
// Envia o estado desta placa para todas (broadcast). 'direto' = também direto para cada placa
// online, que é mais garantido no Wi-Fi: usado quando o estado acabou de mudar.
// Acrescenta sinal, versão e tempo ligado aos dados.
void enviarEstado(const String& dados, bool direto);

// Quando enviar o estado: a cada INTERVALO_ESTADO_MS e, quando ele muda, na hora e mais 2 vezes
// a cada REPETICAO_ESTADO_MS (um pacote perdido no Wi-Fi não pode esconder uma porta que abriu).
class AgendaEstado {
 public:
  void mudou() { mudou_ = true; }
  // true = é hora de enviar; 'direto' = enviar também direto para cada placa.
  bool hora(unsigned long agora, bool& direto);

 private:
  bool mudou_ = true;
  uint8_t repeticoes_ = 0;
  unsigned long ultimo_ = 0;
};

// Quando chegou o último ESTADO de outra placa.
struct Noticia {
  bool recebida = false;
  unsigned long em = 0;
  void chegou(unsigned long agora) {
    recebida = true;
    em = agora;
  }
  bool recente(unsigned long agora) const { return recebida && agora - em < SEM_COMUNICACAO_MS; }
};

// Resposta a um PEDIDO ou LIBERAR ("id=..;ok=..;mot=..;byp=.."). Quem pediu repete até a
// resposta chegar; a repetição (mesmo id) recebe a mesma resposta de novo, sem a placa agir outra vez.
class RespostaGuardada {
 public:
  // Se 'id' é o do último pedido respondido, reenvia a mesma resposta e devolve true.
  bool repetida(Papel destino, uint32_t id);
  void responder(Papel destino, uint32_t id, Motivo motivo);

 private:
  uint32_t id_ = 0;
  String texto_;
};

// Lê um campo dos dados no formato "chave=valor;chave=valor".
bool campoTexto(const char* dados, const char* chave, char* valor, size_t tamanho);
long campoNumero(const char* dados, const char* chave, long padrao);
bool campoLogico(const char* dados, const char* chave);  // "1" = true; qualquer outra coisa ou ausente = false
Origem campoOrigem(const char* dados);                   // campo "ori"
Motivo campoMotivo(const char* dados);                   // campo "mot"
uint32_t campoId(const char* dados);                     // campo "id" (sem sinal); 0 se não tiver
uint32_t novoId();                                       // id aleatório para PEDIDO/LIBERAR (nunca 0)
