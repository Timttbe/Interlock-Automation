#pragma once
// Parte da página só da porta (ver pagina_comum.h).

static const char CORPO_PORTA[] = R"HTML(
<div class="grade">
<div class="card">
<div id="estado" class="estado est-off">...</div>
<div id="detalhes"></div>
<button class="abrir" id="btnAbrir">ABRIR</button>
<div class="res" id="resultado"></div>
</div>
<div class="card"><h3>Comunicação</h3><div id="comunicacao"></div></div>
</div>

<h2>Entradas e saídas</h2>
<div class="card" id="entradas"></div>

<h2>Funcionamento</h2>
<form class="card" id="formOp">
<label>Tempo de abertura (segundos)<input name="tempoAbertura" type="number" min="1" max="60" required></label>
<label>Buzzer na portaria com a porta aberta há mais de (segundos)<input name="tempoAlarme" type="number" min="5" max="3600" required></label>
<label>Sem a portaria e sem notícia da outra porta
<select name="semComunicacao">
<option value="1">Liberar mesmo assim (ninguém fica preso entre as portas)</option>
<option value="0">Bloquear (intertravamento sempre garantido)</option>
</select></label>
<div class="dica">Com a portaria fora do ar, mas a outra porta comunicando, as duas portas continuam se respeitando sozinhas.</div>
<button>Salvar</button>
</form>

<h2>Contas</h2>
<div class="card"><div id="contasInfo"></div>
<div class="dica">As contas são criadas e editadas na página da portaria; esta porta recebe uma cópia e a usa mesmo sem comunicação.</div></div>
)HTML";

static const char JS_PORTA[] = R"JS(
const MODOS={C:'com a portaria',P:'sem a portaria: decide com a outra porta',I:'isolada: sem a portaria e sem a outra porta'};
function rapido(){return !!(E&&E.porta&&E.porta.pedido)}
function desenhar(e){
  const p=e.porta;
  mostrarSituacao($('#estado'),p);
  $('#detalhes').innerHTML=linhasPorta(p)+linha('Sinaleira',p.sinaleira?'acesa':'apagada');
  const b=$('#btnAbrir');b.disabled=p.pedido;b.textContent=p.pedido?'PEDINDO À PORTARIA...':'ABRIR';
  mostrarResultado($('#resultado'),p.resultado);
  const o=p.outra;
  $('#comunicacao').innerHTML=
    linha('Funcionando',esc(MODOS[p.modo]||p.modo))+
    linha('Portaria',p.portaria?pill('comunicando','ok'):pill('sem comunicação','perigo'))+
    linha('Bypass',p.portaria?(p.bypass?pill('LIGADO','aviso'):'desligado'):'desconhecido')+
    linha(o.nome,!o.comunicando?pill('sem comunicação','perigo'):o.aberta?pill('aberta','aviso'):o.liberada?pill('liberada','aviso'):pill('fechada','ok'))+
    linha('Sem portaria e sem a outra porta',p.cfg.liberarSemComunicacao?'libera':'bloqueia');
  $('#entradas').innerHTML=listaES(p.entradas)+listaES(p.saidas);
  if(!opPreenchido){
    const f=$('#formOp');
    f.tempoAbertura.value=p.cfg.tempoAbertura;f.tempoAlarme.value=p.cfg.tempoAlarme;
    f.semComunicacao.value=p.cfg.liberarSemComunicacao?'1':'0';opPreenchido=true}
  const c=e.contas;
  $('#contasInfo').innerHTML=linha('Cópia da portaria',c.total+' conta(s) · '+
    (c.atualizada?pill('atualizada','ok'):c.portariaConhecida?pill('atualizando...','aviso'):pill('portaria sem comunicação','perigo')))+
    (c.lista?c.lista.map(x=>linha(x.nome,x.admin?pill('admin','aviso'):pill('usuário'))).join(''):'')}

$('#btnAbrir').onclick=()=>acao('/api/abrir');
ciclo();
)JS";
