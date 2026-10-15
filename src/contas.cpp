#include "contas.h"
#include <Preferences.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include "config.h"
#include "json.h"
#include "papel.h"
#include "protocolo.h"
#include "registro.h"

namespace {

constexpr size_t TAM_SAL = 16;
constexpr size_t TAM_RESUMO = 32;
constexpr size_t TAM_SEGREDO = TAM_SAL + TAM_RESUMO;  // o que viaja cifrado para as portas
constexpr unsigned ITERACOES = 2000;
constexpr unsigned long INTERVALO_PEDIDO_MS = 3000;
const char* const ESPACO_CONTAS = "contas";

struct Conta {
  char nome[TAM_NOME_MAX + 1];
  uint8_t nivel;
  uint8_t sal[TAM_SAL];
  uint8_t resumo[TAM_RESUMO];
};

Conta contas[MAX_CONTAS];
uint8_t totalContas = 0;
uint32_t versao = 0;  // 0 = de fábrica (só o admin, com a SENHA_ADMIN)
bool adminDeFabrica = true;

// Porta: última versão anunciada pela portaria e quando pediu a lista pela última vez
bool versaoDaPortariaConhecida = false;
uint32_t versaoDaPortaria = 0;
bool pediu = false;
unsigned long ultimoPedido = 0;

void resumir(const char* senha, const uint8_t* sal, uint8_t* resumo) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_pkcs5_pbkdf2_hmac(&ctx, reinterpret_cast<const unsigned char*>(senha), strlen(senha), sal, TAM_SAL,
                            ITERACOES, TAM_RESUMO, resumo);
  mbedtls_md_free(&ctx);
}

void preencher(Conta& c, const char* nome, Nivel nivel, const char* senha) {
  char copia[TAM_NOME_MAX + 1];
  strlcpy(copia, nome, sizeof(copia));  // na troca de senha, nome é o c.nome que o memset vai apagar
  memset(&c, 0, sizeof(c));
  strlcpy(c.nome, copia, sizeof(c.nome));
  c.nivel = static_cast<uint8_t>(nivel);
  for (size_t i = 0; i < TAM_SAL; i += 4) {
    uint32_t r = esp_random();
    memcpy(c.sal + i, &r, 4);
  }
  resumir(senha, c.sal, c.resumo);
}

// Compara sem parar na primeira diferença (não revela pelo tempo quanto do resumo bate).
bool iguais(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t diferenca = 0;
  for (size_t i = 0; i < n; i++) diferenca |= a[i] ^ b[i];
  return diferenca == 0;
}

int procurar(const char* nome) {
  for (uint8_t i = 0; i < totalContas; i++) {
    if (strcmp(contas[i].nome, nome) == 0) return i;
  }
  return -1;
}

void deFabrica() {
  totalContas = 1;
  preencher(contas[0], NOME_ADMIN, Nivel::ADMIN, SENHA_ADMIN);
  versao = 0;
}

void salvar() {
  Preferences p;
  p.begin(ESPACO_CONTAS, false);
  p.putUInt("versao", versao);
  p.putUChar("totalContas", totalContas);
  p.putBytes("lista", contas, sizeof(Conta) * totalContas);
  p.end();
}

void conferirAdminDeFabrica() {
  Nivel nivel;
  adminDeFabrica = conferirSenha(NOME_ADMIN, SENHA_ADMIN, nivel);
}

// Portaria: toda mudança ganha uma versão nova; as portas veem no estado dela e pedem a lista.
void mudou() {
  do {
    versao = esp_random();
  } while (versao == 0);
  salvar();
  conferirAdminDeFabrica();
}

String erroNoNome(const String& nome) {
  if (nome.length() == 0 || nome.length() > TAM_NOME_MAX) return F("O nome precisa ter de 1 a 20 caracteres.");
  for (size_t i = 0; i < nome.length(); i++) {
    char c = nome[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    if (!ok) return F("Use no nome só letras minúsculas sem acento, números, ponto, hífen ou sublinhado.");
  }
  return String();
}

String erroNaSenha(const String& senha) {
  if (senha.length() < TAM_SENHA_MIN || senha.length() > TAM_SENHA_MAX) {
    return F("A senha precisa ter de 6 a 63 caracteres.");
  }
  return String();
}

// Cifra (ou decifra: é a mesma operação) o sal e o resumo de uma conta que vai para as portas,
// com uma sequência tirada da CHAVE_REDE. A versão entra na sequência, então ela nunca se repete.
void cifrar(uint8_t* segredo, uint32_t versaoLista, uint8_t indice) {
  for (uint8_t bloco = 0; bloco * 32 < TAM_SEGREDO; bloco++) {
    char rotulo[40];
    int n = snprintf(rotulo, sizeof(rotulo), "contas|%08x|%u|%u", static_cast<unsigned>(versaoLista), indice, bloco);
    uint8_t sequencia[32];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), reinterpret_cast<const uint8_t*>(CHAVE_REDE),
                    sizeof(CHAVE_REDE) - 1, reinterpret_cast<const uint8_t*>(rotulo), n, sequencia);
    for (size_t i = 0; i < 32 && bloco * 32 + i < TAM_SEGREDO; i++) segredo[bloco * 32 + i] ^= sequencia[i];
  }
}

