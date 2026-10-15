#pragma once
// Partes da página comuns às 3 placas. A página não usa nada da internet (fontes, bibliotecas):
// ela também precisa abrir pela rede própria da placa, onde não há internet.
//
// Montagem (web_porta.cpp / web_porteiro.cpp):
//   PAGINA_INICIO + CSS_COMUM + PAGINA_CABECALHO + corpo do papel + SECOES_COMUNS
//   + PAGINA_SCRIPT + JS_COMUM + script do papel + PAGINA_FIM
// O script do papel define desenhar(e) e rapido(), e chama ciclo() no fim.
//
// Sem login, a página mostra a tela de login (os dados vêm de /api/estado, que pede login).
// Elementos com a classe "so-admin" só aparecem para o admin (as rotas também conferem).

#include "config.h"

static const char PAGINA_INICIO[] = R"HTML(<!DOCTYPE html>
<html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Interlock</title>
<script>try{document.documentElement.dataset.tema=localStorage.getItem('tema')||(matchMedia('(prefers-color-scheme: light)').matches?'claro':'escuro')}catch(e){document.documentElement.dataset.tema='escuro'}</script>
<style>)HTML";

static const char CSS_COMUM[] = R"CSS(
:root{color-scheme:dark;--fundo:#0b0d10;--card:#14181d;--card2:#1b2027;--borda:#2a313a;--linha:#1f252d;--texto:#e6e9ee;
--mudo:#8a94a3;--ouro:#f5a623;--ouro2:#d48c14;--ouroTexto:#f5a623;--ouroFundo:#33260c;--ouroBorda:#7a5512;
--ok:#3fb950;--okBorda:#2e6b39;--perigo:#f85149;--perigoFundo:#3d1416;--perigoBorda:#7d2a27;--perigoTexto:#ffb3ae;
--info:#58a6ff;--infoFundo:#132235;--laranja:#f0883e;--laranjaFundo:#3a2412;--sombra:rgba(0,0,0,.5)}
:root[data-tema=claro]{color-scheme:light;--fundo:#f3f4f6;--card:#fff;--card2:#f0f2f5;--borda:#d5dae1;--linha:#e8ebef;
--texto:#1b1f24;--mudo:#5d6877;--ouro:#f0a500;--ouro2:#d29000;--ouroTexto:#9a6400;--ouroFundo:#fff3d6;--ouroBorda:#e9c56f;
--ok:#1a7f37;--okBorda:#9fd3ad;--perigo:#cf222e;--perigoFundo:#ffe9e9;--perigoBorda:#f2b6b6;--perigoTexto:#a11d27;
--info:#0969da;--infoFundo:#e1edfc;--laranja:#bc4c00;--laranjaFundo:#ffe9d9;--sombra:rgba(0,0,0,.15)}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--fundo);color:var(--texto);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
body:not(.logado) header,body:not(.logado) main{display:none}
body:not(.admin) .so-admin{display:none!important}
.logo{display:block;height:30px;width:auto}
:root[data-tema=claro] .logo-escuro,:root:not([data-tema=claro]) .logo-claro{display:none}
header{position:sticky;top:0;z-index:5;background:var(--card);border-bottom:1px solid var(--borda);padding:8px 16px;display:flex;align-items:center;gap:12px;flex-wrap:wrap}
.marca{display:flex;flex-direction:column;gap:2px}
.marca span{font-size:10px;font-weight:700;letter-spacing:3px;color:var(--mudo);text-transform:uppercase}
header .dir{margin-left:auto;display:flex;gap:6px;flex-wrap:wrap;align-items:center}
#topo{display:flex;gap:6px;flex-wrap:wrap}
.quem{font-size:13px;color:var(--mudo)}
button.icone{margin:0;padding:5px 10px;font-size:13px;font-weight:600;background:transparent;color:var(--texto);border:1px solid var(--borda)}
button.icone:hover{background:var(--card2)}
main{max-width:1000px;margin:0 auto;padding:16px}
h2{font-size:13px;letter-spacing:2px;text-transform:uppercase;color:var(--ouroTexto);margin:26px 0 10px;border-bottom:1px solid var(--borda);padding-bottom:6px}
h3{margin:0 0 8px;font-size:15px;letter-spacing:1px}
.grade{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(280px,1fr))}
.card{background:var(--card);border:1px solid var(--borda);border-radius:8px;padding:14px;min-width:0}
.linha{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:5px 0;border-bottom:1px solid var(--linha);font-size:14px}
.linha:last-child{border-bottom:0}
.linha>span:first-child{color:var(--mudo)}
.linha>span:last-child{text-align:right;overflow-wrap:anywhere}
.pill{display:inline-block;padding:1px 9px;border-radius:999px;font-size:12px;font-weight:600;border:1px solid var(--borda);color:var(--mudo);white-space:nowrap}
.pill.ok{color:var(--ok);border-color:var(--okBorda)}
.pill.aviso{color:var(--ouroTexto);border-color:var(--ouroBorda)}
.pill.perigo{color:var(--perigo);border-color:var(--perigoBorda)}
.vermelho{color:var(--perigo)}
.estado{font-size:22px;font-weight:800;letter-spacing:1px;padding:12px;border-radius:6px;text-align:center;margin:4px 0 8px}
.est-fechada{background:var(--infoFundo);color:var(--info)}
.est-liberada{background:var(--ouroFundo);color:var(--ouroTexto)}
.est-aberta{background:var(--laranjaFundo);color:var(--laranja)}
.est-alarme{background:var(--perigoFundo);color:var(--perigo);animation:pisca 1s infinite}
.est-off{background:var(--card2);color:var(--mudo)}
@keyframes pisca{50%{opacity:.55}}
button{font:inherit;cursor:pointer;border-radius:6px;padding:10px 16px;border:1px solid var(--ouro);background:var(--ouro);color:#111;font-weight:700;margin-top:12px}
button:hover{background:var(--ouro2)}
button.sec{background:transparent;color:var(--ouroTexto)}
button.sec:hover{background:var(--ouroFundo)}
button.perigo{background:transparent;border-color:var(--perigo);color:var(--perigo)}
button.perigo:hover{background:var(--perigoFundo)}
button.pequeno{margin:0 0 0 6px;padding:3px 9px;font-size:12px}
button:disabled{opacity:.5;cursor:not-allowed}
.abrir{width:100%;font-size:18px;padding:14px;letter-spacing:1px}
.res{font-size:13px;color:var(--mudo);min-height:1.4em;margin-top:8px}
.res.ok{color:var(--ok)}
.res.erro{color:var(--perigo)}
label{display:block;font-size:13px;color:var(--mudo);margin-top:10px}
input,select{display:block;width:100%;margin-top:4px;padding:9px 10px;background:var(--card2);border:1px solid var(--borda);border-radius:6px;color:var(--texto);font:inherit}
input:focus,select:focus{outline:none;border-color:var(--ouro)}
input:disabled{opacity:.5}
label.check{display:flex;align-items:center;gap:8px;margin-top:14px;color:var(--texto)}
label.check input{display:inline;width:auto;margin:0}
.dica{font-size:12px;color:var(--mudo);margin-top:6px}
.faixa{padding:10px 14px;border-radius:6px;margin-bottom:10px;font-size:14px}
.faixa.aviso{background:var(--ouroFundo);color:var(--ouroTexto);border:1px solid var(--ouroBorda)}
.faixa.perigo{background:var(--perigoFundo);color:var(--perigoTexto);border:1px solid var(--perigoBorda)}
.faixa a{color:inherit}
.registro{font-family:ui-monospace,Consolas,monospace;font-size:13px;max-height:340px;overflow:auto}
.registro div{padding:3px 0;border-bottom:1px solid var(--linha)}
.registro time{color:var(--mudo);margin-right:8px}
.tabela{overflow-x:auto}
table{width:100%;border-collapse:collapse;font-size:14px}
th,td{text-align:left;padding:6px 8px;border-bottom:1px solid var(--borda);white-space:nowrap}
th{color:var(--mudo);font-weight:500;font-size:12px;text-transform:uppercase;letter-spacing:1px}
#listaRedes{display:flex;flex-direction:column;gap:4px;margin-top:6px}
#listaRedes button{margin:0;padding:6px 10px;text-align:left;background:var(--card2);border-color:var(--borda);color:var(--texto);font-weight:500}
#listaRedes small{color:var(--mudo)}
progress{width:100%;margin-top:10px;accent-color:var(--ouro)}
a{color:var(--info)}
.login{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
.login form{width:100%;max-width:340px;padding:24px;box-shadow:0 8px 30px var(--sombra)}
.login .logo{height:46px;margin:0 auto 6px}
.login h3{text-align:center;color:var(--mudo);font-weight:600;font-size:12px;letter-spacing:3px;text-transform:uppercase;margin-bottom:12px}
.login button{width:100%}
.toast{position:fixed;left:50%;bottom:20px;transform:translate(-50%,200%);background:var(--card2);border:1px solid var(--borda);box-shadow:0 4px 16px var(--sombra);padding:10px 16px;border-radius:8px;transition:transform .25s;max-width:calc(100vw - 32px);z-index:10}
.toast.mostra{transform:translate(-50%,0)}
.toast.ok{border-color:var(--ok)}
.toast.erro{border-color:var(--perigo)}
)CSS";

static const char PAGINA_CABECALHO[] = R"HTML(</style></head><body>
<div id="login" class="login" hidden>
<form class="card" id="formLogin">
<img class="logo logo-claro" src="/logo-claro.png?v=)HTML" FW_VERSAO R"HTML(" alt="Wires">
<img class="logo logo-escuro" src="/logo-escuro.png?v=)HTML" FW_VERSAO R"HTML(" alt="Wires">
<h3 id="loginTitulo">Interlock</h3>
<label>Usuário<input name="usuario" autocomplete="username" autocapitalize="none" spellcheck="false" required></label>
<label>Senha<input name="senha" type="password" autocomplete="current-password" required></label>
<button>Entrar</button>
<div class="res erro" id="loginErro"></div>
</form>
</div>
<header>
<div class="marca">
<img class="logo logo-claro" src="/logo-claro.png?v=)HTML" FW_VERSAO R"HTML(" alt="Wires">
<img class="logo logo-escuro" src="/logo-escuro.png?v=)HTML" FW_VERSAO R"HTML(" alt="Wires">
<span>Interlock</span>
</div>
<b id="nomePlaca"></b>
<div class="dir">
<span id="topo"></span>
<span class="quem" id="quem"></span>
<button type="button" class="icone" id="btnTema" title="Tema claro ou escuro">◐ Tema</button>
<button type="button" class="icone" id="btnReiniciar" title="Reiniciar esta placa">⟳ Reiniciar</button>
<button type="button" class="icone" id="btnSair">Sair</button>
</div>
</header>
<main>
<div id="offline" class="faixa perigo" hidden>Sem resposta da placa. Tentando de novo...</div>
<div id="faixas"></div>
)HTML";

static const char SECOES_COMUNS[] = R"HTML(
<h2>Placas do sistema</h2>
<div class="card tabela"><table>
<thead><tr><th>Placa</th><th>Situação</th><th>IP</th><th>Sinal</th><th>Firmware</th><th>Ligada há</th></tr></thead>
<tbody id="pares"></tbody></table></div>

<h2>Registro</h2>
<div class="card registro" id="registro"></div>

<h2>Rede</h2>
<div class="grade">
<div class="card"><h3>Agora</h3><div id="redeAtual"></div></div>
<form class="card so-admin" id="formRede" autocomplete="off"><h3>Configurar</h3>
<label>Nome do Wi-Fi<input name="ssid" maxlength="32" required></label>
<button type="button" class="sec" id="btnProcurar">Procurar redes</button>
<div id="listaRedes"></div>
<label>Senha do Wi-Fi<input name="senha" type="password" maxlength="63" placeholder="em branco = manter a atual"></label>
<div class="dica">Ao trocar o nome do Wi-Fi, deixe a senha em branco só se a rede for aberta.</div>
<label class="check"><input type="checkbox" name="dhcp"> IP automático (DHCP)</label>
<div id="ipFixo">
<label>IP<input name="ip" required placeholder="192.168.0.50"></label>
<label>Máscara<input name="mascara" required placeholder="255.255.255.0"></label>
<label>Gateway<input name="gateway" required placeholder="192.168.0.1"></label>
<label>DNS (opcional)<input name="dns" placeholder="em branco = o gateway"></label>
</div>
<label>Grupo do intertravamento<input name="grupo" type="number" min="1" max="99" required></label>
<div class="dica">Só mude o grupo se houver mais de um intertravamento na mesma rede. As 3 placas de um conjunto precisam do mesmo grupo.</div>
<button>Salvar e reiniciar</button>
</form>
</div>

<h2>Manutenção</h2>
<div class="grade">
<div class="card"><h3>Placa</h3><div id="sistema"></div></div>
<form class="card so-admin" id="formFirmware"><h3>Firmware</h3>
<label>Arquivo firmware.bin da pasta .pio/build/<span id="envNome"></span><input type="file" name="firmware" accept=".bin" required></label>
<progress id="progresso" max="100" value="0" hidden></progress>
<button>Enviar firmware</button>
</form>
</div>
</main>
<div class="toast" id="toast"></div>
)HTML";

