// Servidor de teste das páginas: 8471 = portaria, 8472 = porta A. As páginas são as montadas por
// montar.js a partir dos .h do firmware; a API imita o JSON do firmware com um estado simulado.
// Contas de teste: admin / interlock123 (admin) e joao / 123456 (usuário).
const http = require('http');
const fs = require('fs');
const path = require('path');

const inicio = Date.now();
const seg = () => Math.floor((Date.now() - inicio) / 1000);
const registro = [];
const reg = m => registro.unshift({ha: 0, hora: Math.floor(Date.now() / 1000), m, t: Date.now()});

const portas = {
  A: {liberadaAte: 0, abertaDesde: 0, origem: 'facial', resultado: null},
  B: {liberadaAte: 0, abertaDesde: Date.now() - 75000, origem: 'botão', resultado: null},  // em alarme
};
let bypassVirtual = false;
// Contas de teste: admin / interlock123 (admin) e joao / 123456 (usuário)
const contas = {admin: {senha: 'interlock123', admin: true}, joao: {senha: '123456', admin: false}};
const sessoes = {};  // token -> nome
function contaDoPedido(req) {
  const m = /(?:^|;\s*)sessao=([0-9a-f]+)/.exec(req.headers.cookie || '');
  const nome = m && sessoes[m[1]];
  return nome && contas[nome] ? {nome, admin: contas[nome].admin} : null;
}
// Logo: os mesmos PNG embutidos no firmware (src/logo.h)
const logoH = fs.readFileSync(path.join(__dirname, '..', '..', 'src', 'logo.h'), 'utf8');
const logo = nome => Buffer.from(logoH.match(new RegExp(nome + '\\[\\] = \\{([^}]*)\\}'))[1].split(',').map(s => s.trim()).filter(Boolean).map(s => parseInt(s, 16)));
const LOGOS = {'/logo-claro.png': logo('LOGO_CLARO'), '/logo-escuro.png': logo('LOGO_ESCURO')};
reg('Portaria ligou. Firmware 7.1.0. Último reinício: ligou na energia');
reg('Porta A encontrada na rede (192.168.0.51)');
reg('Porta B encontrada na rede (192.168.0.52)');
reg('ALARME: Porta B aberta há muito tempo (buzzer ligado)');

function comum(papel, nome, env, conta) {
  const lista = Object.entries(contas).map(([n, c]) => ({nome: n, admin: c.admin}));
  return {
    conta: {nome: conta.nome, admin: conta.admin, senhaDeFabrica: conta.admin && contas.admin.senha === 'interlock123'},
    contas: papel === 'PORTEIRO'
      ? {daPortaria: false, total: lista.length, max: 10, ...(conta.admin ? {lista} : {})}
      : {daPortaria: true, total: lista.length, max: 10, atualizada: true, portariaConhecida: true, ...(conta.admin ? {lista} : {})},
    papel, nome, env, fw: '7.1.0', compilado: 'Oct 13 2026 13:30:00', up: seg() + 3600, reinicio: 'ligou na energia',
    heap: 210000, senhaPadrao: true, conflito: '', rejeitadas: 0,
    wifi: {conectado: true, ssid: 'Condominio', ip: env === 'porteiro' ? '192.168.0.50' : '192.168.0.51',
           mascara: '255.255.255.0', gateway: '192.168.0.1', dns: '192.168.0.1', rssi: -58, canal: 6,
           mac: '24:6F:28:AA:BB:CC', host: env === 'porteiro' ? 'interlock-portaria' : 'interlock-porta-a'},
    ap: {ligado: true, nome: env === 'porteiro' ? 'Interlock-Portaria' : 'Interlock-PortaA', ip: '192.168.4.1', clientes: 1},
    cfgRede: {ssid: 'Condominio', dhcp: true, ip: '', mascara: '', gateway: '', dns: '', grupo: 1},
    pares: [
      {nome: 'Portaria', eu: papel === 'PORTEIRO', online: true, conhecido: true, ip: '192.168.0.50', rssi: -58, fw: '7.0.0', up: 3700},
      {nome: 'Porta A', eu: papel === 'PORTA_A', online: true, conhecido: true, ip: '192.168.0.51', rssi: -61, fw: '7.0.0', up: 3690},
      {nome: 'Porta B', eu: false, online: false, conhecido: true, ip: '192.168.0.52', rssi: -70, fw: '7.0.0', up: 0},
    ],
    registro: registro.map(e => ({ha: Math.floor((Date.now() - e.t) / 1000), hora: e.hora, m: e.m})),
  };
}

function estadoPorta(L) {
  const p = portas[L], agora = Date.now();
  const liberada = p.liberadaAte > agora;
  const aberta = p.abertaDesde > 0;
  return {liberada, aberta, restante: liberada ? p.liberadaAte - agora : 0,
          abertaHa: aberta ? Math.floor((agora - p.abertaDesde) / 1000) : 0, alarme: aberta && agora - p.abertaDesde > 60000,
          origem: p.origem};
}

