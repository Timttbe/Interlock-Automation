// Monta as páginas a partir dos .h do firmware (as mesmas partes, na mesma ordem de web_*.cpp)
// e confere a sintaxe do JavaScript. Gera porta.html e portaria.html nesta pasta, usadas por
// servidor.js para ver as páginas no navegador sem placa nenhuma.
// Uso (na pasta do projeto):  node simulador/paginas/montar.js
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const raiz = path.join(__dirname, '..', '..');
const src = process.argv[2] || path.join(raiz, 'src');
const versao = fs.readFileSync(path.join(raiz, 'include', 'config.h'), 'utf8').match(/#define FW_VERSAO "([^"]+)"/)[1];

// Lê a expressão depois de "=" até o ";": textos R"x(...)x", "..." e a macro FW_VERSAO, juntos.
function lerExpressao(texto, inicio) {
  let i = inicio, valor = '';
  for (;;) {
    while (/\s/.test(texto[i])) i++;
    if (texto[i] === ';') return valor;
    if (texto.startsWith('R"', i)) {
      const abre = texto.indexOf('(', i);
      const delim = ')' + texto.slice(i + 2, abre) + '"';
      const fecha = texto.indexOf(delim, abre);
      valor += texto.slice(abre + 1, fecha);
      i = fecha + delim.length;
    } else if (texto[i] === '"') {
      let j = i + 1;
      while (texto[j] !== '"') j += texto[j] === '\\' ? 2 : 1;
      valor += JSON.parse(texto.slice(i, j + 1));
      i = j + 1;
    } else if (texto.startsWith('FW_VERSAO', i)) {
      valor += versao;
      i += 'FW_VERSAO'.length;
    } else {
      throw new Error('não entendi o texto perto de: ' + texto.slice(i, i + 40));
    }
  }
}

const partes = {};
for (const arq of ['pagina_comum.h', 'pagina_porta.h', 'pagina_porteiro.h']) {
  const texto = fs.readFileSync(path.join(src, arq), 'utf8');
  const re = /static const char (\w+)\[\] = /g;
  let m;
  while ((m = re.exec(texto))) partes[m[1]] = lerExpressao(texto, re.lastIndex);
}
const pagina = (corpo, js) =>
  ['PAGINA_INICIO', 'CSS_COMUM', 'PAGINA_CABECALHO', corpo, 'SECOES_COMUNS', 'PAGINA_SCRIPT', 'JS_COMUM', js, 'PAGINA_FIM']
    .map(n => {
      if (partes[n] === undefined) throw new Error('parte não encontrada: ' + n);
      return partes[n];
    })
    .join('');

const paginas = {porta: pagina('CORPO_PORTA', 'JS_PORTA'), portaria: pagina('CORPO_PORTEIRO', 'JS_PORTEIRO')};
for (const [nome, html] of Object.entries(paginas)) {
  for (const script of html.match(/<script>[\s\S]*?<\/script>/g)) {
    new vm.Script(script.slice(8, -9), {filename: nome + '.js'});  // lança erro se a sintaxe estiver errada
  }
  fs.writeFileSync(path.join(__dirname, nome + '.html'), html);
  console.log(`${nome}.html: ${Buffer.byteLength(html)} bytes, JavaScript ok`);
}
