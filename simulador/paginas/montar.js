// Monta as páginas a partir dos .h do firmware (as mesmas partes, na mesma ordem de web_*.cpp)
// e confere a sintaxe do JavaScript. Gera porta.html e portaria.html nesta pasta, usadas por
// servidor.js para ver as páginas no navegador sem placa nenhuma.
// Uso (na pasta do projeto):  node simulador/paginas/montar.js
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const src = process.argv[2] || path.join(__dirname, '..', '..', 'src');
const partes = {};
for (const arq of ['pagina_comum.h', 'pagina_porta.h', 'pagina_porteiro.h']) {
  const texto = fs.readFileSync(path.join(src, arq), 'utf8');
  const re = /static const char (\w+)\[\] = (?:R"(\w*)\(([\s\S]*?)\)\2"|"((?:[^"\\]|\\.)*)");/g;
  let m;
  while ((m = re.exec(texto))) partes[m[1]] = m[3] !== undefined ? m[3] : JSON.parse('"' + m[4] + '"');
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
  const js = html.match(/<script>([\s\S]*)<\/script>/)[1];
  new vm.Script(js, {filename: nome + '.js'});  // lança erro se a sintaxe estiver errada
  fs.writeFileSync(path.join(__dirname, nome + '.html'), html);
  console.log(`${nome}.html: ${html.length} bytes, JavaScript ok`);
}
