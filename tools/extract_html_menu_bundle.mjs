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

const generatedExtensions = new Set(Object.values(extByMime));
for (const entry of fs.readdirSync(assetDir, { withFileTypes: true })) {
  if (!entry.isFile()) {
    continue;
  }
  if (generatedExtensions.has(path.extname(entry.name).toLowerCase())) {
    fs.rmSync(path.join(assetDir, entry.name));
  }
}

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
  "  @keyframes caret{ 50%{ opacity: 0; } }\n\n  .story-mode .grain,\n  .story-mode .flicker,\n  .story-mode .tape-line,\n  .story-mode .crt-curve{\n    display: none;\n  }\n  .story-mode .scanlines{ opacity: .12; }\n  .story-mode .vignette{ mix-blend-mode: normal; opacity: .82; }\n  .story-mode .screen:not(.active) *,\n  .story-mode .screen:not(.active)::before,\n  .story-mode .screen:not(.active)::after{\n    animation-play-state: paused !important;\n  }\n\n  .story-foot{",
);

template = template.replace(
  /  \.typewriter\{[\s\S]*?  \.typewriter \.blood\{ color: var\(--blood-bright\); \}/,
  `  .typewriter{
    font-family: 'Special Elite', serif;
    font-size: 18px;
    line-height: 1.85;
    color: var(--ink);
    position: relative; z-index: 2;
    overflow: hidden;
    padding-right: 12px;
    flex: 1;
    min-height: 150px;
    pointer-events: none;
    contain: paint;
  }
  .typewriter-stream{
    transform: translateY(0);
    transition: transform .22s ease-out;
    will-change: transform;
  }
  .typewriter p{
    margin-bottom: 16px;
    animation: paragraphRise .42s ease both;
  }
  @keyframes paragraphRise{ from{ opacity: 0; transform: translateY(10px); } to{ opacity: 1; transform: translateY(0); } }
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
  let storyTypeRaf = 0;
  let storyParaTimer = 0;
  let storyFlow = null;
  let storyCurrentTarget = null;
  let storyTargetStack = [];
  let storyTextNode = null;
  let storyLastFlowAt = 0;

  function stopStoryReveal(){
    typing = false;
    if (storyTypeRaf) {
      cancelAnimationFrame(storyTypeRaf);
      storyTypeRaf = 0;
    }
    if (storyParaTimer) {
      clearTimeout(storyParaTimer);
      storyParaTimer = 0;
    }
  }

  function storyTokens(html){
    const tokens = [];
    for (let i = 0; i < html.length;) {
      if (html[i] === '<') {
        const close = html.indexOf('>', i);
        if (close !== -1) {
          tokens.push({ tag: html.slice(i, close + 1).toLowerCase(), pause: 0 });
          i = close + 1;
          continue;
        }
      }

      const ch = html[i++];
      const pause =
        ch === '.' || ch === '!' || ch === '?' ? 180 :
        ch === ',' || ch === ';' || ch === ':' || ch === '—' ? 60 :
        0;
      tokens.push({ text: ch, pause });
    }
    return tokens;
  }

  function storyResetWriter(p){
    const caret = document.createElement('span');
    caret.className = 'caret';
    p.appendChild(caret);
    storyCurrentTarget = p;
    storyTargetStack = [];
    storyTextNode = null;
    return caret;
  }

  function storyAppendToken(p, caret, token){
    if (token.tag) {
      if (token.tag === '<b>' || token.tag === '<i>') {
        const element = document.createElement(token.tag === '<b>' ? 'b' : 'i');
        if (storyCurrentTarget === p) {
          p.insertBefore(element, caret);
        } else {
          storyCurrentTarget.appendChild(element);
        }
        storyTargetStack.push(storyCurrentTarget);
        storyCurrentTarget = element;
        storyTextNode = null;
      } else if (token.tag === '</b>' || token.tag === '</i>') {
        storyCurrentTarget = storyTargetStack.pop() || p;
        storyTextNode = null;
      }
      return;
    }

    if (!storyTextNode) {
      storyTextNode = document.createTextNode('');
      if (storyCurrentTarget === p) {
        p.insertBefore(storyTextNode, caret);
      } else {
        storyCurrentTarget.appendChild(storyTextNode);
      }
    }
    storyTextNode.data += token.text;
  }

  function syncStoryFlow(now, force){
    if (!storyFlow) return;
    if (!force && now - storyLastFlowAt < 260) return;
    const box = document.getElementById('ttype');
    if (!box) return;
    const offset = Math.max(0, storyFlow.offsetHeight - box.clientHeight);
    storyFlow.style.transform = offset > 0 ? \`translateY(\${-offset}px)\` : '';
    storyLastFlowAt = now;
  }

  function startTyping(){
    stopStoryReveal();
    typing = true;
    const box = document.getElementById('ttype');
    if (!box) return;
    box.innerHTML = "";
    storyFlow = document.createElement('div');
    storyFlow.className = 'typewriter-stream';
    box.appendChild(storyFlow);
    storyLastFlowAt = 0;

    let pi = 0;
    function nextPara(){
      if (!typing) return;
      if (pi >= storyText.length) {
        typing = false;
        return;
      }

      const data = storyText[pi++];
      const p = document.createElement('p');
      if (data.cls) p.className = data.cls;
      storyFlow.appendChild(p);
      const caret = storyResetWriter(p);

      const tokens = storyTokens(data.t);
      let ti = 0;
      let nextAt = performance.now();

      function paint(now){
        if (!typing) return;
        let changed = false;
        let budget = 0;

        while (ti < tokens.length && now >= nextAt && budget < 8) {
          const token = tokens[ti++];
          storyAppendToken(p, caret, token);
          nextAt += token.tag ? 0 : (12 + Math.random() * 30 + token.pause);
          changed = true;
          budget++;
        }

        if (changed) {
          syncStoryFlow(now, false);
        }

        if (ti >= tokens.length) {
          caret.remove();
          syncStoryFlow(now, true);
          storyTypeRaf = 0;
          storyParaTimer = setTimeout(nextPara, 420);
          return;
        }

        storyTypeRaf = requestAnimationFrame(paint);
      }

      storyTypeRaf = requestAnimationFrame(paint);
    }

    nextPara();
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
  "  setInterval(()=>{\n    const bpm = 64 + Math.floor(Math.random()*22);\n    document.getElementById('hud-time').textContent = String(bpm).padStart(3,'0');\n  }, 900);",
  "  setInterval(()=>{\n    if (document.body.classList.contains('story-mode')) return;\n    const bpm = 64 + Math.floor(Math.random()*22);\n    document.getElementById('hud-time').textContent = String(bpm).padStart(3,'0');\n  }, 900);",
);

template = template.replace(
  "  setInterval(()=>{\n    const n = 4 + Math.floor(Math.random()*4);\n    document.getElementById('trk').textContent = '■'.repeat(n) + '□'.repeat(7-n);\n  }, 800);",
  "  setInterval(()=>{\n    if (document.body.classList.contains('story-mode')) return;\n    const n = 4 + Math.floor(Math.random()*4);\n    document.getElementById('trk').textContent = '■'.repeat(n) + '□'.repeat(7-n);\n  }, 800);",
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

if (/__bundler\/(?:manifest|template)|__bundler_loading|__bundler_thumbnail|DecompressionStream|createObjectURL|DOMParser/.test(template)) {
  throw new Error('Extracted menu still contains the standalone bundler runtime. Use void_echo_menu_source.html as source and keep web/index.html as generated runtime.');
}

fs.writeFileSync(path.join(outDir, 'index.html'), template, 'utf8');

console.log(`Extracted ${extractedAssets} assets to ${outDir}`);
