#pragma once
// Registro de eventos (liberações, negações, alarmes, comunicação...). Guarda os últimos na
// memória para a página e também escreve no monitor serial.

class JsonEscritor;

void registrar(const char* formato, ...) __attribute__((format(printf, 1, 2)));

// Escreve os eventos na lista JSON já aberta, do mais recente para o mais antigo.
void registroJson(JsonEscritor& j);
