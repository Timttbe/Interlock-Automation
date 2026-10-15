#pragma once
// Parte da página só da portaria (ver pagina_comum.h).

static const char CORPO_PORTEIRO[] = R"HTML(
<div class="grade">
<div class="card"><h3>Porta A</h3>
<div id="estA" class="estado est-off">...</div><div id="detA"></div>
<button class="abrir" data-porta="A">ABRIR PORTA A</button><div class="res" id="resA"></div></div>
<div class="card"><h3>Porta B</h3>
<div id="estB" class="estado est-off">...</div><div id="detB"></div>
<button class="abrir" data-porta="B">ABRIR PORTA B</button><div class="res" id="resB"></div></div>
</div>

<h2>Bypass</h2>
<div class="card">
<div id="bypassEstado" class="estado est-off">BYPASS</div>
<div id="bypassInfo"></div>
<button class="sec" id="btnBypass">Ligar bypass pela página</button>
<div class="dica">Com o bypass ligado (pela chave ou pela página), as duas portas podem abrir ao mesmo tempo. O bypass da página desliga se a portaria reiniciar.</div>
</div>

<h2>Entradas e saídas</h2>
<div class="card" id="entradas"></div>

<h2>Funcionamento</h2>
<form class="card" id="formOp">
<label>Quando uma porta está sem comunicação com a portaria
<select name="semComunicacao">
<option value="1">Liberar a outra porta mesmo assim</option>
<option value="0">Bloquear a outra porta</option>
</select></label>
<div class="dica">Tempo de abertura, tempo do alarme e a regra para quando a própria porta fica sem a portaria ficam na página de cada porta.</div>
<button>Salvar</button>
</form>
)HTML";

static const char JS_PORTEIRO[] = R"JS(
const MODOS={C:'com a portaria',P:'sem a portaria',I:'isolada'};
function rapido(){return !!(E&&E.portaria&&E.portaria.portas.some(p=>p.resultado&&p.resultado.aguardando))}
function desenhar(e){
  const pt=e.portaria;
  pt.portas.forEach(p=>{
    const L=p.letra;
    mostrarSituacao($('#est'+L),p);
    const permissao=linha('Abrir agora',pill(p.permissao,p.podeAbrir?'ok':'aviso'));
    $('#det'+L).innerHTML=!p.comunicando?permissao:linhasPorta(p)+
      linha('Buzzer',p.buzzer?pill('TOCANDO','perigo'):'desligado')+
      linha('Funcionando',esc(MODOS[p.modo]||p.modo))+permissao;
    mostrarResultado($('#res'+L),p.resultado)});
  const be=$('#bypassEstado');
  be.className='estado '+(pt.bypass?'est-liberada':'est-fechada');be.textContent='BYPASS '+(pt.bypass?'LIGADO':'DESLIGADO');
  $('#bypassInfo').innerHTML=linha('Chave',pt.chave?pill('ligada','aviso'):'desligada')+linha('Pela página',pt.virtual?pill('ligado','aviso'):'desligado');
  const b=$('#btnBypass');b.textContent=pt.virtual?'Desligar bypass da página':'Ligar bypass pela página';b.className=pt.virtual?'':'sec';
  $('#entradas').innerHTML=listaES(pt.entradas)+listaES(pt.saidas);
  if(!opPreenchido){$('#formOp').semComunicacao.value=pt.cfg.liberarSemComunicacao?'1':'0';opPreenchido=true}}

document.querySelectorAll('button[data-porta]').forEach(b=>b.onclick=()=>acao('/api/abrir',{porta:b.dataset.porta}));
$('#btnBypass').onclick=()=>{
  if(!E)return;
  const ligar=!E.portaria.virtual;
  if(ligar&&!confirm('Ligar o bypass? As duas portas vão poder abrir ao mesmo tempo.'))return;
  acao('/api/bypass',{ligar:ligar?1:0})};
ciclo();
)JS";
