#pragma once
// Lógica do papel desta placa. Implementada em porta.cpp + web_porta.cpp (PORTA_A/PORTA_B) ou em
// porteiro.cpp + web_porteiro.cpp (PORTEIRO), conforme o ambiente gravado.

#include "papel.h"

class JsonEscritor;
class WebServer;

void iniciarLogica();  // primeira coisa do setup: relés e buzzers desligados (porta travada)
void atualizarLogica(unsigned long agora);
void tratarMensagem(Papel origem, char tipo, const char* dados, unsigned long agora);
void colocarEmEstadoSeguro();  // antes de reiniciar ou gravar firmware: porta travada, buzzers desligados

void logicaJson(JsonEscritor& j, unsigned long agora);  // parte do papel em /api/estado
void registrarRotasLogica(WebServer& servidor);         // página e rotas do papel
