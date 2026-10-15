// Mensagens entre as placas, por UDP (porta PORTA_UDP) na rede do roteador.
//
// Formato (texto, para dar para ler num analisador de rede):
//   ITL1|grupo|origem|sessão|seq|eco P,eco A,eco B|tipo|dados|assinatura
//     grupo       1 a 99; mensagens de outro grupo (outro conjunto de portas na mesma rede) são ignoradas
//     origem      P (portaria), A ou B
//     sessão      número sorteado quando a placa liga (8 dígitos hexa)
//     seq         contador de mensagens da sessão
//     eco         última sessão vista de cada placa (ver sessao.h)
//     tipo        E, Q, L ou R (ver protocolo.h)
//     dados       "chave=valor;chave=valor"
//     assinatura  HMAC-SHA256 de tudo antes dela (até a última '|'), com a CHAVE_REDE (32 hexa)
//
// Sem a CHAVE_REDE ninguém consegue criar uma mensagem válida, e sessão + seq impedem que uma
// mensagem gravada da rede seja reenviada depois.
//
// O estado vai por broadcast (255.255.255.255) a cada segundo, que também serve para as placas
// se acharem na rede. Pedidos, comandos e respostas vão direto para o IP da placa e são
// repetidos até a resposta chegar (ver porta.cpp e porteiro.cpp).

#include "protocolo.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbedtls/md.h>
#include "config.h"
#include "configuracao.h"
#include "rede_wifi.h"
#include "registro.h"
#include "sessao.h"

namespace {

constexpr char PREFIXO[] = "ITL1|";
constexpr size_t TAM_MAX = 1400;  // cabe num pacote Wi-Fi sem fragmentar; a maior é a lista de contas
constexpr size_t BYTES_ASSINATURA = 16;  // primeiros 128 bits do HMAC-SHA256
constexpr size_t TAM_ASSINATURA = BYTES_ASSINATURA * 2;
constexpr unsigned long AVISO_REPETIDO_MS = 60000;
const IPAddress BROADCAST(255, 255, 255, 255);

struct Par {
  InfoPar info;
  SessaoPar sessao;
  uint32_t ultimoSeqEstado = 0;  // último ESTADO aplicado (um estado mais velho que chegue atrasado é ignorado)
  bool online = false;
};

WiFiUDP udp;
bool udpAberto = false;
TratadorMensagem tratador = nullptr;
uint32_t minhaSessao = 0;
uint32_t meuSeq = 0;
Par pares[NUM_PAPEIS];
uint32_t recusadas = 0;
unsigned long ultimoAvisoRecusa = 0;
bool avisouRecusa = false;
char conflito[16] = "";
unsigned long ultimoAvisoConflito = 0;
// Fora da pilha, porque são grandes. O programa roda numa linha só e um envio nunca acontece no
// meio de outro, então um de cada basta (um pacote recebido pode responder com um envio).
char entrada[TAM_MAX + 1];
char saida[TAM_MAX + 1];

void assinar(const char* texto, size_t tamanho, char* saidaHex) {
  static const char HEXA[] = "0123456789abcdef";
  uint8_t mac[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), reinterpret_cast<const uint8_t*>(CHAVE_REDE),
                  sizeof(CHAVE_REDE) - 1, reinterpret_cast<const uint8_t*>(texto), tamanho, mac);
  for (size_t i = 0; i < BYTES_ASSINATURA; i++) {
    saidaHex[2 * i] = HEXA[mac[i] >> 4];
    saidaHex[2 * i + 1] = HEXA[mac[i] & 0x0F];
  }
  saidaHex[TAM_ASSINATURA] = 0;
}

// Compara sem parar na primeira diferença (não revela pelo tempo quantos caracteres batem).
bool iguais(const char* a, const char* b, size_t n) {
  uint8_t diferenca = 0;
  for (size_t i = 0; i < n; i++) diferenca |= a[i] ^ b[i];
  return diferenca == 0;
}

size_t montar(char* buf, size_t capacidade, char tipo, const char* dados) {
  meuSeq++;
  int n = snprintf(buf, capacidade, "ITL1|%u|%c|%08x|%u|%08x,%08x,%08x|%c|%s|", static_cast<unsigned>(cfgRede.grupo),
                   codigoPapel(MEU_PAPEL), static_cast<unsigned>(minhaSessao), static_cast<unsigned>(meuSeq),
                   static_cast<unsigned>(pares[0].sessao.sessaoVista), static_cast<unsigned>(pares[1].sessao.sessaoVista),
                   static_cast<unsigned>(pares[2].sessao.sessaoVista), tipo, dados);
  if (n <= 0 || static_cast<size_t>(n) + TAM_ASSINATURA + 1 > capacidade) return 0;
  assinar(buf, n, buf + n);
  return n + TAM_ASSINATURA;
}

void transmitir(const IPAddress& destino, const char* buf, size_t n) {
  if (!udpAberto || !wifiConectado()) return;
  udp.beginPacket(destino, PORTA_UDP);
  udp.write(reinterpret_cast<const uint8_t*>(buf), n);
  udp.endPacket();
}

