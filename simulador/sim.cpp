// Simulador do Interlock: roda o firmware real das 3 placas (cada uma num namespace próprio, com
// a própria memória: ver dispositivo.inc) numa rede simulada, com relógio, pinos e perda de
// pacotes controlados. Cada cenário roda num processo novo (placas recém-ligadas):
//   simulador <número do cenário> [-v]     (-v mostra o registro de eventos das 3 placas)
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <random>
#include <string>
#include <vector>
#include "stubs/pre.h"
#include "config.h"  // níveis dos relés e buzzers

enum { PORTARIA = 0, PA = 1, PB = 2 };
const char* NOMES[] = {"PORTARIA", "PORTA_A ", "PORTA_B "};

// Pinos (iguais ao config.h)
const int SENSOR = 32, FACIAL = 33, BOTAO = 25, IMA = 26;
const int BOTAO_A = 32, BOTAO_B = 33, CHAVE = 25, BUZZER_A = 26, BUZZER_B = 27;

typedef void (*FnVoid)();
typedef void (*FnRegra)(int);

// Instâncias do firmware (dev_*.cpp). As "_r" são cópias novas para simular um reinício.
#define INSTANCIA_EXTERNA(n) \
  void n##_setup();          \
  void n##_loop();           \
  void n##_regra(int);
INSTANCIA_EXTERNA(porteiro)
INSTANCIA_EXTERNA(porteiro_r)
INSTANCIA_EXTERNA(porta_a)
INSTANCIA_EXTERNA(porta_a_r)
INSTANCIA_EXTERNA(porta_b)
struct Instancia {
  FnVoid setup, loop;
  FnRegra regra;
  bool usada;
};
Instancia I_PORTEIRO = {porteiro_setup, porteiro_loop, porteiro_regra, false};
Instancia I_PORTEIRO_R = {porteiro_r_setup, porteiro_r_loop, porteiro_r_regra, false};
Instancia I_PORTA_A = {porta_a_setup, porta_a_loop, porta_a_regra, false};
Instancia I_PORTA_A_R = {porta_a_r_setup, porta_a_r_loop, porta_a_r_regra, false};
Instancia I_PORTA_B = {porta_b_setup, porta_b_loop, porta_b_regra, false};

SerialSim Serial;
EspSim ESP;
WiFiSim WiFi;

struct Pacote {
  unsigned long entrega;
  int destino;
  uint32_t origemIp;
  uint32_t destinoIp;
  std::string dados;
};

struct Placa {
  Instancia* inst = nullptr;
  int entrada[40];
  int saida[40];
  bool wifi = true;
  bool ligada = true;
  unsigned long bootEm = 0;
  unsigned long relogioInicial = 0;  // millis() da placa no boot (para testar o estouro do millis)
  std::deque<Pacote> caixa;
  std::vector<std::string> log;
};

Placa placas[3];
unsigned long agora = 0;
std::mt19937 rng(20261013);
double perda = 0.0;
int latMin = 1, latMax = 6;
std::vector<Pacote> emTransito;
bool verboso = false;
bool gravar = false;
std::vector<Pacote> gravados;
bool bypassEsperado = false;
unsigned long sobreposicaoAtual = 0, maiorSobreposicao = 0;
int falhas = 0, acertos = 0;

uint32_t ipDe(int d) { return 10u | (uint32_t(d + 1) << 24); }  // 10.0.0.1, .2, .3
double sorteio() { return std::uniform_real_distribution<double>(0, 1)(rng); }

