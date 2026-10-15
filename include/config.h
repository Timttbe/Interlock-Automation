#pragma once
// Configuração de fábrica do Interlock (ESP32).
//
// O que dá para mudar pela página da placa (Wi-Fi, IP, tempos da porta) fica salvo na flash e
// vale mais que os valores daqui. Os valores daqui valem na primeira gravação, quando o Wi-Fi de
// fábrica abaixo muda, ou depois de restaurar a rede (botão BOOT da placa apertado por 10 s).

#if __has_include("segredos.h")
#include "segredos.h"
#else
#include "segredos.exemplo.h"
#endif

// As mesmas senhas de segredos.exemplo.h: a página avisa enquanto alguma delas estiver em uso.
#define SENHA_ADMIN_EXEMPLO "interlock123"
#define CHAVE_REDE_EXEMPLO "troque-esta-chave-do-intertravamento"

#define FW_VERSAO "7.0.0"

// ===================== Wi-Fi de fábrica =====================
// Vazio = a placa liga só a rede própria ("Interlock-...") até o Wi-Fi ser configurado pela página.
#define WIFI_SSID_PADRAO ""
#define WIFI_SENHA_PADRAO ""

// ===================== Pinos (ESP32 DevKit / NodeMCU-32S) =====================
// Entradas: contato entre o pino e o GND (pull-up interno). Ficaram de fora de propósito os pinos
// 0, 2, 5, 12 e 15 (definem o modo de boot), 6 a 11 (flash), 1 e 3 (USB) e 34 a 39 (sem pull-up).
// Os mesmos números nas 3 placas: entradas 32, 33 e 25; relés 26 e 27.

// PORTA_A / PORTA_B
#define PINO_SENSOR 32     // sensor magnético da porta
#define PINO_FACIAL 33     // contato seco do leitor facial
#define PINO_BOTAO 25      // botão de abertura
#define PINO_IMA 26        // relé 1: eletroímã
#define PINO_SINALEIRA 27  // relé 2: sinaleira puxe/empurre

// PORTEIRO
#define PINO_BOTAO_A 32       // abre a porta A
#define PINO_BOTAO_B 33       // abre a porta B
#define PINO_CHAVE_BYPASS 25  // chave do bypass (contato fechado = bypass ligado)
#define PINO_BUZZER_A 26      // buzzer da porta A, direto no pino
#define PINO_BUZZER_B 27      // buzzer da porta B, direto no pino

#define PINO_LED 2  // LED da placa (-1 se não tiver): aceso = Wi-Fi ok, piscando = sem Wi-Fi
#define LED_ATIVO_EM HIGH
#define PINO_BOOT 0  // botão BOOT da placa: segurar 10 s (LED pisca rápido) e soltar restaura a rede de fábrica

// ===================== Níveis =====================
// Nível que LIGA o relé: tem que bater com o jumper de gatilho do módulo (H/L). Com o jumper em L:
// em repouso o pino fica em HIGH (relé desligado, porta travada) e vai para LOW só durante a
// liberação. Se o relé ficar ligado em repouso e desligar no gatilho, o jumper e este valor não
// batem: com o jumper em H, use HIGH aqui.
#define RELE_ATIVO_EM LOW
// Buzzer ATIVO ligado direto no pino (ou por transistor): HIGH = tocando. Direto no pino, ele precisa
// consumir no máximo uns 20 mA a 3,3 V; senão, use um transistor. Buzzer num relé do módulo: LOW.
#define BUZZER_ATIVO_EM HIGH
// Sensor magnético NA (fecha com o ímã): porta fechada = LOW, aberta = HIGH. Assim um fio
// rompido aparece como porta aberta, que é o lado seguro (bloqueia a outra porta).
#define SENSOR_ABERTO_EM HIGH

// Eletroímã: ligue no contato NF do relé. Relé desligado = ímã energizado = porta travada; o relé
// só liga durante o tempo de abertura. Se a placa travar ou reiniciar, o relé desliga e a porta trava.

// ===================== Funcionamento (padrões; mudam pela página) =====================
#define TEMPO_ABERTURA_PADRAO_S 5  // quanto tempo o eletroímã fica solto
#define TEMPO_ALARME_PADRAO_S 60   // porta aberta por mais que isso = buzzer na portaria
// Sem notícia da outra porta (roteador fora, placa desligada...):
//   true  = libera mesmo assim: ninguém fica preso entre as portas, mas o intertravamento não é garantido;
//   false = bloqueia: intertravamento garantido, mas ninguém passa até a comunicação voltar.
#define LIBERAR_SEM_COMUNICACAO_PADRAO true
// Trava de novo assim que a porta abre e fecha, sem esperar o fim do tempo de abertura.
#define TRAVAR_AO_FECHAR true
// Buzzer intermitente (meio segundo ligado, meio desligado). Com buzzer em relé, prefira contínuo
// (o relé se desgasta ligando e desligando); direto no pino, tanto faz.
#define BUZZER_INTERMITENTE false

// ===================== Tempos internos =====================
#define DEBOUNCE_MS 50
#define DEBOUNCE_SENSOR_MS 80
#define DEBOUNCE_CHAVE_MS 100
#define ENTRADA_TRAVADA_MS 10000  // facial/botão acionado por mais que isso = fio em curto?

#define PORTA_UDP 4211
#define INTERVALO_ESTADO_MS 1000  // cada placa avisa o próprio estado nesse intervalo e quando ele muda
#define REPETICAO_ESTADO_MS 150   // estado que mudou vai na hora e mais 2 vezes nesse intervalo
#define SEM_COMUNICACAO_MS 5000   // sem notícia de uma placa por esse tempo = sem comunicação
#define INICIANDO_MS 5000         // logo depois de ligar a placa ainda não sabe o estado das outras
#define REENVIO_COMANDO_MS 150    // repete pedido/comando sem resposta nesse intervalo
#define PRAZO_PEDIDO_MS 1000      // a porta espera a portaria responder (sem resposta: nega)
#define PRAZO_COMANDO_MS 1000     // a portaria espera a porta confirmar um "abrir"
#define PRAZO_LIBERACAO_MS 2000   // a portaria segura a outra porta até a liberada confirmar

#define AP_SEM_WIFI_MS 30000           // sem Wi-Fi por esse tempo: liga a rede própria
// Rede própria também nos primeiros minutos depois de ligar, mesmo com o Wi-Fi conectado (útil
// para corrigir um IP fixo errado sem ir até a placa). 0 = não (IP errado: botão BOOT por 10 s).
#define AP_NO_BOOT_MS 0
#define AP_EM_USO_MAX_MS 120000        // Wi-Fi voltou com alguém na rede própria: ela fica até 2 min, para não derrubar a página
#define INTERVALO_RECONEXAO_MS 15000          // sem Wi-Fi: tenta reconectar nesse intervalo (para sempre)
#define INTERVALO_RECONEXAO_COM_AP_MS 60000   // idem, com alguém conectado na rede própria (cada tentativa a deixa instável)
#define RESTAURAR_REDE_MS 10000        // botão BOOT apertado por esse tempo (e no máximo 3x isso)
#define WATCHDOG_S 15                  // programa travado por esse tempo: a placa reinicia