bool lerNumero(const char* texto, int base, uint32_t& valor) {
  if (!*texto) return false;
  char* fim;
  unsigned long v = strtoul(texto, &fim, base);
  if (*fim) return false;
  valor = v;
  return true;
}

void recusar(const IPAddress& de) {
  recusadas++;
  unsigned long agora = millis();
  if (!avisouRecusa || agora - ultimoAvisoRecusa >= AVISO_REPETIDO_MS) {
    avisouRecusa = true;
    ultimoAvisoRecusa = agora;
    registrar("Mensagem com assinatura inválida vinda de %s: a CHAVE_REDE é a mesma nas 3 placas?",
              de.toString().c_str());
  }
}

void avisarConflito(const IPAddress& de) {
  unsigned long agora = millis();
  String ip = de.toString();
  if (strcmp(conflito, ip.c_str()) != 0 || agora - ultimoAvisoConflito >= AVISO_REPETIDO_MS) {
    strlcpy(conflito, ip.c_str(), sizeof(conflito));
    ultimoAvisoConflito = agora;
    registrar("ATENÇÃO: a placa %s também foi gravada como %s", conflito, nomeExibicao(MEU_PAPEL));
  }
}

void processar(char* msg, const IPAddress& de, unsigned long agora) {
  if (strncmp(msg, PREFIXO, sizeof(PREFIXO) - 1) != 0) return;  // não é do Interlock

  // Grupo primeiro: outro conjunto de portas na mesma rede é ignorado sem contar como recusa.
  char* fim;
  unsigned long grupo = strtoul(msg + sizeof(PREFIXO) - 1, &fim, 10);
  if (*fim != '|' || grupo != cfgRede.grupo) return;

  char* barra = strrchr(msg, '|');
  if (!barra || strlen(barra + 1) != TAM_ASSINATURA) return;
  char esperada[TAM_ASSINATURA + 1];
  assinar(msg, barra + 1 - msg, esperada);
  if (!iguais(esperada, barra + 1, TAM_ASSINATURA)) {
    recusar(de);
    return;
  }
  *barra = 0;

  // Depois do grupo: origem|sessão|seq|ecos|tipo|dados (os dados nunca têm '|')
  char* campos[6];
  char* atual = fim + 1;
  for (int i = 0; i < 5; i++) {
    campos[i] = atual;
    char* b = strchr(atual, '|');
    if (!b) return;
    *b = 0;
    atual = b + 1;
  }
  campos[5] = atual;

  Papel origem;
  uint32_t sessao, seq;
  unsigned eco[NUM_PAPEIS];
  if (strlen(campos[0]) != 1 || !papelDoCodigo(campos[0][0], origem)) return;
  if (!lerNumero(campos[1], 16, sessao) || !lerNumero(campos[2], 10, seq)) return;
  if (sscanf(campos[3], "%8x,%8x,%8x", &eco[0], &eco[1], &eco[2]) != 3) return;
  if (strlen(campos[4]) != 1) return;
  char tipo = campos[4][0];
  const char* dados = campos[5];

  if (origem == MEU_PAPEL) {
    if (sessao != minhaSessao) avisarConflito(de);  // outra placa gravada com o mesmo papel
    return;
  }

  Par& par = pares[indice(origem)];
  Aceite aceite = avaliarMensagem(par.sessao, minhaSessao, sessao, seq, eco[indice(MEU_PAPEL)]);
  if (!aceitou(aceite)) return;

  if (aceite == Aceite::NOVA_SESSAO) {
    par.ultimoSeqEstado = 0;
    registrar(par.info.conhecida ? "%s reiniciou e voltou a comunicar (%s)" : "%s encontrada na rede (%s)",
              nomeExibicao(origem), de.toString().c_str());
    par.info.conhecida = true;
  } else if (!par.online) {
    registrar("%s voltou a comunicar (%s)", nomeExibicao(origem), de.toString().c_str());
  }
  par.online = true;
  par.info.ip = de;
  par.info.ultimoContato = agora;

  if (tipo == MSG_ESTADO) {
    if (seq <= par.ultimoSeqEstado) return;  // estado mais velho que o já aplicado
    par.ultimoSeqEstado = seq;
    par.info.rssi = campoNumero(dados, "rssi", 0);
    campoTexto(dados, "fw", par.info.fw, sizeof(par.info.fw));
    par.info.ligadaS = campoNumero(dados, "up", 0);
  }
  if (tratador) tratador(origem, tipo, dados, agora);
}

}  // namespace

void iniciarProtocolo(TratadorMensagem t) {
  tratador = t;
  // Sorteada com o rádio já ligado (o gerador do ESP32 é aleatório de verdade com o Wi-Fi ativo).
  do {
    minhaSessao = esp_random();
  } while (minhaSessao == 0);
  udpAberto = udp.begin(PORTA_UDP);
  if (!udpAberto) registrar("ERRO: não consegui abrir a porta UDP %u", PORTA_UDP);
}