unsigned long h_millis(int d) { return placas[d].relogioInicial + (agora - placas[d].bootEm); }
int h_digitalRead(int d, int p) { return placas[d].entrada[p]; }
void h_digitalWrite(int d, int p, int v) { placas[d].saida[p] = v; }
void h_udpEnviar(int d, uint32_t ip, const uint8_t* dados, size_t n) {
  if (!placas[d].wifi) return;
  std::string s(reinterpret_cast<const char*>(dados), n);
  if (gravar) gravados.push_back({0, -1, ipDe(d), ip, s});
  for (int k = 0; k < 3; k++) {
    if (k == d) continue;
    if (ip != 0xFFFFFFFFu && ip != ipDe(k)) continue;
    if (sorteio() < perda) continue;
    int lat = std::uniform_int_distribution<int>(latMin, latMax)(rng);
    emTransito.push_back({agora + lat, k, ipDe(d), ip, s});
  }
}
int h_udpReceber(int d, uint8_t* buf, size_t cap, uint32_t* de) {
  auto& c = placas[d].caixa;
  if (c.empty()) return 0;
  Pacote p = c.front();
  c.pop_front();
  size_t n = std::min(cap, p.dados.size());
  memcpy(buf, p.dados.data(), n);
  *de = p.origemIp;
  return static_cast<int>(p.dados.size());
}
uint32_t h_aleatorio() { return rng(); }
int h_wifi(int d) { return placas[d].wifi; }
void h_log(int d, const char* t) {
  placas[d].log.push_back(t);
  if (verboso) printf("        %7.3f %s %s", agora / 1000.0, NOMES[d], t);
}
void h_reiniciar(int d) { printf("        !! %s pediu reinício\n", NOMES[d]); }

HostApi API = {h_millis, h_digitalRead, h_digitalWrite, h_udpEnviar, h_udpReceber,
               h_aleatorio, h_wifi, h_log, h_reiniciar};
int atual = 0;  // placa que está rodando agora
namespace sim {
const HostApi* host() { return &API; }
int dispositivo() { return atual; }
}  // namespace sim

// Liga (ou "reinicia") a placa d com uma instância nova do firmware.
void carregar(int d, Instancia* inst, unsigned long relogioInicial = 0) {
  if (inst->usada) {
    printf("instância já usada: cada cenário só pode reiniciar uma vez cada placa\n");
    exit(2);
  }
  inst->usada = true;
  Placa& p = placas[d];
  p.inst = inst;
  p.caixa.clear();
  p.log.clear();
  p.bootEm = agora;
  p.relogioInicial = relogioInicial;
  p.ligada = true;
  atual = d;
  inst->setup();
}

bool liberada(int d) { return placas[d].saida[IMA] == RELE_ATIVO_EM; }
bool buzzer(int porta) { return placas[PORTARIA].saida[porta == PA ? BUZZER_A : BUZZER_B] == BUZZER_ATIVO_EM; }

void passo() {
  agora++;
  for (size_t i = 0; i < emTransito.size();) {
    if (emTransito[i].entrega <= agora) {
      Placa& dst = placas[emTransito[i].destino];
      if (dst.wifi && dst.ligada) dst.caixa.push_back(emTransito[i]);
      emTransito.erase(emTransito.begin() + i);
    } else {
      i++;
    }
  }
  for (int d = 0; d < 3; d++) {
    if (!placas[d].ligada) continue;
    atual = d;
    placas[d].inst->loop();
  }
  // Monitor: as duas portas destravadas ao mesmo tempo sem bypass
  if (liberada(PA) && liberada(PB) && !bypassEsperado) {
    sobreposicaoAtual++;
    maiorSobreposicao = std::max(maiorSobreposicao, sobreposicaoAtual);
  } else {
    sobreposicaoAtual = 0;
  }
}
void rodar(unsigned long ms) {
  for (unsigned long i = 0; i < ms; i++) passo();
}

void reiniciarTudo(unsigned long relogioInicial = 0) {
  emTransito.clear();
  perda = 0;
  bypassEsperado = false;
  maiorSobreposicao = sobreposicaoAtual = 0;
  for (int d = 0; d < 3; d++) {
    Placa& p = placas[d];
    for (int i = 0; i < 40; i++) {
      p.entrada[i] = 1;  // pull-up: nada acionado
      p.saida[i] = -1;
    }
    p.wifi = true;
  }
  placas[PA].entrada[SENSOR] = placas[PB].entrada[SENSOR] = 0;  // portas fechadas
  carregar(PORTARIA, &I_PORTEIRO, relogioInicial);
  carregar(PA, &I_PORTA_A, relogioInicial);
  carregar(PB, &I_PORTA_B, relogioInicial);
  rodar(8000);  // tempo de as placas se acharem
}

