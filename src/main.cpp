// Interlock Automation: intertravamento de duas portas com ESP32.
//
// O mesmo firmware vai nas 3 placas; o papel (PORTEIRO, PORTA_A ou PORTA_B) vem do ambiente
// gravado no platformio.ini. Onde está cada coisa:
//   porta.cpp / porteiro.cpp       lógica de cada papel (entradas, relés, pedidos de abertura)
//   intertravamento.cpp            regras: quem pode abrir e quando
//   protocolo.cpp + sessao.cpp     mensagens UDP assinadas entre as placas
//   rede_wifi.cpp                  Wi-Fi do roteador, IP fixo e rede própria de socorro
//   web*.cpp + pagina_*.h          página de cada placa; atualizacao.cpp: firmware pela rede
//   configuracao.cpp               o que fica salvo na flash
//   registro.cpp                   eventos mostrados na página
//   sistema.cpp                    LED, botão BOOT, watchdog e reinício
//   include/config.h               pinos, níveis dos relés e tempos

#include <Arduino.h>
#include "config.h"
#include "configuracao.h"
#include "logica.h"
#include "papel.h"
#include "protocolo.h"
#include "rede_wifi.h"
#include "registro.h"
#include "sistema.h"
#include "web.h"

void setup() {
  iniciarLogica();  // primeiro de tudo: relés e buzzers desligados (porta travada)
  Serial.begin(115200);
  carregarConfiguracao();
  registrar("%s ligou. Firmware %s. Último reinício: %s", nomeExibicao(MEU_PAPEL), FW_VERSAO, motivoDoReinicio());
  iniciarSistema();
  iniciarWifi();
  iniciarProtocolo(tratarMensagem);
  iniciarWeb();
}

void loop() {
  atualizarWifi(millis());
  atualizarProtocolo(millis());
  atualizarLogica(millis());
  atualizarWeb();
  atualizarSistema(millis());
}
