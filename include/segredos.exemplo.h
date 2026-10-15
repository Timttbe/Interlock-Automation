#pragma once
// Senhas do Interlock (exemplo).
//
// Para usar senhas próprias, copie este arquivo para "segredos.h", nesta mesma pasta, e troque
// os valores. O segredos.h não vai para o git. As 3 placas precisam ser gravadas com os mesmos
// valores. Enquanto estas senhas de exemplo estiverem em uso, a página mostra um aviso.

// Senha da página (usuário "admin"), da rede própria da placa e da atualização pela rede.
// De 8 a 63 caracteres.
#define SENHA_ADMIN "interlock123"

// Chave que assina as mensagens entre as placas: sem ela, ninguém na rede consegue mandar uma
// porta abrir. Pelo menos 16 caracteres.
#define CHAVE_REDE "troque-esta-chave-do-intertravamento"