void regra(int d, bool liberar) {
  atual = d;
  placas[d].inst->regra(liberar);
}

void acionar(int d, int pino, unsigned long ms = 150) {
  placas[d].entrada[pino] = 0;
  rodar(ms);
  placas[d].entrada[pino] = 1;
}
void porta(int d, bool aberta) { placas[d].entrada[SENSOR] = aberta ? 1 : 0; }

size_t marca(int d) { return placas[d].log.size(); }
bool logTem(int d, const char* trecho, size_t desde = 0) {
  for (size_t i = desde; i < placas[d].log.size(); i++)
    if (placas[d].log[i].find(trecho) != std::string::npos) return true;
  return false;
}

void confere(bool ok, const char* descricao) {
  if (ok) {
    acertos++;
    printf("  ok     %s\n", descricao);
  } else {
    falhas++;
    printf("  FALHOU %s\n", descricao);
  }
}

void titulo(const char* t) { printf("\n== %s\n", t); }

// ---------------------------------------------------------------------------------------------

void cenarioInicio() {
  titulo("Inicialização: as placas se acham na rede");
  reiniciarTudo();
  confere(logTem(PORTARIA, "Porta A encontrada") && logTem(PORTARIA, "Porta B encontrada"), "portaria achou as duas portas");
  confere(logTem(PA, "Portaria encontrada") && logTem(PA, "Porta B encontrada"), "porta A achou a portaria e a porta B");
  confere(logTem(PA, "Funcionando com a portaria"), "porta A em modo 'com a portaria'");
  confere(!liberada(PA) && !liberada(PB), "nenhuma porta liberada ao ligar");
  const int releDesligado = RELE_ATIVO_EM == HIGH ? LOW : HIGH;
  confere(placas[PA].saida[IMA] == releDesligado && placas[PB].saida[IMA] == releDesligado,
          RELE_ATIVO_EM == HIGH ? "pinos dos relés dos ímãs em LOW desde que a placa ligou"
                                : "pinos dos relés dos ímãs em HIGH desde que a placa ligou");
  confere(!buzzer(PA) && !buzzer(PB), "buzzers desligados");
  const int desligado = BUZZER_ATIVO_EM == HIGH ? LOW : HIGH;
  confere(placas[PORTARIA].saida[BUZZER_A] == desligado && placas[PORTARIA].saida[BUZZER_B] == desligado,
          BUZZER_ATIVO_EM == HIGH ? "pinos dos buzzers em LOW desde que a placa ligou"
                                  : "pinos dos buzzers em HIGH desde que a placa ligou");
}

void cenarioIntertravamento() {
  titulo("Facial na A libera; B fica bloqueada enquanto A está liberada ou aberta");
  reiniciarTudo();
  acionar(PA, FACIAL);
  confere(liberada(PA), "facial A: porta A liberada");
  confere(logTem(PORTARIA, "Porta A liberada (facial)"), "portaria registrou a liberação da A");
  size_t m = marca(PB);
  acionar(PB, FACIAL);
  confere(!liberada(PB), "facial B com A liberada: B continua travada");
  confere(logTem(PB, "destravada", m), "B registrou o motivo (outra porta destravada)");

  porta(PA, true);  // alguém abre a A
  rodar(6000);      // tempo de abertura acaba com a porta ainda aberta
  confere(!liberada(PA), "tempo acabou: eletroímã da A volta a travar");
  m = marca(PB);
  acionar(PB, BOTAO);
  confere(!liberada(PB), "A ainda aberta depois do tempo: B continua bloqueada");
  confere(logTem(PB, "aberta", m), "motivo: a outra porta está aberta");

  porta(PA, false);
  rodar(300);
  acionar(PB, BOTAO);
  confere(liberada(PB), "A fechou: botão da B libera a B");
}

