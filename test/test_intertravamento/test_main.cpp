// Testes das regras do intertravamento (intertravamento.cpp) e da proteção contra mensagens
// repetidas (sessao.cpp).
//   Numa placa ESP32 ligada no USB:  pio test -e testes
//   No PC (precisa de gcc):          pio test -e testes_pc

#include <unity.h>
#include "intertravamento.h"
#include "sessao.h"

namespace {

VisaoPorta porta(bool comunicando, bool liberada, bool aberta) {
  VisaoPorta v;
  v.comunicando = comunicando;
  v.liberada = liberada;
  v.aberta = aberta;
  return v;
}

const VisaoPorta FECHADA = porta(true, false, false);
const VisaoPorta LIBERADA = porta(true, true, false);
const VisaoPorta ABERTA = porta(true, false, true);              // tempo de abertura já acabou, porta ainda aberta
const VisaoPorta LIBERADA_E_ABERTA = porta(true, true, true);
const VisaoPorta SEM_COMUNICACAO = porta(false, false, false);
const VisaoPorta SEM_COMUNICACAO_ABERTA = porta(false, true, true);  // último estado conhecido: aberta

void confere(Motivo esperado, Motivo obtido) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(esperado), static_cast<int>(obtido));
}

}  // namespace

void setUp() {}
void tearDown() {}

// ===================== decidirAbertura =====================

void test_abre_com_a_outra_fechada() { confere(Motivo::LIBERADO, decidirAbertura(FECHADA, false, false, true)); }

void test_nao_abre_com_a_outra_liberada() {
  confere(Motivo::OUTRA_LIBERADA, decidirAbertura(LIBERADA, false, false, true));
  confere(Motivo::OUTRA_LIBERADA, decidirAbertura(LIBERADA_E_ABERTA, false, false, true));
}

void test_nao_abre_com_a_outra_aberta_mesmo_depois_do_tempo() {
  confere(Motivo::OUTRA_ABERTA, decidirAbertura(ABERTA, false, false, true));
}

void test_nao_abre_com_a_outra_em_liberacao() {
  confere(Motivo::OUTRA_EM_LIBERACAO, decidirAbertura(FECHADA, true, false, true));
}

void test_bypass_libera_sempre() {
  confere(Motivo::LIBERADO_BYPASS, decidirAbertura(LIBERADA_E_ABERTA, true, true, false));
  confere(Motivo::LIBERADO_BYPASS, decidirAbertura(SEM_COMUNICACAO, false, true, false));
}

void test_sem_comunicacao_segue_a_regra() {
  confere(Motivo::LIBERADO_SEM_COMUNICACAO, decidirAbertura(SEM_COMUNICACAO, false, false, true));
  confere(Motivo::OUTRA_SEM_COMUNICACAO, decidirAbertura(SEM_COMUNICACAO, false, false, false));
  // O último estado conhecido não vale mais quando a outra porta parou de comunicar.
  confere(Motivo::LIBERADO_SEM_COMUNICACAO, decidirAbertura(SEM_COMUNICACAO_ABERTA, false, false, true));
}

void test_em_liberacao_vale_mesmo_sem_comunicacao() {
  confere(Motivo::OUTRA_EM_LIBERACAO, decidirAbertura(SEM_COMUNICACAO, true, false, true));
}

void test_quem_libera() {
  TEST_ASSERT_TRUE(motivoLibera(Motivo::LIBERADO));
  TEST_ASSERT_TRUE(motivoLibera(Motivo::LIBERADO_BYPASS));
  TEST_ASSERT_TRUE(motivoLibera(Motivo::LIBERADO_SEM_COMUNICACAO));
  for (int m = static_cast<int>(Motivo::OUTRA_LIBERADA); m <= MOTIVO_MAXIMO; m++) {
    TEST_ASSERT_FALSE(motivoLibera(static_cast<Motivo>(m)));
  }
}

void test_motivo_vindo_da_rede() {
  confere(Motivo::OUTRA_ABERTA, motivoDoNumero(4));
  confere(Motivo::PORTARIA_SEM_RESPOSTA, motivoDoNumero(11));
  confere(Motivo::DESCONHECIDO, motivoDoNumero(-1));
  confere(Motivo::DESCONHECIDO, motivoDoNumero(MOTIVO_MAXIMO + 1));
}

