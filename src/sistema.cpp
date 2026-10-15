#include "sistema.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "configuracao.h"
#include "entradas.h"
#include "logica.h"
#include "rede_wifi.h"
#include "registro.h"

static_assert(sizeof(SENHA_ADMIN) - 1 >= 8 && sizeof(SENHA_ADMIN) - 1 <= 63,
              "SENHA_ADMIN precisa ter de 8 a 63 caracteres (é também a senha da rede própria)");
static_assert(sizeof(CHAVE_REDE) - 1 >= 16, "CHAVE_REDE precisa ter pelo menos 16 caracteres");

namespace {

Saida led(PINO_LED, LED_ATIVO_EM);
Entrada botaoBoot(PINO_BOOT, LOW, DEBOUNCE_MS);
bool redeRestaurada = false;
bool reiniciar = false;
unsigned long reiniciarPedidoEm = 0;
bool jaIniciou = false;

}  // namespace

void iniciarSistema() {
  led.iniciar();
  botaoBoot.iniciar();
  // Se o programa travar por WATCHDOG_S segundos, a placa reinicia (e, reiniciando, o relé
  // desliga e a porta trava). O loop avisa o watchdog a cada volta.
  esp_task_wdt_init(WATCHDOG_S, true);
  enableLoopWDT();
}

void atualizarSistema(unsigned long agora) {
  if (!jaIniciou && agora >= INICIANDO_MS) jaIniciou = true;

  // Botão BOOT: segurar 10 s (até o LED piscar bem rápido) e soltar restaura a rede de fábrica.
  // Só vale ao soltar, e não se ficar mais de 30 s apertado: em algumas placas o monitor serial
  // consegue segurar o GPIO0 em LOW pelo circuito de gravação automática.
  unsigned long apertadoHa = agora - botaoBoot.mudouEm();
  if (botaoBoot.atualizar(agora)) {
    if (botaoBoot.ativa()) {
      registrar("Botão BOOT apertado: segure 10 s, até o LED piscar rápido, e solte para restaurar a rede");
    } else if (!redeRestaurada && apertadoHa >= RESTAURAR_REDE_MS && apertadoHa <= 3 * RESTAURAR_REDE_MS) {
      redeRestaurada = true;
      restaurarRedeDeFabrica();
      registrar("Rede de fábrica restaurada pelo botão BOOT (Wi-Fi do config.h, IP automático, grupo 1)");
      agendarReinicio("rede restaurada");
    }
  }
  apertadoHa = agora - botaoBoot.mudouEm();
  bool bootPronto = botaoBoot.ativa() && apertadoHa >= RESTAURAR_REDE_MS && apertadoHa <= 3 * RESTAURAR_REDE_MS;

  // LED: aceso = Wi-Fi conectado; piscando devagar = sem Wi-Fi; rápido = rede própria ligada.
  bool aceso;
  if (bootPronto) {
    aceso = (agora / 60) % 2;
  } else if (wifiConectado()) {
    aceso = true;
  } else {
    aceso = (agora / (redePropriaLigada() ? 150 : 500)) % 2;
  }
  led.ligar(aceso);

  if (reiniciar && agora - reiniciarPedidoEm >= 1500) {
    colocarEmEstadoSeguro();
    Serial.flush();
    ESP.restart();
  }
}

void agendarReinicio(const char* motivo) {
  if (reiniciar) return;
  registrar("Reiniciando: %s", motivo);
  reiniciar = true;
  reiniciarPedidoEm = millis();
}

const char* motivoDoReinicio() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "ligou na energia";
    case ESP_RST_EXT: return "botão EN/RST";
    case ESP_RST_SW: return "reiniciada pelo programa (página, rede nova ou firmware novo)";
    case ESP_RST_PANIC: return "erro no programa (exceção)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "travou (watchdog)";
    case ESP_RST_BROWNOUT: return "queda de tensão na alimentação (brownout): confira a fonte";
    case ESP_RST_DEEPSLEEP: return "saiu do modo de economia";
    default: return "desconhecido";
  }
}

bool iniciando() { return !jaIniciou && millis() < INICIANDO_MS; }