void cenarioTravarAoFechar() {
  titulo("Trava assim que a porta abre e fecha (sem esperar o tempo de abertura)");
  reiniciarTudo();
  acionar(PA, FACIAL);
  porta(PA, true);
  rodar(1000);
  porta(PA, false);
  rodar(200);
  confere(!liberada(PA), "A travou ao fechar, antes dos 5 s");
  acionar(PB, FACIAL);
  confere(liberada(PB), "B já pode abrir");
}

void cenarioAlarme() {
  titulo("Porta aberta por mais de 60 s: buzzer na portaria até fechar");
  reiniciarTudo();
  acionar(PA, FACIAL);
  porta(PA, true);
  rodar(59000);
  confere(!buzzer(PA), "59 s: buzzer ainda desligado");
  rodar(2000);
  confere(buzzer(PA), "61 s: buzzer da porta A tocando");
  confere(!buzzer(PB), "buzzer da porta B desligado");
  porta(PA, false);
  rodar(300);
  confere(!buzzer(PA), "porta fechou: buzzer desligou");
}

void cenarioBotoesPortaria() {
  titulo("Botões da portaria seguem o intertravamento");
  reiniciarTudo();
  acionar(PORTARIA, BOTAO_A);
  confere(liberada(PA), "botão A da portaria libera a A");
  acionar(PORTARIA, BOTAO_B);
  confere(!liberada(PB), "botão B com A liberada: B continua travada");
  rodar(5500);
  acionar(PORTARIA, BOTAO_B);
  confere(liberada(PB), "A travou: botão B libera a B");
}

void cenarioBypass() {
  titulo("Chave de bypass: as duas portas abrem juntas");
  reiniciarTudo();
  placas[PORTARIA].entrada[CHAVE] = 0;
  bypassEsperado = true;
  rodar(300);
  acionar(PA, FACIAL);
  porta(PA, true);
  acionar(PB, FACIAL);
  confere(liberada(PA) && liberada(PB), "com bypass, A e B liberadas ao mesmo tempo");
  porta(PA, false);
  rodar(6000);
  placas[PORTARIA].entrada[CHAVE] = 1;
  bypassEsperado = false;
  rodar(300);
  porta(PA, true);
  rodar(100);
  acionar(PB, FACIAL);
  confere(!liberada(PB), "bypass desligado e A aberta: B bloqueada de novo");
}

void cenarioSimultaneos(int repeticoes, double perdaPacotes) {
  char t[160];
  snprintf(t, sizeof(t), "Facial na A e na B quase ao mesmo tempo (%d vezes, perda de %.0f%% dos pacotes)", repeticoes,
           perdaPacotes * 100);
  titulo(t);
  reiniciarTudo();
  perda = perdaPacotes;
  int duas = 0, nenhuma = 0, uma = 0;
  for (int i = 0; i < repeticoes; i++) {
    int atraso = std::uniform_int_distribution<int>(0, 15)(rng);
    int primeiro = sorteio() < 0.5 ? PA : PB, segundo = primeiro == PA ? PB : PA;
    placas[primeiro].entrada[FACIAL] = 0;
    rodar(atraso);
    placas[segundo].entrada[FACIAL] = 0;
    rodar(150);
    placas[PA].entrada[FACIAL] = placas[PB].entrada[FACIAL] = 1;
    rodar(700);  // dá tempo de um pedido sem resposta cair na decisão local
    bool a = liberada(PA), b = liberada(PB);
    if (a && b) duas++;
    else if (a || b) uma++;
    else nenhuma++;
    rodar(6000);  // tempo acabar
  }
  printf("         resultado: %d com uma porta liberada, %d com nenhuma, %d com as duas\n", uma, nenhuma, duas);
  confere(duas == 0, "nunca as duas liberadas");
  confere(maiorSobreposicao == 0, "nem por 1 ms as duas ficaram destravadas juntas");
  confere(perdaPacotes > 0 || nenhuma == 0, "sem perda de pacotes, sempre uma das duas abre");
}

