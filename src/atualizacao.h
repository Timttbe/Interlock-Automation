#pragma once
// Atualização do firmware pela rede: POST /api/atualizar com o arquivo firmware.bin.
// Usada pela página (Manutenção) e pelos ambientes *_ota do platformio.ini.

void registrarRotaAtualizacao();
