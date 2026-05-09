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
  (function(){
    function exoCommand(command){
      var nonce = Date.now().toString(36) + Math.random().toString(36).slice(2);
      var payload = 'exo:' + command + ':' + nonce;
      try { document.title = payload; } catch (err) {}
      try { window.location.href = 'exo://' + command + '?nonce=' + nonce; } catch (err) {}
    }

    window.exoMenuBridge = window.exoMenuBridge || {};
    window.exoMenuBridge.startGame = window.exoMenuBridge.startGame || function(){ exoCommand('start-game'); };
    window.exoMenuBridge.quitGame = window.exoMenuBridge.quitGame || function(){ exoCommand('quit-game'); };
  })();
</script>
`;

template = template.replace(/<\/head>/i, `${bridgeScript}</head>`);

template = template
  .replace(/<link rel="preconnect" href="https:\/\/fonts\.googleapis\.com"[^>]*>\s*/gi, '')
  .replace(/<link rel="preconnect" href="https:\/\/fonts\.gstatic\.com"[^>]*>\s*/gi, '')
  .replace('position: fixed; inset: -50%;', 'position: fixed; left: -50%; top: -50%; right: -50%; bottom: -50%;')
  .replaceAll('position: fixed; inset: 0;', 'position: fixed; left: 0; top: 0; right: 0; bottom: 0;')
  .replace('position: absolute; inset: -10%;', 'position: absolute; left: -10%; top: -10%; right: -10%; bottom: -10%;')
  .replaceAll('position: absolute; inset: 0;', 'position: absolute; left: 0; top: 0; right: 0; bottom: 0;')
  .replaceAll('position:fixed;inset:0;', 'position:fixed;left:0;top:0;right:0;bottom:0;')
  .replace('font-size: clamp(56px, 9vw, 104px);', 'font-size: 92px;');

template = template.replace(
  /(@font-face\s*\{[^}]*font-family:\s*'Special Elite';[^}]*unicode-range:\s*U\+0000-00FF[^}]*\})/,
  (block) => block.replace(
    /src:\s*url\("([^"]+\.woff2)"\)\s*format\('woff2'\);/,
    'src:\n    url("assets/SpecialElite-Regular.ttf") format(\'truetype\'),\n    url("$1") format(\'woff2\');',
  ),
);

template = template.replace(
  "    el.addEventListener('click', (e)=>{\n      const a = el.dataset.action;",
  "    el.addEventListener('click', (e)=>{\n      e.preventDefault();\n      if (el.disabled) return;\n      const a = el.dataset.action;",
);

template = template.replace(
  "    const items = [...active.querySelectorAll('.menu-item:not([disabled])')];\n    let i = items.indexOf(document.activeElement);\n    if (e.key === 'ArrowDown'){ i = (i+1+items.length) % items.length; items[i].focus(); e.preventDefault(); }\n    if (e.key === 'ArrowUp')  { i = (i-1+items.length) % items.length; items[i<0?items.length-1:i].focus(); e.preventDefault(); }",
  "    const items = [...active.querySelectorAll('.menu-item:not([disabled])')];\n    if (!items.length) return;\n    let i = items.indexOf(document.activeElement);\n    if (i < 0) i = 0;\n    if (e.key === 'ArrowDown'){ i = (i+1) % items.length; items[i].focus(); e.preventDefault(); }\n    if (e.key === 'ArrowUp')  { i = (i-1+items.length) % items.length; items[i].focus(); e.preventDefault(); }",
);

template = template.replace(
  /(\.story::before\{\s*content: "";\s*position: absolute; left: 0; top: 0; right: 0; bottom: 0;\s*)background-image: url\("data:image\/svg\+xml;utf8,<svg[\s\S]*?pointer-events: none;\s*opacity: \.35;\s*\}/,
  `$1background:
      repeating-linear-gradient(0deg, rgba(255,255,255,0.018) 0 1px, transparent 1px 4px),
      linear-gradient(180deg, rgba(201,197,188,0.02), transparent 45%, rgba(0,0,0,0.18));
    pointer-events: none;
    opacity: .28;
  }`,
);

template = template.replace(
  "  @keyframes caret{ 50%{ opacity: 0; } }\n\n  .story-foot{",
  "  @keyframes caret{ 50%{ opacity: 0; } }\n\n  .story-mode .grain,\n  .story-mode .flicker,\n  .story-mode .tape-line{\n    animation: none;\n    opacity: .04;\n  }\n  .story-mode .scanlines{ opacity: .22; }\n\n  .story-foot{",
);

template = template.replace(
  /  \.typewriter\{[\s\S]*?  \.typewriter \.blood\{ color: var\(--blood-bright\); \}/,
  `  .typewriter{
    font-family: 'Special Elite', serif;
    font-size: 17px;
    line-height: 1.7;
    color: var(--ink);
    position: relative; z-index: 2;
    overflow: hidden;
    padding-right: 0;
    flex: 0 0 168px;
    min-height: 168px;
    pointer-events: none;
  }
  .typewriter::after{
    content: "";
    position: absolute;
    left: 0; right: 0; bottom: 0;
    height: 42px;
    background: linear-gradient(180deg, transparent, rgba(7,8,10,0.94));
    pointer-events: none;
  }
  .typewriter p{
    margin-bottom: 13px;
    opacity: 0;
    transform: translateY(18px);
    transition: opacity .65s ease, transform .65s ease;
  }
  .typewriter p.reveal{
    opacity: 1;
    transform: translateY(0);
  }
  .typewriter p.retiring{
    opacity: 0;
    transform: translateY(-16px);
  }
  .typewriter .quiet{ color: var(--ink-dim); font-style: italic; }
  .typewriter .blood{ color: var(--blood-bright); }`,
);

template = template.replace(
  "      next.classList.add('active');\n      trans.classList.remove('on');",
  "      if (cur && cur.id === 'screen-story' && id !== 'screen-story') stopStoryReveal();\n      next.classList.add('active');\n      document.body.classList.toggle('story-mode', id === 'screen-story');\n      trans.classList.remove('on');",
);

template = template.replace(
  "    opacity: 0; pointer-events: none;\n    transition: opacity .8s ease;",
  "    opacity: 0; pointer-events: none;\n    visibility: hidden;\n    transition: opacity .8s ease;",
);

template = template.replace(
  "  .screen.active{ opacity: 1; pointer-events: auto; }",
  "  .screen.active{ opacity: 1; pointer-events: auto; visibility: visible; }",
);

template = template.replace(
  /let typing = false;\s*function startTyping\(\)\{[\s\S]*?\n  \}\s*\n\s*\/\/ ----------- Begin game/,
  `let typing = false;
  let storyRevealTimer = 0;

  function stopStoryReveal(){
    if (storyRevealTimer) {
      clearTimeout(storyRevealTimer);
      storyRevealTimer = 0;
    }
    typing = false;
  }

  function startTyping(){
    stopStoryReveal();
    typing = true;
    const box = document.getElementById('ttype');
    if (!box) return;
    box.innerHTML = "";

    let index = 0;
    const maxVisible = 3;
    const revealNext = () => {
      if (!typing || index >= storyText.length) {
        storyRevealTimer = 0;
        return;
      }

      const data = storyText[index++];
      const paragraph = document.createElement('p');
      if (data.cls) paragraph.className = data.cls;
      paragraph.innerHTML = data.t;
      box.appendChild(paragraph);

      requestAnimationFrame(() => paragraph.classList.add('reveal'));

      while (box.children.length > maxVisible) {
        const first = box.firstElementChild;
        if (!first) break;
        first.classList.add('retiring');
        setTimeout(() => first.remove(), 680);
        break;
      }

      storyRevealTimer = setTimeout(revealNext, data.cls === 'blood' ? 2600 : 1800);
    };

    revealNext();
  }

  // ----------- Begin game`,
);