void cenarioPortariaFora() {
  titulo("Portaria desligada: as portas se respeitam sozinhas");
  reiniciarTudo();
  placas[PORTARIA].ligada = false;
  rodar(6000);
  confere(logTem(PA, "sem a portaria, decidindo com a outra porta"), "porta A passou a decidir com a B");
  size_t m = marca(PA);
  acionar(PA, FACIAL);
  confere(liberada(PA), "facial A libera mesmo sem a portaria");
  confere(logTem(PA, "Porta liberada (facial)", m), "A registrou a liberação");
  acionar(PB, FACIAL);
  confere(!liberada(PB), "B não abre com a A liberada (sem a portaria)");
  rodar(6000);
  acionar(PB, FACIAL);
  confere(liberada(PB), "A travou: B abre");
}

void cenarioPortariaAcabouDeCair() {
  titulo("Portaria caiu agora há pouco: nega até ela ser dada como fora; depois as portas decidem");
  reiniciarTudo();
  placas[PORTARIA].ligada = false;
  rodar(500);
  size_t m = marca(PA);
  placas[PA].entrada[FACIAL] = 0;
  rodar(1200);
  placas[PA].entrada[FACIAL] = 1;
  confere(!liberada(PA), "portaria ainda parece no ar e não respondeu: A nega");
  confere(logTem(PA, "a portaria não respondeu", m), "A registrou o motivo");
  rodar(4000);
  acionar(PA, FACIAL);
  confere(liberada(PA), "5 s sem notícia da portaria: A decide com a B e libera");
}

void cenarioSimultaneosSemPortaria() {
  titulo("Sem a portaria: facial na A e na B no mesmo instante (40 vezes)");
  reiniciarTudo();
  placas[PORTARIA].ligada = false;
  rodar(6000);
  int duasFinal = 0, conflitos = 0;
  unsigned long pior = 0;
  for (int i = 0; i < 40; i++) {
    maiorSobreposicao = 0;
    size_t m = marca(PB), mA = marca(PA);
    int atraso = std::uniform_int_distribution<int>(0, 4)(rng);
    placas[PA].entrada[FACIAL] = 0;
    rodar(atraso);
    placas[PB].entrada[FACIAL] = 0;
    rodar(150);
    placas[PA].entrada[FACIAL] = placas[PB].entrada[FACIAL] = 1;
    rodar(300);
    if (liberada(PA) && liberada(PB)) duasFinal++;
    if (logTem(PB, "conflito", m) || logTem(PA, "conflito", mA)) conflitos++;
    pior = std::max(pior, maiorSobreposicao);
    rodar(6000);
  }
  printf("         resolvidos pela regra de conflito: %d; maior tempo com as duas destravadas: %lu ms\n", conflitos, pior);
  confere(duasFinal == 0, "depois de 300 ms nunca ficam as duas liberadas");
  confere(pior <= 20, "se as duas soltarem juntas, uma trava de novo em até 20 ms");
}

void cenarioSemPortariaComPerda() {
  titulo("Sem a portaria e com 25% de perda: facial na A e na B quase juntos (60 vezes)");
  reiniciarTudo();
  placas[PORTARIA].ligada = false;
  rodar(6000);
  perda = 0.25;
  int duasFinal = 0;
  unsigned long pior = 0;
  for (int i = 0; i < 60; i++) {
    maiorSobreposicao = 0;
    int atraso = std::uniform_int_distribution<int>(0, 10)(rng);
    placas[PA].entrada[FACIAL] = 0;
    rodar(atraso);
    placas[PB].entrada[FACIAL] = 0;
    rodar(150);
    placas[PA].entrada[FACIAL] = placas[PB].entrada[FACIAL] = 1;
    rodar(1500);
    if (liberada(PA) && liberada(PB)) duasFinal++;
    pior = std::max(pior, maiorSobreposicao);
    rodar(6000);
  }
  printf("         maior tempo com as duas destravadas: %lu ms\n", pior);
  confere(duasFinal == 0, "depois de 1,5 s nunca ficam as duas liberadas");
  confere(pior <= 400, "uma delas trava de novo em até 400 ms mesmo perdendo pacotes");
}

