#include "atualizacao.h"
#include <Update.h>
#include "acesso.h"
#include "config.h"
#include "logica.h"
#include "papel.h"
#include "registro.h"
#include "sistema.h"
#include "web.h"

namespace {

// Marca que vai dentro do firmware. A placa só aceita um arquivo com a marca do próprio papel:
// assim não dá para gravar por engano o firmware da portaria numa porta, ou o da porta A na B.
const char MARCA_FIRMWARE[] = "<INTERLOCK-FW:" NOME_PAPEL ">";

bool iniciada = false;
String erro;
size_t marcaCasada = 0;  // quantos caracteres da marca apareceram em sequência no arquivo
bool marcaEncontrada = false;

// Procura a marca no arquivo, pedaço por pedaço. Como o '<' só aparece no começo da marca,
// basta recomeçar a contagem quando um caractere não bate.
void procurarMarca(const uint8_t* dados, size_t tamanho) {
  const size_t tamanhoMarca = sizeof(MARCA_FIRMWARE) - 1;
  for (size_t i = 0; i < tamanho && !marcaEncontrada; i++) {
    if (dados[i] == static_cast<uint8_t>(MARCA_FIRMWARE[marcaCasada])) {
      marcaEncontrada = ++marcaCasada == tamanhoMarca;
    } else {
      marcaCasada = dados[i] == static_cast<uint8_t>(MARCA_FIRMWARE[0]) ? 1 : 0;
    }
  }
}

// Recebe o arquivo enquanto ele chega. A resposta vai depois, em concluir().
// Enquanto o arquivo chega o resto do programa fica parado: por isso a placa vai antes para o
// estado seguro (porta travada, buzzers desligados).
void receber() {
  HTTPUpload& envio = servidor.upload();
  switch (envio.status) {
    case UPLOAD_FILE_START:
      // Sobra de um envio anterior que caiu no meio (nesse caso concluir() não chega a rodar).
      if (Update.isRunning()) Update.abort();
      iniciada = false;
      erro = "";
      marcaCasada = 0;
      marcaEncontrada = false;
      if (!adminNoPedido()) return;  // admin logado na página, ou usuário e senha de admin (curl)
      registrar("Recebendo firmware \"%s\"...", envio.filename.c_str());
      colocarEmEstadoSeguro();
      iniciada = Update.begin(UPDATE_SIZE_UNKNOWN);
      if (!iniciada) erro = Update.errorString();
      break;
    case UPLOAD_FILE_WRITE:
      feedLoopWDT();
      if (!iniciada || erro.length()) return;
      procurarMarca(envio.buf, envio.currentSize);
      if (Update.write(envio.buf, envio.currentSize) != envio.currentSize) erro = Update.errorString();
      break;
    case UPLOAD_FILE_END:
      if (!iniciada || erro.length()) return;
      if (!marcaEncontrada) {
        erro = String("este arquivo não é o firmware da ") + nomeExibicao(MEU_PAPEL) +
               " (use o firmware.bin da pasta .pio/build/" AMBIENTE_PIO ")";
      } else if (!Update.end(true)) {
        erro = Update.errorString();
      }
      break;
    case UPLOAD_FILE_ABORTED:
      // A conexão caiu no meio: o servidor não chama concluir(), então descarta aqui.
      if (Update.isRunning()) Update.abort();
      if (iniciada) registrar("Envio do firmware interrompido: a placa continua com o firmware atual");
      iniciada = false;
      erro = "o envio do arquivo foi interrompido";
      break;
  }
}

void concluir() {
  if (!adminNoPedido()) return responderErro("Só o admin pode gravar firmware.", 403);
  if (!iniciada && erro.isEmpty()) erro = "nenhum arquivo recebido";
  if (erro.isEmpty()) {
    registrar("Firmware novo gravado");
    responderOk("Firmware gravado. A placa vai reiniciar com a versão nova.");
    agendarReinicio("firmware novo");
    return;
  }
  if (Update.isRunning()) Update.abort();  // descarta o que foi gravado pela metade
  registrar("Firmware não gravado: %s", erro.c_str());
  responderErro("Firmware não gravado: " + erro + ". A placa continua com o firmware atual.");
}

}  // namespace

void registrarRotaAtualizacao() { servidor.on("/api/atualizar", HTTP_POST, concluir, receber); }