int valorHexa(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

}  // namespace

void iniciarContas() {
  Preferences p;
  p.begin(ESPACO_CONTAS, false);
  totalContas = p.getUChar("totalContas", 0);
  versao = p.getUInt("versao", 0);
  bool gravada = p.isKey("lista");
  bool ok = totalContas >= 1 && totalContas <= MAX_CONTAS && gravada &&
            p.getBytesLength("lista") == sizeof(Conta) * totalContas &&
            p.getBytes("lista", contas, sizeof(Conta) * totalContas) == sizeof(Conta) * totalContas;
  p.end();
  for (uint8_t i = 0; ok && i < totalContas; i++) contas[i].nome[TAM_NOME_MAX] = 0;
  if (!ok || strcmp(contas[0].nome, NOME_ADMIN) != 0 || contas[0].nivel != static_cast<uint8_t>(Nivel::ADMIN)) {
    if (gravada) registrar("Lista de contas gravada inválida: vale só o admin com a senha de fábrica");
    deFabrica();
  }
  conferirAdminDeFabrica();
}

bool conferirSenha(const char* nome, const char* senha, Nivel& nivel) {
  uint8_t resumo[TAM_RESUMO];
  int i = procurar(nome);
  if (i < 0) {
    // Gasta o mesmo tempo de uma conta que existe: o tempo não revela quais nomes existem.
    static const uint8_t SAL_QUALQUER[TAM_SAL] = {};
    resumir(senha, SAL_QUALQUER, resumo);
    return false;
  }
  resumir(senha, contas[i].sal, resumo);
  if (!iguais(resumo, contas[i].resumo, TAM_RESUMO)) return false;
  nivel = static_cast<Nivel>(contas[i].nivel);
  return true;
}

bool contaExiste(const char* nome, Nivel* nivel) {
  int i = procurar(nome);
  if (i < 0) return false;
  if (nivel) *nivel = static_cast<Nivel>(contas[i].nivel);
  return true;
}

bool senhaAdminDeFabrica() { return adminDeFabrica; }
uint32_t versaoContas() { return versao; }

String criarConta(const String& nome, const String& senha) {
  String erro = erroNoNome(nome);
  if (erro.isEmpty()) erro = erroNaSenha(senha);
  if (!erro.isEmpty()) return erro;
  if (procurar(nome.c_str()) >= 0) return F("Já existe uma conta com esse nome.");
  if (totalContas >= MAX_CONTAS) return F("Limite de contas atingido: apague uma antes.");
  preencher(contas[totalContas++], nome.c_str(), Nivel::USUARIO, senha.c_str());
  mudou();
  registrar("Conta criada: %s", nome.c_str());
  return String();
}

String apagarConta(const String& nome) {
  if (nome == NOME_ADMIN) return F("A conta admin não pode ser apagada.");
  int i = procurar(nome.c_str());
  if (i < 0) return F("Conta não encontrada.");
  for (uint8_t k = i; k + 1 < totalContas; k++) contas[k] = contas[k + 1];
  totalContas--;
  mudou();
  registrar("Conta apagada: %s", nome.c_str());
  return String();
}

String definirSenha(const String& nome, const String& senha) {
  String erro = erroNaSenha(senha);
  if (!erro.isEmpty()) return erro;
  int i = procurar(nome.c_str());
  if (i < 0) return F("Conta não encontrada.");
  preencher(contas[i], contas[i].nome, static_cast<Nivel>(contas[i].nivel), senha.c_str());
  mudou();
  registrar("Senha trocada: %s", nome.c_str());
  return String();
}

void restaurarSenhaAdmin() {
  preencher(contas[0], NOME_ADMIN, Nivel::ADMIN, SENHA_ADMIN);
  if (MEU_PAPEL == Papel::PORTEIRO) {
    mudou();  // as portas recebem a senha nova pela cópia
  } else {
    versao = 0;  // na próxima cópia da portaria volta a valer a senha de lá
    salvar();
    adminDeFabrica = true;
  }
}