void cenarioBotaoPortariaContraFacial() {
  titulo("Botão A da portaria e facial da B quase juntos, 10% de perda (60 vezes)");
  reiniciarTudo();
  perda = 0.10;
  int duas = 0;
  for (int i = 0; i < 60; i++) {
    int atraso = std::uniform_int_distribution<int>(0, 20)(rng);
    bool portariaPrimeiro = sorteio() < 0.5;
    placas[portariaPrimeiro ? PORTARIA : PB].entrada[portariaPrimeiro ? BOTAO_A : FACIAL] = 0;
    rodar(atraso);
    placas[portariaPrimeiro ? PB : PORTARIA].entrada[portariaPrimeiro ? FACIAL : BOTAO_A] = 0;
    rodar(150);
    placas[PORTARIA].entrada[BOTAO_A] = placas[PB].entrada[FACIAL] = 1;
    rodar(1200);
    if (liberada(PA) && liberada(PB)) duas++;
    rodar(6000);
  }
  confere(duas == 0, "nunca as duas liberadas");
  confere(maiorSobreposicao == 0, "nem por 1 ms");
}

void cenarioIsoladas(bool liberarSemComunicacao) {
  titulo(liberarSemComunicacao ? "Wi-Fi caiu em todas, regra 'liberar'" : "Wi-Fi caiu em todas, regra 'bloquear'");
  reiniciarTudo();
  for (int d = 0; d < 3; d++) regra(d, liberarSemComunicacao);
  for (int d = 0; d < 3; d++) placas[d].wifi = false;
  bypassEsperado = liberarSemComunicacao;  // com 'liberar', as duas podem abrir: é o combinado
  rodar(6000);
  confere(logTem(PA, "isolada"), "porta A em modo isolada");
  acionar(PA, FACIAL);
  confere(liberada(PA) == liberarSemComunicacao, liberarSemComunicacao ? "A libera (ninguém fica preso)" : "A bloqueia");
  acionar(PORTARIA, BOTAO_B);
  confere(!liberada(PB), "botão da portaria não alcança a B (sem rede)");
  for (int d = 0; d < 3; d++) placas[d].wifi = true;
  bypassEsperado = false;
  rodar(8000);
  confere(logTem(PA, "Funcionando com a portaria"), "Wi-Fi voltou: A volta a funcionar com a portaria");
  acionar(PORTARIA, BOTAO_B);
  confere(liberada(PB), "botão B da portaria volta a funcionar");
}

void cenarioReinicioPortaria() {
  titulo("Portaria reinicia: as portas seguem e depois voltam para ela");
  reiniciarTudo();
  carregar(PORTARIA, &I_PORTEIRO_R);
  rodar(1000);
  acionar(PA, FACIAL);
  confere(liberada(PA), "durante o reinício, A abre (decide com a B)");
  rodar(8000);
  confere(logTem(PA, "Portaria reiniciou"), "A percebeu o reinício da portaria");
  acionar(PB, FACIAL);
  confere(liberada(PB), "depois do reinício, B abre normalmente");
  confere(logTem(PORTARIA, "Porta B liberada (facial)"), "pela portaria");
}

void cenarioRepeticao() {
  titulo("Ataque: mensagem 'abrir' gravada da rede e reenviada depois");
  reiniciarTudo();
  gravados.clear();
  gravar = true;
  acionar(PORTARIA, BOTAO_A);
  gravar = false;
  confere(liberada(PA), "botão A da portaria abriu a A (mensagens gravadas)");
  rodar(6000);
  confere(!liberada(PA), "A travou");
  int liberar = 0;
  for (auto& p : gravados) {
    if (p.dados.find("|L|") == std::string::npos) continue;
    liberar++;
    emTransito.push_back({agora + 1, PA, p.origemIp, p.destinoIp, p.dados});
  }
  rodar(200);
  printf("         %d mensagens LIBERAR reenviadas\n", liberar);
  confere(liberar > 0 && !liberada(PA), "reenvio da mensagem gravada não abre a porta");

  // Mensagem alterada: troca o grupo/conteúdo e mantém a assinatura antiga
  size_t m = marca(PA);
  for (auto& p : gravados) {
    if (p.dados.find("|L|") == std::string::npos) continue;
    std::string alterada = p.dados;
    size_t pos = alterada.find(";ori=");
    if (pos != std::string::npos) alterada[pos + 5] = 'F';
    emTransito.push_back({agora + 1, PA, p.origemIp, p.destinoIp, alterada});
    break;
  }
  rodar(200);
  confere(!liberada(PA) && logTem(PA, "assinatura inválida", m), "mensagem alterada é recusada (assinatura)");

  // Depois que a porta reinicia, a mensagem antiga também não vale
  carregar(PA, &I_PORTA_A_R);
  rodar(5000);
  for (auto& p : gravados)
    if (p.dados.find("|L|") != std::string::npos) emTransito.push_back({agora + 1, PA, p.origemIp, p.destinoIp, p.dados});
  rodar(200);
  confere(!liberada(PA), "nem depois de a porta reiniciar");
}