void test_codigos_de_origem_vao_e_voltam() {
  for (int o = 0; o < static_cast<int>(Origem::DESCONHECIDA); o++) {
    Origem origem = static_cast<Origem>(o);
    TEST_ASSERT_EQUAL_INT(o, static_cast<int>(origemDoCodigo(codigoOrigem(origem))));
  }
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Origem::DESCONHECIDA), static_cast<int>(origemDoCodigo('x')));
}

// ===================== deveCancelarLiberacao =====================

void test_duas_liberadas_e_fechadas_b_cede() {
  TEST_ASSERT_FALSE(deveCancelarLiberacao(true, false, false, LIBERADA));  // A mantém
  TEST_ASSERT_TRUE(deveCancelarLiberacao(false, false, false, LIBERADA));  // B trava
}

void test_a_outra_ja_abriu_a_fechada_cede() {
  TEST_ASSERT_TRUE(deveCancelarLiberacao(true, false, false, LIBERADA_E_ABERTA));
  TEST_ASSERT_TRUE(deveCancelarLiberacao(true, false, false, ABERTA));
  TEST_ASSERT_TRUE(deveCancelarLiberacao(false, false, false, ABERTA));
}

void test_porta_ja_aberta_nao_cancela() {
  TEST_ASSERT_FALSE(deveCancelarLiberacao(false, true, false, LIBERADA_E_ABERTA));
}

void test_bypass_nao_cancela() { TEST_ASSERT_FALSE(deveCancelarLiberacao(false, false, true, LIBERADA_E_ABERTA)); }

void test_sem_comunicacao_nao_cancela() {
  TEST_ASSERT_FALSE(deveCancelarLiberacao(false, false, false, SEM_COMUNICACAO_ABERTA));
}

void test_outra_fechada_nao_cancela() {
  TEST_ASSERT_FALSE(deveCancelarLiberacao(true, false, false, FECHADA));
  TEST_ASSERT_FALSE(deveCancelarLiberacao(false, false, false, FECHADA));
}

// ===================== avaliarMensagem (sessões) =====================

const uint32_t EU = 0x11111111;
const uint32_t ELA = 0x22222222;
const uint32_t ELA_DEPOIS_DE_REINICIAR = 0x33333333;

void test_primeiro_contato_precisa_do_eco() {
  SessaoPar par;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Aceite::SEM_ECO), static_cast<int>(avaliarMensagem(par, EU, ELA, 1, 0)));
  TEST_ASSERT_EQUAL_HEX32(ELA, par.sessaoVista);  // o próximo eco leva a sessão dela
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Aceite::NOVA_SESSAO), static_cast<int>(avaliarMensagem(par, EU, ELA, 2, EU)));
  TEST_ASSERT_EQUAL_HEX32(ELA, par.sessao);
}

void test_mesma_mensagem_duas_vezes_e_recusada() {
  SessaoPar par;
  avaliarMensagem(par, EU, ELA, 10, EU);
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 11, 0)));  // na sessão aceita, o eco não importa
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Aceite::REPETIDA), static_cast<int>(avaliarMensagem(par, EU, ELA, 11, 0)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Aceite::REPETIDA), static_cast<int>(avaliarMensagem(par, EU, ELA, 10, EU)));
}

void test_fora_de_ordem_dentro_da_janela_aceita_uma_vez() {
  SessaoPar par;
  avaliarMensagem(par, EU, ELA, 100, EU);
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 105, 0)));
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 103, 0)));  // chegou atrasada
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, ELA, 103, 0)));
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, ELA, 105, 0)));
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 104, 0)));
}

void test_velha_demais_e_recusada() {
  SessaoPar par;
  avaliarMensagem(par, EU, ELA, 1, EU);
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 100, 0)));
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, ELA, 50, 0)));  // 50 atrás: fora da janela de 32
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 80, 0)));   // 20 atrás: dentro
}

void test_salto_grande_limpa_a_janela() {
  SessaoPar par;
  avaliarMensagem(par, EU, ELA, 1, EU);
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 1000, 0)));
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 999, 0)));
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, ELA, 1000, 0)));
}

