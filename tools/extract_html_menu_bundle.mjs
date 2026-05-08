import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const root = process.argv[2] ? path.resolve(process.argv[2]) : process.cwd();
const sourcePath = path.join(root, 'assets/ui/main_menu/void_echo_menu_source.html');
const outDir = path.join(root, 'assets/ui/main_menu/web');
const assetDir = path.join(outDir, 'assets');

const source = fs.readFileSync(sourcePath, 'utf8');
const manifestMatch = source.match(/<script\s+type=["']__bundler\/manifest["'][^>]*>([\s\S]*?)<\/script>/i);
const templateMatch = source.match(/<script\s+type=["']__bundler\/template["'][^>]*>([\s\S]*?)<\/script>/i);

if (!manifestMatch || !templateMatch) {
  throw new Error('Bundler manifest/template not found');
}

const manifest = JSON.parse(manifestMatch[1].trim());
let template = JSON.parse(templateMatch[1].trim());

fs.mkdirSync(assetDir, { recursive: true });

const extByMime = {
  'font/woff2': '.woff2',
  'font/woff': '.woff',
  'image/png': '.png',
  'image/jpeg': '.jpg',
  'image/svg+xml': '.svg',
  'text/css': '.css',
  'application/javascript': '.js',
};

let extractedAssets = 0;
for (const [uuid, entry] of Object.entries(manifest)) {
  let bytes = Buffer.from(entry.data, 'base64');
  if (entry.compressed) {
    bytes = zlib.gunzipSync(bytes);
  }

  const extension = extByMime[entry.mime] || '.bin';
  const fileName = `${uuid}${extension}`;
  fs.writeFileSync(path.join(assetDir, fileName), bytes);
  template = template.split(uuid).join(`assets/${fileName}`);
  extractedAssets += 1;
}

const bridgeScript = `
<script>
  window.exoMenuBridge = window.exoMenuBridge || {};
  window.exoMenuBridge.startGame = window.exoMenuBridge.startGame || function(){ window.location.href = 'exo://start-game'; };
  window.exoMenuBridge.quitGame = window.exoMenuBridge.quitGame || function(){ window.location.href = 'exo://quit-game'; };
</script>
`;

template = template.replace(/<\/head>/i, `${bridgeScript}</head>`);
template = template.replace("if (a === 'story')    go('screen-story');", "if (a === 'story')    beginGame();");
template = template.replace(
  /function beginGame\(\)\{([\s\S]*?)setTimeout\(\(\)=>\{([\s\S]*?)trans\.classList\.remove\('on'\);\s*\}, 1200\);\s*\}/,
  (_match, before, inside) => `function beginGame(){${before}setTimeout(()=>{${inside}trans.classList.remove('on');
      window.exoMenuBridge.startGame();
    }, 1200);
  }`,
);
template = template.replace(
  /function quitFx\(\)\{([\s\S]*?)setTimeout\(\(\)=>\{([\s\S]*?)\}, 1400\);\s*\}/,
  (_match, before, inside) => `function quitFx(){${before}setTimeout(()=>{${inside}
      window.exoMenuBridge.quitGame();
    }, 1400);
  }`,
);

for (const [from, to] of [
  ['РђРљРў I', 'АКТ I'],
  ['РЇ РџРћРњРќР® Р’РЎРЃ', 'НУЛЕВОЙ ПАЦИЕНТ'],
  ['РєР»РёРЅРёРєР° РІР°Р№СЃС…Р°СѓРїС‚ В· 23:51 В· 12.10.1994', 'офис Антона · ноябрь · третий этаж'],
  ['[ Р·Р°РіСЂСѓР·РєР° ... ]', '[ загрузка 3D сцены ... ]'],
  ['Я ПОМНЮ ВСЁ', 'НУЛЕВОЙ ПАЦИЕНТ'],
  ['клиника вайсхаупт · 23:51 · 12.10.1994', 'офис Антона · ноябрь · третий этаж'],
  ['[ загрузка ... ]', '[ загрузка 3D сцены ... ]'],
  ['вЂ” NO SIGNAL вЂ”', '— NO SIGNAL —'],
  ["'в– '.repeat(n) + 'в–Ў'.repeat(7-n)", "'■'.repeat(n) + '□'.repeat(7-n)"],
]) {
  template = template.split(from).join(to);
}

fs.mkdirSync(outDir, { recursive: true });
fs.writeFileSync(path.join(outDir, 'index.html'), template, 'utf8');

console.log(`Extracted ${extractedAssets} assets to ${outDir}`);