void contasJson(JsonEscritor& j, bool admin) {
  j.abrirObjeto("contas");
  j.logico("daPortaria", MEU_PAPEL != Papel::PORTEIRO);
  j.numero("total", totalContas);
  j.numero("max", MAX_CONTAS);
  if (MEU_PAPEL != Papel::PORTEIRO) {
    j.logico("atualizada", versaoDaPortariaConhecida && versaoDaPortaria == versao);
    j.logico("portariaConhecida", versaoDaPortariaConhecida);
  }
  if (admin) {
    j.abrirLista("lista");
    for (uint8_t i = 0; i < totalContas; i++) {
      j.abrirObjeto();
      j.texto("nome", contas[i].nome);
      j.logico("admin", contas[i].nivel == static_cast<uint8_t>(Nivel::ADMIN));
      j.fecharObjeto();
    }
    j.fecharLista();
  }
  j.fecharObjeto();
}

// "v=<versão>;n=<quantas>;c=<nome>:<nível>:<sal e resumo cifrados, hexa>,..."
String contasParaMensagem() {
  static const char HEXA[] = "0123456789abcdef";
  String s;
  s.reserve(40 + totalContas * (TAM_NOME_MAX + 4 + TAM_SEGREDO * 2));
  char cabecalho[32];
  snprintf(cabecalho, sizeof(cabecalho), "v=%08x;n=%u;c=", static_cast<unsigned>(versao), totalContas);
  s += cabecalho;
  for (uint8_t i = 0; i < totalContas; i++) {
    uint8_t segredo[TAM_SEGREDO];
    memcpy(segredo, contas[i].sal, TAM_SAL);
    memcpy(segredo + TAM_SAL, contas[i].resumo, TAM_RESUMO);
    cifrar(segredo, versao, i);
    if (i) s += ',';
    s += contas[i].nome;
    s += ':';
    s += static_cast<char>('0' + contas[i].nivel);
    s += ':';
    for (uint8_t b : segredo) {
      s += HEXA[b >> 4];
      s += HEXA[b & 0x0F];
    }
  }
  return s;
}

void versaoContasDaPortaria(uint32_t versaoAnunciada, unsigned long agora) {
  versaoDaPortariaConhecida = true;
  versaoDaPortaria = versaoAnunciada;
  if (versaoAnunciada == versao || (pediu && agora - ultimoPedido < INTERVALO_PEDIDO_MS)) return;
  pediu = true;
  ultimoPedido = agora;
  char dados[16];
  snprintf(dados, sizeof(dados), "v=%08x", static_cast<unsigned>(versao));
  enviarPara(Papel::PORTEIRO, MSG_PEDIR_CONTAS, dados);
}

bool contasDaMensagem(const char* dados) {
  char texto[12];
  if (!campoTexto(dados, "v", texto, sizeof(texto))) return false;
  uint32_t versaoLista = strtoul(texto, nullptr, 16);
  long quantas = campoNumero(dados, "n", 0);
  const char* p = strstr(dados, ";c=");
  if (!p || quantas < 1 || quantas > MAX_CONTAS) return false;
  p += 3;

  // Monta numa lista separada e só troca se tudo estiver certo.
  Conta nova[MAX_CONTAS];
  uint8_t lidas = 0;
  while (*p && *p != ';' && lidas < quantas) {
    Conta& c = nova[lidas];
    memset(&c, 0, sizeof(c));
    size_t n = 0;
    while (*p && *p != ':' && n < TAM_NOME_MAX) c.nome[n++] = *p++;
    if (*p++ != ':' || n == 0 || erroNoNome(c.nome).length()) return false;
    if (*p != '0' && *p != '1') return false;
    c.nivel = *p++ - '0';
    if (*p++ != ':') return false;
    uint8_t segredo[TAM_SEGREDO];
    for (size_t b = 0; b < TAM_SEGREDO; b++) {
      int alto = valorHexa(p[0]), baixo = alto < 0 ? -1 : valorHexa(p[1]);
      if (baixo < 0) return false;
      segredo[b] = alto << 4 | baixo;
      p += 2;
    }
    cifrar(segredo, versaoLista, lidas);
    memcpy(c.sal, segredo, TAM_SAL);
    memcpy(c.resumo, segredo + TAM_SAL, TAM_RESUMO);
    lidas++;
    if (*p == ',') p++;
  }
  if (lidas != quantas || strcmp(nova[0].nome, NOME_ADMIN) != 0 || nova[0].nivel != static_cast<uint8_t>(Nivel::ADMIN)) {
    return false;
  }
  memcpy(contas, nova, sizeof(Conta) * lidas);
  totalContas = lidas;
  versao = versaoLista;
  salvar();
  conferirAdminDeFabrica();
  registrar("Contas atualizadas pela portaria (%u contas)", totalContas);
  return true;
}