static const char PAGINA_SCRIPT[] = "<script>\n";

static const char JS_COMUM[] = R"JS(
'use strict';
const $=s=>document.querySelector(s);
const esc=t=>String(t==null?'':t).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let E=null,falhas=0,redePreenchida=false,esperando=false;

function tempo(s){s=Math.max(0,Math.round(s));if(s<120)return s+' s';if(s<7200)return Math.round(s/60)+' min';if(s<172800)return Math.round(s/3600)+' h';return Math.round(s/86400)+' dias'}
function linha(rotulo,valor){return '<div class="linha"><span>'+esc(rotulo)+'</span><span>'+valor+'</span></div>'}
function pill(texto,classe){return '<span class="pill '+(classe||'')+'">'+esc(texto)+'</span>'}
function aviso(texto,erro){const t=$('#toast');t.textContent=texto;t.className='toast mostra '+(erro?'erro':'ok');clearTimeout(aviso.t);aviso.t=setTimeout(()=>t.classList.remove('mostra'),5000)}

async function api(url,dados){
  const r=await fetch(url,{method:'POST',body:new URLSearchParams(dados||{})});
  let j={};try{j=await r.json()}catch(e){}
  if(r.status===401&&url!=='/api/entrar')mostrarLogin();
  if(!r.ok||j.ok===false)throw new Error(j.erro||('A placa respondeu '+r.status));
  return j}

