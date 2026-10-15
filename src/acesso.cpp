#include "acesso.h"
#include <mbedtls/base64.h>
#include "registro.h"
#include "web.h"

namespace {

constexpr uint8_t MAX_SESSOES = 8;
constexpr size_t TAM_TOKEN = 32;
constexpr unsigned long SESSAO_PARADA_MS = 12UL * 60 * 60 * 1000;
constexpr uint8_t TENTATIVAS = 5;
constexpr unsigned long JANELA_TENTATIVAS_MS = 60000;
constexpr unsigned long BLOQUEIO_MS = 30000;
constexpr unsigned long LEMBRAR_BASIC_MS = 600000;

struct Sessao {
  bool usada = false;
  char token[TAM_TOKEN + 1] = "";
  char nome[TAM_NOME_MAX + 1] = "";
  unsigned long ultimoUso = 0;
};
Sessao sessoes[MAX_SESSOES];

// Senhas erradas
uint8_t erradas = 0;
unsigned long primeiraErrada = 0;
bool bloqueado = false;
unsigned long bloqueadoEm = 0;

// Último usuário e senha no cabeçalho (curl): conferir de novo a cada pedido custaria ~100 ms.
String ultimoBasic;
unsigned long ultimoBasicEm = 0;
bool ultimoBasicAdmin = false;

const char* CABECALHOS[] = {"Cookie"};  // collectHeaders() não aceita a lista como const

String ipDoPedido() { return servidor.client().remoteIP().toString(); }

// Valor do cookie "sessao" do pedido, ou vazio.
String tokenDoPedido() {
  String cookies = servidor.header("Cookie");
  int inicio = 0;
  while (inicio < static_cast<int>(cookies.length())) {
    int fim = cookies.indexOf(';', inicio);
    if (fim < 0) fim = cookies.length();
    String par = cookies.substring(inicio, fim);
    par.trim();
    if (par.startsWith("sessao=")) return par.substring(7);
    inicio = fim + 1;
  }
  return String();
}

Sessao* sessaoDoPedido() {
  String token = tokenDoPedido();
  if (token.length() != TAM_TOKEN) return nullptr;
  unsigned long agora = millis();
  for (Sessao& s : sessoes) {
    if (!s.usada || token != s.token) continue;
    if (agora - s.ultimoUso > SESSAO_PARADA_MS || !contaExiste(s.nome)) {
      s.usada = false;
      return nullptr;
    }
    s.ultimoUso = agora;
    return &s;
  }
  return nullptr;
}

// Lugar para uma sessão nova: uma vaga, ou a usada há mais tempo.
Sessao& vaga() {
  Sessao* escolhida = &sessoes[0];
  for (Sessao& s : sessoes) {
    if (!s.usada) return s;
    if (millis() - s.ultimoUso > millis() - escolhida->ultimoUso) escolhida = &s;
  }
  return *escolhida;
}

bool loginBloqueado(unsigned long agora) {
  if (bloqueado && agora - bloqueadoEm >= BLOQUEIO_MS) {
    bloqueado = false;
    erradas = 0;
  }
  return bloqueado;
}

void anotarSenhaErrada(unsigned long agora) {
  if (erradas == 0 || agora - primeiraErrada > JANELA_TENTATIVAS_MS) {
    erradas = 0;
    primeiraErrada = agora;
  }
  if (++erradas >= TENTATIVAS) {
    bloqueado = true;
    bloqueadoEm = agora;
    registrar("Login bloqueado por 30 s: muitas senhas erradas (%s)", ipDoPedido().c_str());
  }
}

void tratarEntrar() {
  unsigned long agora = millis();
  if (loginBloqueado(agora)) return responderErro("Muitas senhas erradas. Espere 30 segundos.", 429);
  String nome = servidor.arg("usuario");
  nome.trim();
  nome.toLowerCase();
  String senha = servidor.arg("senha");
  Nivel nivel;
  if (!conferirSenha(nome.c_str(), senha.c_str(), nivel)) {
    // Só mostra o nome se a conta existe: alguém pode ter digitado a senha no campo do usuário.
    if (contaExiste(nome.c_str())) {
      registrar("Senha errada na página para \"%s\" (%s)", nome.c_str(), ipDoPedido().c_str());
    } else {
      registrar("Tentativa de login com usuário que não existe (%s)", ipDoPedido().c_str());
    }
    anotarSenhaErrada(agora);
    return responderErro("Usuário ou senha errados.", 401);
  }
  erradas = 0;

  Sessao& s = vaga();
  static const char HEXA[] = "0123456789abcdef";
  for (size_t i = 0; i < TAM_TOKEN; i += 8) {
    uint32_t r = esp_random();
    for (size_t k = 0; k < 8; k++) s.token[i + k] = HEXA[(r >> (4 * k)) & 0x0F];
  }
  s.token[TAM_TOKEN] = 0;
  strlcpy(s.nome, nome.c_str(), sizeof(s.nome));
  s.ultimoUso = agora;
  s.usada = true;
  servidor.sendHeader("Set-Cookie", String("sessao=") + s.token + "; Path=/; HttpOnly; SameSite=Strict");
  registrar("Entrou na página: %s (%s)", s.nome, ipDoPedido().c_str());
  responderOk("Olá, " + nome + "!");
}

void tratarSair() {
  Sessao* s = sessaoDoPedido();
  if (s) {
    registrar("Saiu da página: %s", s->nome);
    s->usada = false;
  }
  servidor.sendHeader("Set-Cookie", "sessao=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
  responderOk("Até logo.");
}

// Usuário e senha de admin no cabeçalho Authorization (HTTP Basic), como manda o curl.
bool adminPeloCabecalho() {
  String cabecalho = servidor.header("Authorization");
  if (!cabecalho.startsWith("Basic ")) return false;
  unsigned long agora = millis();
  if (cabecalho == ultimoBasic && agora - ultimoBasicEm < LEMBRAR_BASIC_MS) return ultimoBasicAdmin;
  if (loginBloqueado(agora)) return false;

  String codificado = cabecalho.substring(6);
  codificado.trim();
  unsigned char texto[TAM_NOME_MAX + TAM_SENHA_MAX + 4];
  size_t tamanho = 0;
  bool admin = false;
  if (mbedtls_base64_decode(texto, sizeof(texto) - 1, &tamanho, reinterpret_cast<const unsigned char*>(codificado.c_str()),
                            codificado.length()) == 0) {
    texto[tamanho] = 0;
    char* doisPontos = strchr(reinterpret_cast<char*>(texto), ':');
    if (doisPontos) {
      *doisPontos = 0;
      Nivel nivel;
      admin = conferirSenha(reinterpret_cast<char*>(texto), doisPontos + 1, nivel) && nivel == Nivel::ADMIN;
    }
  }
  if (!admin) anotarSenhaErrada(agora);
  ultimoBasic = cabecalho;
  ultimoBasicEm = agora;
  ultimoBasicAdmin = admin;
  return admin;
}

}  // namespace

void iniciarAcesso() {
  servidor.collectHeaders(CABECALHOS, sizeof(CABECALHOS) / sizeof(CABECALHOS[0]));
  servidor.on("/api/entrar", HTTP_POST, tratarEntrar);
  servidor.on("/api/sair", HTTP_POST, tratarSair);
}

bool exigirLogin(String* nome, Nivel* nivel) {
  Sessao* s = sessaoDoPedido();
  if (!s) {
    responderErro("Entre com seu usuário e senha.", 401);
    return false;
  }
  if (nome) *nome = s->nome;
  if (nivel) contaExiste(s->nome, nivel);
  return true;
}

bool exigirAdmin(String* nome) {
  Nivel nivel;
  if (!exigirLogin(nome, &nivel)) return false;
  if (nivel != Nivel::ADMIN) {
    responderErro("Só o admin pode fazer isso.", 403);
    return false;
  }
  return true;
}

bool adminNoPedido() {
  Sessao* s = sessaoDoPedido();
  Nivel nivel;
  if (s && contaExiste(s->nome, &nivel)) return nivel == Nivel::ADMIN;
  return adminPeloCabecalho();
}

void encerrarSessoesDe(const char* nome, bool menosAtual) {
  Sessao* atual = menosAtual ? sessaoDoPedido() : nullptr;
  for (Sessao& s : sessoes) {
    if (s.usada && &s != atual && strcmp(s.nome, nome) == 0) s.usada = false;
  }
}