void atualizarProtocolo(unsigned long agora) {
  if (!udpAberto) return;
  for (int i = 0; i < 8; i++) {  // no máximo 8 por volta, para não segurar o resto do programa
    int tamanho = udp.parsePacket();
    if (tamanho <= 0) break;
    int lidos = udp.read(entrada, TAM_MAX);
    IPAddress de = udp.remoteIP();
    udp.flush();
    if (lidos <= 0 || tamanho > static_cast<int>(TAM_MAX)) continue;  // grande demais: não é nosso
    entrada[lidos] = 0;
    processar(entrada, de, agora);
  }

  for (uint8_t i = 0; i < NUM_PAPEIS; i++) {
    Par& par = pares[i];
    if (par.online && agora - par.info.ultimoContato >= SEM_COMUNICACAO_MS) {
      par.online = false;
      registrar("%s sem comunicação", nomeExibicao(static_cast<Papel>(i)));
    }
  }
}

bool parOnline(Papel p) {
  const Par& par = pares[indice(p)];
  return p != MEU_PAPEL && par.sessao.sessao != 0 && millis() - par.info.ultimoContato < SEM_COMUNICACAO_MS;
}

const InfoPar& infoPar(Papel p) { return pares[indice(p)].info; }
uint32_t mensagensRecusadas() { return recusadas; }
const char* conflitoDePapel() { return conflito; }

void enviarPara(Papel destino, char tipo, const String& dados) {
  size_t n = montar(saida, sizeof(saida), tipo, dados.c_str());
  if (!n) return;
  transmitir(parOnline(destino) ? pares[indice(destino)].info.ip : BROADCAST, saida, n);
}

void enviarEstado(const String& dados, bool direto) {
  String completo = dados;
  completo += ";rssi=";
  completo += wifiConectado() ? WiFi.RSSI() : 0;
  completo += ";fw=" FW_VERSAO ";up=";
  completo += millis() / 1000;

  size_t n = montar(saida, sizeof(saida), MSG_ESTADO, completo.c_str());
  if (!n) return;
  transmitir(BROADCAST, saida, n);
  // A mesma mensagem direto: quem receber as duas descarta a segunda (mesmo seq).
  if (direto) {
    for (uint8_t i = 0; i < NUM_PAPEIS; i++) {
      Papel p = static_cast<Papel>(i);
      if (parOnline(p)) transmitir(pares[i].info.ip, saida, n);
    }
  }
}

bool AgendaEstado::hora(unsigned long agora, bool& direto) {
  unsigned long passou = agora - ultimo_;
  if (mudou_ && passou >= 50) {  // no máximo 20 por segundo, se o estado ficar mudando
    mudou_ = false;
    repeticoes_ = 2;
    direto = true;
  } else if (repeticoes_ && passou >= REPETICAO_ESTADO_MS) {
    repeticoes_--;
    direto = true;
  } else if (passou >= INTERVALO_ESTADO_MS) {
    direto = false;
  } else {
    return false;
  }
  ultimo_ = agora;
  return true;
}

bool campoTexto(const char* dados, const char* chave, char* valor, size_t tamanho) {
  size_t tamChave = strlen(chave);
  for (const char* p = dados; p && *p;) {
    if (strncmp(p, chave, tamChave) == 0 && p[tamChave] == '=') {
      const char* v = p + tamChave + 1;
      size_t n = strcspn(v, ";");
      if (n >= tamanho) n = tamanho - 1;
      memcpy(valor, v, n);
      valor[n] = 0;
      return true;
    }
    p = strchr(p, ';');
    if (p) p++;
  }
  if (tamanho) valor[0] = 0;
  return false;
}

long campoNumero(const char* dados, const char* chave, long padrao) {
  char v[16];
  if (!campoTexto(dados, chave, v, sizeof(v)) || !v[0]) return padrao;
  char* fim;
  long n = strtol(v, &fim, 10);
  return *fim ? padrao : n;
}

bool campoLogico(const char* dados, const char* chave) { return campoNumero(dados, chave, 0) == 1; }

Origem campoOrigem(const char* dados) {
  char codigo[4];
  campoTexto(dados, "ori", codigo, sizeof(codigo));
  return origemDoCodigo(codigo[0]);
}

Motivo campoMotivo(const char* dados) { return motivoDoNumero(campoNumero(dados, "mot", -1)); }

bool RespostaGuardada::repetida(Papel destino, uint32_t id) {
  if (id == 0 || id != id_) return false;
  enviarPara(destino, MSG_RESPOSTA, texto_);
  return true;
}

void RespostaGuardada::responder(Papel destino, uint32_t id, Motivo motivo) {
  char texto[48];
  snprintf(texto, sizeof(texto), "id=%u;ok=%d;mot=%d;byp=%d", static_cast<unsigned>(id), motivoLibera(motivo),
           static_cast<int>(motivo), motivo == Motivo::LIBERADO_BYPASS);
  id_ = id;
  texto_ = texto;
  enviarPara(destino, MSG_RESPOSTA, texto_);
}

uint32_t campoId(const char* dados) {
  char v[16];
  uint32_t id;
  if (!campoTexto(dados, "id", v, sizeof(v)) || !lerNumero(v, 10, id)) return 0;
  return id;
}

uint32_t novoId() {
  uint32_t id;
  do {
    id = esp_random();
  } while (id == 0);
  return id;
}