function quando(ev){
  if(ev.hora)return new Date(ev.hora*1000).toLocaleString('pt-BR',{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit'});
  return 'há '+tempo(ev.ha)}

// Linhas de entradas/saídas: [{nome,pino,ativa,textoAtiva,textoInativa,ha,cont?,travada?}]
function listaES(itens){
  return itens.map(x=>linha(x.nome+' (GPIO'+x.pino+')',
    (x.travada?'<b class="vermelho">'+esc(x.textoAtiva)+' há '+tempo(x.ha)+': fio em curto ou contato preso?</b>':esc(x.ativa?x.textoAtiva:x.textoInativa))+
    (x.cont!=null?' · '+x.cont+'×':''))).join('')}

// Quadro grande de uma porta. Na portaria, a porta pode estar sem comunicação.
function mostrarSituacao(el,p){
  const [c,t]=p.comunicando===false?['est-off','SEM COMUNICAÇÃO']:
    p.alarme?['est-alarme','ABERTA HÁ '+tempo(p.abertaHa).toUpperCase()]:
    p.aberta?['est-aberta','ABERTA']:
    p.liberada?['est-liberada','LIBERADA · '+Math.ceil(p.restante/1000)+' s']:['est-fechada','FECHADA'];
  el.className='estado '+c;el.textContent=t}
function linhasPorta(p){
  return linha('Eletroímã',p.liberada?'solto · '+esc(p.origem)+(p.comBypass?' · bypass':''):'travando')+
    linha('Sensor',p.aberta?'aberta há '+tempo(p.abertaHa):'fechada')}
// Último pedido de abertura: {liberou,motivo,origem,ha, aguardando? (portaria), decidiu? (porta)}
function mostrarResultado(el,r){
  el.className='res'+(r&&!r.aguardando?(r.liberou?' ok':' erro'):'');
  el.textContent=!r?'':r.aguardando?'Liberada, esperando a porta confirmar...':
    [r.liberou?'Liberada':'Negada: '+r.motivo,r.origem,r.decidiu,'há '+tempo(r.ha)].filter(Boolean).join(' · ')}
// Botão que manda um comando: mostra a resposta e já consulta a placa de novo.
async function acao(url,dados){
  try{const j=await api(url,dados);aviso(j.msg);return j}
  catch(e){aviso(e.message,true)}
  finally{ciclo()}}

function desenharComum(e){
  document.title=e.nome+' · Interlock';
  document.body.classList.add('logado');
  document.body.classList.toggle('admin',e.conta.admin);
  $('#nomePlaca').textContent=e.nome;
  $('#topo').innerHTML=(e.wifi.conectado?pill(e.wifi.ip,'ok'):pill('sem Wi-Fi','perigo'))+(e.ap.ligado?pill('rede própria ligada','aviso'):'');
  $('#quem').innerHTML=esc(e.conta.nome)+(e.conta.admin?' '+pill('admin','aviso'):'');
  let f='';
  if(e.conta.admin&&e.senhaPadrao)f+='<div class="faixa perigo">A senha da rede própria e a chave entre as placas ainda são as de exemplo. Crie include/segredos.h com valores próprios e grave de novo as 3 placas.</div>';
  if(e.conta.senhaDeFabrica)f+='<div class="faixa aviso">A conta admin ainda usa a senha de fábrica. Troque em '+(e.papel==='PORTEIRO'?'Contas → Minha senha':'Contas, na página da portaria')+'.</div>';
  if(e.conflito)f+='<div class="faixa perigo">A placa '+esc(e.conflito)+' também foi gravada como '+esc(e.nome)+'. Cada placa precisa de um papel diferente.</div>';
  if(!e.cfgRede.ssid)f+='<div class="faixa aviso">Wi-Fi não configurado'+(e.conta.admin?': preencha a seção Rede.':'.')+'</div>';
  else if(!e.wifi.conectado)f+='<div class="faixa aviso">Sem conexão com o Wi-Fi "'+esc(e.cfgRede.ssid)+'". A placa continua tentando sozinha.</div>';
  $('#faixas').innerHTML=f;

  $('#pares').innerHTML=e.pares.map(x=>{
    const ok=x.eu||x.online;
    const ip=!x.ip?'-':x.eu?esc(x.ip):'<a href="http://'+esc(x.ip)+'/" target="_blank" rel="noopener">'+esc(x.ip)+'</a>';
    const sit=ok?pill('comunicando','ok'):pill(x.conhecido?'sem comunicação':'não encontrada','perigo');
    return '<tr><td>'+esc(x.nome)+(x.eu?' (esta)':'')+'</td><td>'+sit+'</td><td>'+ip+'</td><td>'+(ok&&x.rssi?x.rssi+' dBm':'-')+
      '</td><td>'+esc(x.fw||'-')+'</td><td>'+(ok?tempo(x.up):'-')+'</td></tr>'}).join('');

  $('#registro').innerHTML=e.registro.length?e.registro.map(ev=>'<div><time>'+esc(quando(ev))+'</time>'+esc(ev.m)+'</div>').join(''):'<div>Nada registrado ainda.</div>';

  const w=e.wifi,c=e.cfgRede;
  $('#redeAtual').innerHTML=
    linha('Wi-Fi',w.conectado?esc(w.ssid)+' · '+w.rssi+' dBm · canal '+w.canal:'desconectado')+
    linha('IP',w.conectado?esc(w.ip)+(c.dhcp?' (automático)':' (fixo)'):'-')+
    linha('Máscara',w.conectado?esc(w.mascara):'-')+
    linha('Gateway',w.conectado?esc(w.gateway):'-')+
    linha('DNS',w.conectado?esc(w.dns):'-')+
    linha('Nome na rede',esc(w.host)+'.local')+
    linha('MAC',esc(w.mac))+
    linha('Rede própria',e.ap.ligado?esc(e.ap.nome)+' · '+esc(e.ap.ip)+' · '+e.ap.clientes+' conectado(s)':'desligada')+
    linha('Grupo',esc(c.grupo));
  if(!redePreenchida&&e.conta.admin)preencherRede(e);

  $('#sistema').innerHTML=
    linha('Placa',esc(e.nome)+' ('+esc(e.papel)+')')+
    linha('Firmware',esc(e.fw)+' · '+esc(e.compilado))+
    linha('Ligada há',tempo(e.up))+
    linha('Último reinício',esc(e.reinicio))+
    linha('Memória livre',Math.round(e.heap/1024)+' KB')+
    linha('Mensagens recusadas',e.rejeitadas?'<b class="vermelho">'+e.rejeitadas+' (CHAVE_REDE diferente em alguma placa?)</b>':'0');
  $('#envNome').textContent=e.env}

function modoIp(){const f=$('#formRede'),d=f.dhcp.checked;f.querySelectorAll('#ipFixo input').forEach(i=>i.disabled=d)}

// No modo automático, os campos de IP fixo já vêm com o IP atual, para facilitar fixar a placa nele.
function preencherRede(e){
  const f=$('#formRede'),c=e.cfgRede,w=e.wifi,atual=c.dhcp&&w.conectado;
  f.ssid.value=c.ssid;f.dhcp.checked=c.dhcp;
  f.ip.value=atual?w.ip:c.ip;f.mascara.value=atual?w.mascara:c.mascara;f.gateway.value=atual?w.gateway:c.gateway;
  f.dns.value=c.dns;f.grupo.value=c.grupo;
  modoIp();redePreenchida=true}

function aguardarReinicio(novoIp){
  esperando=true;
  const o=$('#offline');o.hidden=false;
  if(novoIp&&novoIp!==location.hostname){
    o.innerHTML='Reiniciando a placa. Ela vai voltar em <a href="http://'+esc(novoIp)+'/">http://'+esc(novoIp)+'/</a>.';
    return}
  o.textContent='Reiniciando a placa. A página recarrega sozinha quando ela voltar.';
  // Depois de reiniciar a sessão acabou: a placa responde 401 e a página volta para o login.
  const tentar=()=>fetch('/api/estado',{cache:'no-store'}).then(r=>{if(r.ok||r.status===401)location.reload();else setTimeout(tentar,2000)}).catch(()=>setTimeout(tentar,2000));
  setTimeout(tentar,5000)}

function mostrarLogin(){
  esperando=false;clearTimeout(timer);
  document.body.classList.remove('logado','admin');
  if(!$('#login').hidden)return;
  $('#login').hidden=false;
  setTimeout(()=>$('#formLogin').usuario.focus(),50)}

// Consulta a placa a cada segundo (300 ms enquanto espera uma resposta). Chamada fora de hora
// (depois de um botão), consulta já: se uma consulta estiver em andamento, faz outra logo depois.
let timer=null,buscando=false,deNovo=false;
async function ciclo(){
  if(esperando)return;
  if(buscando){deNovo=true;return}
  clearTimeout(timer);buscando=true;
  try{
    const r=await fetch('/api/estado',{cache:'no-store'});
    if(r.status===401){buscando=false;deNovo=false;mostrarLogin();return}
    if(!r.ok)throw new Error();
    E=await r.json();falhas=0;$('#offline').hidden=true;$('#login').hidden=true;
    desenharComum(E);desenhar(E)}
  catch(e){if(++falhas>=3)$('#offline').hidden=false}
  buscando=false;
  timer=setTimeout(ciclo,deNovo?0:rapido()?300:1000);deNovo=false}

$('#formLogin').onsubmit=async ev=>{
  ev.preventDefault();
  const f=ev.target,erro=$('#loginErro');erro.textContent='';
  try{await api('/api/entrar',new FormData(f));f.senha.value='';redePreenchida=false;opPreenchido=false;ciclo()}
  catch(e){erro.textContent=e.message;f.senha.select()}};
$('#btnSair').onclick=async()=>{try{await api('/api/sair')}catch(e){}location.reload()};
$('#btnTema').onclick=()=>{
  const tema=document.documentElement.dataset.tema==='claro'?'escuro':'claro';
  document.documentElement.dataset.tema=tema;
  try{localStorage.setItem('tema',tema)}catch(e){}};
$('#btnReiniciar').onclick=async()=>{
  if(!E||!confirm('Reiniciar '+E.nome+' agora? Durante o reinício (uns 10 s) a porta fica travada.'))return;
  try{const j=await api('/api/reiniciar');aviso(j.msg);aguardarReinicio()}catch(e){aviso(e.message,true)}};

$('#formRede').dhcp.onchange=modoIp;
$('#formRede').onsubmit=async ev=>{
  ev.preventDefault();
  if(!E||!confirm('Salvar a rede e reiniciar '+E.nome+'? Se o Wi-Fi não conectar, a rede própria da placa ('+E.ap.nome+') volta sozinha em 30 s. Se o IP fixo estiver errado, segure o botão BOOT da placa por 10 s e solte.'))return;
  try{const j=await api('/api/rede',new FormData(ev.target));aviso(j.msg);aguardarReinicio(j.novoIp)}
  catch(e){aviso(e.message,true)}};

$('#btnProcurar').onclick=async()=>{
  const b=$('#btnProcurar'),l=$('#listaRedes');
  b.disabled=true;b.textContent='Procurando (a comunicação para por uns 2 s)...';l.innerHTML='';
  try{
    let url='/api/redes?novo=1';
    for(let i=0;i<15;i++){
      const j=await (await fetch(url,{cache:'no-store'})).json();url='/api/redes';
      if(j.redes){
        l.innerHTML=j.redes.length?j.redes.map(x=>'<button type="button" data-ssid="'+esc(x.ssid)+'">'+esc(x.ssid)+' <small>'+x.rssi+' dBm'+(x.aberta?' · sem senha':'')+'</small></button>').join(''):'<div class="dica">Nenhuma rede encontrada.</div>';
        return}
      await new Promise(ok=>setTimeout(ok,1000))}
    aviso('A procura não terminou. Tente de novo.',true)}
  catch(e){aviso('Falha ao procurar redes',true)}
  finally{b.disabled=false;b.textContent='Procurar redes'}};
$('#listaRedes').onclick=ev=>{const b=ev.target.closest('button[data-ssid]');if(!b)return;const f=$('#formRede');f.ssid.value=b.dataset.ssid;f.senha.focus()};

$('#formFirmware').onsubmit=ev=>{
  ev.preventDefault();
  const arq=ev.target.firmware.files[0];
  if(!arq||!E||!confirm('Gravar "'+arq.name+'" em '+E.nome+'? Durante a gravação a placa fica parada (porta travada, buzzers desligados) e no fim reinicia.'))return;
  const x=new XMLHttpRequest(),fd=new FormData(),p=$('#progresso');
  fd.append('firmware',arq,arq.name);p.hidden=false;p.value=0;
  x.upload.onprogress=e=>{if(e.lengthComputable)p.value=100*e.loaded/e.total};
  x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
    if(x.status===200&&j.ok){aviso(j.msg);aguardarReinicio()}else{p.hidden=true;aviso(j.erro||('Falhou ('+x.status+')'),true)}};
  x.onerror=()=>{p.hidden=true;aviso('A conexão caiu durante o envio.',true)};
  x.open('POST','/api/atualizar');x.send(fd)};

// Formulário "Funcionamento" (cada página preenche os campos dela em desenhar()).
let opPreenchido=false;
$('#formOp').onsubmit=async ev=>{ev.preventDefault();if(await acao('/api/operacao',new FormData(ev.target)))opPreenchido=false};
)JS";

static const char PAGINA_FIM[] = "\n</script></body></html>\n";