function jsonPortaria(conta) {
  const e = comum('PORTEIRO', 'Portaria', 'porteiro', conta);
  const portasJson = ['A', 'B'].map(L => {
    const s = estadoPorta(L), o = estadoPorta(L === 'A' ? 'B' : 'A');
    const bloqueio = bypassVirtual ? '' : o.liberada ? 'a outra porta está destravada' : o.aberta ? 'a outra porta está aberta' : '';
    return {letra: L, nome: 'Porta ' + L, comunicando: true, ...s, modo: 'C', comBypass: false, buzzer: s.alarme,
            podeAbrir: !bloqueio, permissao: bloqueio || (bypassVirtual ? 'pode abrir (bypass)' : 'pode abrir'),
            resultado: portas[L].resultado ? {...portas[L].resultado, ha: Math.floor((Date.now() - portas[L].resultado.t) / 1000)} : null};
  });
  e.portaria = {
    bypass: bypassVirtual, chave: false, virtual: bypassVirtual, portas: portasJson,
    entradas: [
      {nome: 'Botão da porta A', pino: 32, ativa: false, textoAtiva: 'apertado', textoInativa: 'solto', ha: 120, cont: 14, travada: false},
      {nome: 'Botão da porta B', pino: 33, ativa: true, textoAtiva: 'apertado', textoInativa: 'solto', ha: 45, cont: 3, travada: true},
      {nome: 'Chave do bypass', pino: 25, ativa: false, textoAtiva: 'ligada', textoInativa: 'desligada', ha: 3000, cont: 1, travada: false},
    ],
    saidas: [
      {nome: 'Buzzer da porta A', pino: 26, ativa: portasJson[0].buzzer, textoAtiva: 'tocando', textoInativa: 'desligado'},
      {nome: 'Buzzer da porta B', pino: 27, ativa: portasJson[1].buzzer, textoAtiva: 'tocando', textoInativa: 'desligado'},
    ],
    cfg: {liberarSemComunicacao: true},
  };
  return e;
}

let pedidoAte = 0;
const cfgPorta = {tempoAbertura: 5, tempoAlarme: 60, liberarSemComunicacao: true};
function jsonPorta(conta) {
  const e = comum('PORTA_A', 'Porta A', 'porta_a', conta);
  const s = estadoPorta('A'), o = estadoPorta('B');
  e.porta = {
    ...s, comBypass: false, sinaleira: s.liberada, modo: 'C', pedido: pedidoAte > Date.now(), portaria: true, bypass: bypassVirtual,
    outra: {nome: 'Porta B', comunicando: true, liberada: o.liberada, aberta: o.aberta},
    resultado: portas.A.resultado ? {liberou: portas.A.resultado.liberou, motivo: portas.A.resultado.motivo, origem: portas.A.resultado.origem,
              decidiu: 'pela portaria', ha: Math.floor((Date.now() - portas.A.resultado.t) / 1000)} : null,
    entradas: [
      {nome: 'Sensor da porta', pino: 32, ativa: s.aberta, textoAtiva: 'aberta', textoInativa: 'fechada', ha: 30, cont: 120, travada: false},
      {nome: 'Facial', pino: 33, ativa: false, textoAtiva: 'acionado', textoInativa: 'em repouso', ha: 30, cont: 87, travada: false},
      {nome: 'Botão', pino: 25, ativa: false, textoAtiva: 'apertado', textoInativa: 'solto', ha: 300, cont: 33, travada: false},
    ],
    saidas: [
      {nome: 'Relé do eletroímã', pino: 26, ativa: s.liberada, textoAtiva: 'ligado (ímã solto)', textoInativa: 'desligado (ímã travando)'},
      {nome: 'Relé da sinaleira', pino: 27, ativa: s.liberada, textoAtiva: 'ligado (acesa)', textoInativa: 'desligado (apagada)'},
    ],
    cfg: cfgPorta,
  };
  return e;
}

function abrir(L, origem) {
  const o = estadoPorta(L === 'A' ? 'B' : 'A');
  const motivo = bypassVirtual ? '' : o.liberada ? 'a outra porta está destravada' : o.aberta ? 'a outra porta está aberta' : '';
  portas[L].resultado = {liberou: !motivo, aguardando: false, motivo: motivo || 'liberada', origem, t: Date.now()};
  if (motivo) {
    reg(`Porta ${L}: abertura negada (${origem}): ${motivo}`);
    return motivo;
  }
  portas[L].liberadaAte = Date.now() + 5000;
  portas[L].origem = origem;
  reg(`Porta ${L} liberada (${origem})`);
  return '';
}

function corpo(req) {
  return new Promise(ok => {
    let d = '';
    req.on('data', c => (d += c));
    req.on('end', () => ok(new URLSearchParams(d)));
  });
}