void test_reinicio_da_outra_placa() {
  SessaoPar par;
  avaliarMensagem(par, EU, ELA, 500, EU);
  // Depois de reiniciar ela ainda não conhece a nossa sessão: espera o eco.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Aceite::SEM_ECO),
                        static_cast<int>(avaliarMensagem(par, EU, ELA_DEPOIS_DE_REINICIAR, 1, 0)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Aceite::NOVA_SESSAO),
                        static_cast<int>(avaliarMensagem(par, EU, ELA_DEPOIS_DE_REINICIAR, 2, EU)));
  // Uma mensagem gravada da sessão antiga não volta a valer, nem com o eco certo.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Aceite::SESSAO_ANTIGA), static_cast<int>(avaliarMensagem(par, EU, ELA, 501, EU)));
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA_DEPOIS_DE_REINICIAR, 3, 0)));
}

void test_mensagem_anterior_a_sessao_e_recusada() {
  // Esta placa reiniciou e voltou a aceitar a outra pela mensagem 200. Uma mensagem gravada antes
  // disso (mesma sessão da outra placa, seq 190) não pode ser aceita, mesmo dentro da janela.
  SessaoPar par;
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 200, EU)));
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, ELA, 190, 0)));
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, ELA, 199, 0)));
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 202, 0)));
  TEST_ASSERT_TRUE(aceitou(avaliarMensagem(par, EU, ELA, 201, 0)));  // depois do 200, fora de ordem, vale
}

void test_mensagem_de_antes_de_eu_ligar_e_recusada() {
  // Mensagem gravada antes desta placa reiniciar: traz o eco da sessão antiga desta placa.
  SessaoPar par;
  const uint32_t EU_ANTES = 0x44444444;
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, ELA, 7, EU_ANTES)));
}

void test_sessao_zero_nunca_vale() {
  SessaoPar par;
  TEST_ASSERT_FALSE(aceitou(avaliarMensagem(par, EU, 0, 1, EU)));
}

static int rodarTestes() {
  UNITY_BEGIN();
  RUN_TEST(test_abre_com_a_outra_fechada);
  RUN_TEST(test_nao_abre_com_a_outra_liberada);
  RUN_TEST(test_nao_abre_com_a_outra_aberta_mesmo_depois_do_tempo);
  RUN_TEST(test_nao_abre_com_a_outra_em_liberacao);
  RUN_TEST(test_bypass_libera_sempre);
  RUN_TEST(test_sem_comunicacao_segue_a_regra);
  RUN_TEST(test_em_liberacao_vale_mesmo_sem_comunicacao);
  RUN_TEST(test_quem_libera);
  RUN_TEST(test_motivo_vindo_da_rede);
  RUN_TEST(test_codigos_de_origem_vao_e_voltam);
  RUN_TEST(test_duas_liberadas_e_fechadas_b_cede);
  RUN_TEST(test_a_outra_ja_abriu_a_fechada_cede);
  RUN_TEST(test_porta_ja_aberta_nao_cancela);
  RUN_TEST(test_bypass_nao_cancela);
  RUN_TEST(test_sem_comunicacao_nao_cancela);
  RUN_TEST(test_outra_fechada_nao_cancela);
  RUN_TEST(test_primeiro_contato_precisa_do_eco);
  RUN_TEST(test_mesma_mensagem_duas_vezes_e_recusada);
  RUN_TEST(test_fora_de_ordem_dentro_da_janela_aceita_uma_vez);
  RUN_TEST(test_velha_demais_e_recusada);
  RUN_TEST(test_salto_grande_limpa_a_janela);
  RUN_TEST(test_reinicio_da_outra_placa);
  RUN_TEST(test_mensagem_anterior_a_sessao_e_recusada);
  RUN_TEST(test_mensagem_de_antes_de_eu_ligar_e_recusada);
  RUN_TEST(test_sessao_zero_nunca_vale);
  return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup() {
  delay(2000);  // tempo do monitor serial conectar
  rodarTestes();
}
void loop() {}
#else
int main() { return rodarTestes(); }
#endif
