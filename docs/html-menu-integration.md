# Live HTML Menu Integration

The menu is transferred as a live HTML/CSS/JS interface, not as a screenshot.

## Runtime Path

- Source bundle: `assets/ui/main_menu/void_echo_menu_source.html`
- Engine-ready HTML: `assets/ui/main_menu/web/index.html`
- Extracted font assets: `assets/ui/main_menu/web/assets/*.woff2`
- C++ bridge: `Exo::HtmlMenu`
- Build flag: `EXO_ENABLE_HTML_UI=ON`
- Local SDK path: `local_deps/ultralight-sdk`

## Architecture

`Application` starts in menu mode when running normally. `HtmlMenu` creates an Ultralight offscreen view, loads `file:///assets/ui/main_menu/web/index.html`, advances CSS/JS animations each frame, uploads the Ultralight BGRA bitmap into an OpenGL texture, and draws it fullscreen before the 3D game loop starts.

Mouse movement, mouse clicks, and basic keyboard navigation are forwarded into the HTML view. The HTML page keeps its own transitions, hover effects, typewriter effect, VHS layers, and settings interactions.

The bridge from HTML back to C++ is URL-based:

- `exo://start-game` closes the menu and enables player control.
- `exo://quit-game` exits after the menu's quit effect.

This avoids hard-coded click zones in C++ and lets menu layout stay controlled by HTML.

## Library Choice

Ultralight is the preferred path for this project because it supports embedding web content into C/C++ games and can render through a CPU bitmap or custom GPU path. WebView2 can also host HTML/CSS/JS in native Windows apps, but it is a child-window overlay rather than an OpenGL texture. CEF supports offscreen rendering too, but it is much heavier for this engine.

## Rules

- Do not use PNG screenshots for this menu.
- Do not edit C++ for one menu label or visual adjustment; edit `web/index.html`.
- Keep the standalone source file as reference, but ship the unpacked runtime HTML.
- If the HTML export changes, unpack it again and keep the `exo://` bridge.
- Do not commit `local_deps/`; the SDK is a local build dependency.

## Verification

- Open `assets/ui/main_menu/web/index.html` in a browser and confirm the menu animates.
- Build with `-DEXO_ENABLE_HTML_UI=ON`.
- Start `ExoEngine.exe`; the animated HTML menu should appear first.
- Click `Новая игра`, then `Войти в клинику`; after the menu transition, the 3D room should start and mouse look should be captured.
