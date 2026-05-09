# Main Menu Runtime

`void_echo_menu_source.html` is the untouched source exported by the HTML tool.

`web/index.html` is the runtime version used by the engine. It is unpacked from the standalone bundle so Ultralight can load it directly without the temporary bundler bootstrap, `Blob` URLs, or `DecompressionStream`.

The runtime contract with C++ is URL-based:

- `Новая игра` opens the prehistory/diagnosis screen inside the live HTML menu.
- `Принять диагноз` sends `exo://start-game` after the menu's own transition animation and starts the 3D game.
- `exo://quit-game` closes the game after the menu's quit animation.

The diagnosis screen uses a lightweight paragraph-by-paragraph reveal plus a lighter `story-mode` effect profile. Do not bring back per-character DOM typewriter timers or an internal draggable story scrollbar there; Ultralight stutters when the letter, card, fullscreen VHS layers, and text scrolling repaint at the same time.

Do not replace this with a PNG or video capture. The engine renders this HTML live every frame through `Exo::HtmlMenu`.