function servir(porta, pagina, json) {
  http.createServer(async (req, res) => {
    const url = req.url.split('?')[0];
    const enviar = (codigo, obj) => {
      res.writeHead(codigo, {'Content-Type': 'application/json'});
      res.end(JSON.stringify(obj));
    };
    if (url === '/') {
      res.writeHead(200, {'Content-Type': 'text/html; charset=utf-8'});
      return res.end(fs.readFileSync(path.join(__dirname, pagina)));
    }
    if (LOGOS[url]) {
      res.writeHead(200, {'Content-Type': 'image/png'});
      return res.end(LOGOS[url]);
    }
    if (url === '/api/entrar') {
      const a = await corpo(req);
      const nome = (a.get('usuario') || '').trim().toLowerCase();
      if (!contas[nome] || contas[nome].senha !== a.get('senha')) return enviar(401, {ok: false, erro: 'Usuário ou senha errados.'});
      const token = require('crypto').randomBytes(16).toString('hex');
      sessoes[token] = nome;
      reg('Entrou na página: ' + nome);
      res.setHeader('Set-Cookie', `sessao=${token}; Path=/; HttpOnly; SameSite=Strict`);
      return enviar(200, {ok: true, msg: 'Olá, ' + nome + '!'});
    }
    const conta = contaDoPedido(req);
    if (url === '/api/sair') {
      res.setHeader('Set-Cookie', 'sessao=; Path=/; Max-Age=0');
      return enviar(200, {ok: true, msg: 'Até logo.'});
    }
    if (url.startsWith('/api/') && !conta) return enviar(401, {ok: false, erro: 'Entre com seu usuário e senha.'});
    const soAdmin = ['/api/rede', '/api/redes', '/api/atualizar', '/api/contas/criar', '/api/contas/apagar', '/api/contas/senha'];
    if (soAdmin.includes(url) && !conta.admin) return enviar(403, {ok: false, erro: 'Só o admin pode fazer isso.'});
    if (url === '/api/estado') {
      enviar(200, json(conta));
    } else if (url === '/api/contas/criar') {
      const a = await corpo(req);
      const nome = (a.get('nome') || '').trim().toLowerCase();
      if (contas[nome]) return enviar(400, {ok: false, erro: 'Já existe uma conta com esse nome.'});
      if ((a.get('senha') || '').length < 6) return enviar(400, {ok: false, erro: 'A senha precisa ter de 6 a 63 caracteres.'});
      contas[nome] = {senha: a.get('senha'), admin: false};
      reg('Conta criada: ' + nome);
      enviar(200, {ok: true, msg: `Conta "${nome}" criada.`});
    } else if (url === '/api/contas/apagar') {
      const a = await corpo(req);
      delete contas[a.get('nome')];
      enviar(200, {ok: true, msg: `Conta "${a.get('nome')}" apagada.`});
    } else if (url === '/api/contas/senha') {
      const a = await corpo(req);
      contas[a.get('nome')].senha = a.get('senha');
      enviar(200, {ok: true, msg: `Senha de "${a.get('nome')}" trocada.`});
    } else if (url === '/api/minha-senha') {
      const a = await corpo(req);
      if (contas[conta.nome].senha !== a.get('atual')) return enviar(400, {ok: false, erro: 'A senha atual não confere.'});
      contas[conta.nome].senha = a.get('nova');
      enviar(200, {ok: true, msg: 'Sua senha foi trocada.'});
    } else if (url === '/api/reiniciar') {
      enviar(200, {ok: true, msg: 'A placa vai reiniciar.'});
    } else if (url === '/api/abrir') {
      const a = await corpo(req);
      if (pagina === 'portaria.html') {
        const L = a.get('porta');
        const m = abrir(L, 'página da portaria');
        return m ? enviar(400, {ok: false, erro: `Porta ${L}: abertura negada, ${m}`}) : enviar(200, {ok: true, msg: `Porta ${L}: liberada, esperando a porta confirmar`});
      }
      pedidoAte = Date.now() + 400;
      setTimeout(() => abrir('A', 'página da porta'), 400);
      enviar(200, {ok: true, msg: 'Pedido enviado à portaria'});
    } else if (url === '/api/bypass') {
      const a = await corpo(req);
      bypassVirtual = a.get('ligar') === '1';
      reg('Bypass pela página ' + (bypassVirtual ? 'LIGADO' : 'desligado'));
      enviar(200, {ok: true, msg: bypassVirtual ? 'Bypass da página ligado' : 'Bypass da página desligado'});
    } else if (url === '/api/operacao') {
      const a = await corpo(req);
      if (a.has('tempoAbertura')) {
        cfgPorta.tempoAbertura = +a.get('tempoAbertura');
        cfgPorta.tempoAlarme = +a.get('tempoAlarme');
      }
      enviar(200, {ok: true, msg: 'Salvo. Já está valendo.'});
    } else if (url === '/api/redes') {
      enviar(200, {redes: [{ssid: 'Condominio', rssi: -55, aberta: false}, {ssid: 'Visitantes', rssi: -71, aberta: true}]});
    } else if (url === '/api/rede') {
      enviar(400, {ok: false, erro: 'O IP tem que estar na mesma faixa do gateway e ser diferente dele (ex.: gateway 192.168.0.1, placa 192.168.0.50). Nada foi salvo.'});
    } else {
      res.writeHead(204);
      res.end();
    }
  }).listen(porta);
}

servir(8471, 'portaria.html', jsonPortaria);
servir(8472, 'porta.html', jsonPorta);
console.log('páginas de teste em http://localhost:8471 (portaria) e http://localhost:8472 (porta A)');
