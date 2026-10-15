#pragma once
// LED de situação, botão BOOT (restaurar a rede), watchdog e reinício.

void iniciarSistema();
void atualizarSistema(unsigned long agora);

// Reinicia daqui a 1,5 s, para dar tempo da resposta da página chegar.
void agendarReinicio(const char* motivo);
const char* motivoDoReinicio();
// Primeiros segundos depois de ligar: ainda não deu tempo de saber o estado das outras placas.
bool iniciando();