template = template.replace(
  "  function beginGame(){\n    trans.classList.add('on');",
  "  function beginGame(){\n    document.body.classList.remove('story-mode');\n    trans.classList.add('on');",
);

template = template.replace(
  "    if (Math.random() < 0.25){\n      document.body.style.filter = 'invert(1) hue-rotate(180deg)';\n      setTimeout(()=> document.body.style.filter = '', 60 + Math.random()*80);\n    }\n  }, 5500);",
  "    if (!document.body.classList.contains('story-mode') && Math.random() < 0.12){\n      document.body.style.filter = 'invert(1) hue-rotate(180deg)';\n      setTimeout(()=> document.body.style.filter = '', 60 + Math.random()*80);\n    }\n  }, 8000);",
);

template = template.replace(
  /function beginGame\(\)\{([\s\S]*?)setTimeout\(\(\)=>\{([\s\S]*?)trans\.classList\.remove\('on'\);\s*\}, 1200\);\s*\}/,
  (_match, before, inside) => `function beginGame(){${before}setTimeout(()=>{${inside.trimEnd()}
      trans.classList.remove('on');
      window.exoMenuBridge.startGame();
    }, 1200);
  }`,
);
template = template.replace(
  /function quitFx\(\)\{([\s\S]*?)setTimeout\(\(\)=>\{([\s\S]*?)\}, 1400\);\s*\}/,
  (_match, before, inside) => `function quitFx(){${before}setTimeout(()=>{${inside.trimEnd()}
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