void cenarioPortaForcada() {
  titulo("Porta forçada e sensor com fio rompido");
  reiniciarTudo();
  size_t m = marca(PA);
  porta(PA, true);
  rodar(300);
  confere(logTem(PA, "abriu sem estar liberada", m), "A registrou abertura sem liberação");
  acionar(PB, FACIAL);
  confere(!liberada(PB), "B bloqueada enquanto A está aberta");
  rodar(61000);
  confere(buzzer(PA), "fio rompido (A sempre 'aberta'): alarme na portaria");
}

void cenarioPerdaPesada() {
  titulo("Wi-Fi ruim: 30% dos pacotes perdidos, uso normal");
  reiniciarTudo();
  perda = 0.30;
  int ok = 0;
  for (int i = 0; i < 20; i++) {
    int d = i % 2 ? PA : PB;
    acionar(d, FACIAL, 900);
    if (liberada(d)) ok++;
    rodar(6000);
  }
  printf("         %d de 20 aberturas liberadas\n", ok);
  confere(ok >= 18, "quase todas as aberturas funcionam mesmo perdendo 30% dos pacotes");
  confere(maiorSobreposicao == 0, "e nunca as duas ao mesmo tempo");
}

void cenarioEstouroMillis() {
  titulo("millis() estourando (placas ligadas há 49,7 dias)");
  reiniciarTudo(0xFFFFFFFFul - 12000);  // o contador estoura uns 4 s depois de as placas se acharem
  acionar(PA, FACIAL);
  confere(liberada(PA), "facial A libera durante o estouro");
  rodar(6000);
  confere(!liberada(PA), "A travou depois do tempo (com o contador já estourado)");
  acionar(PB, FACIAL);
  confere(liberada(PB), "B libera normalmente");
  acionar(PA, FACIAL);
  confere(!liberada(PA), "intertravamento continua valendo");
}

int main(int argc, char** argv) {
  // simulador <n> [-v]: roda o cenário n (placas recém-ligadas); sem n, mostra quantos são.
  void (*cenarios[])() = {
      cenarioInicio, cenarioIntertravamento, cenarioTravarAoFechar, cenarioAlarme, cenarioBotoesPortaria,
      cenarioBypass, [] { cenarioSimultaneos(60, 0.0); }, [] { cenarioSimultaneos(60, 0.25); },
      cenarioPortariaFora, cenarioPortariaAcabouDeCair, cenarioSimultaneosSemPortaria,
      [] { cenarioIsoladas(true); }, [] { cenarioIsoladas(false); }, cenarioReinicioPortaria, cenarioRepeticao,
      cenarioPortaForcada, cenarioPerdaPesada, cenarioEstouroMillis, cenarioSemPortariaComPerda,
      cenarioBotaoPortariaContraFacial};
  int total = sizeof(cenarios) / sizeof(cenarios[0]);
  if (argc < 2) {
    printf("%d\n", total);
    return 0;
  }
  verboso = argc > 2 && strcmp(argv[2], "-v") == 0;
  int n = atoi(argv[1]);
  if (n < 0 || n >= total) return 3;
  rng.seed(20261013 + n);
  cenarios[n]();
  printf("   -> %d ok, %d falharam\n", acertos, falhas);
  return falhas ? 1 : 0;
}
