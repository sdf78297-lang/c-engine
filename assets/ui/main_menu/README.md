# Main Menu Runtime

`void_echo_menu_source.html` is the untouched source exported by the HTML tool.

`web/index.html` is the runtime version used by the engine. It is unpacked from the standalone bundle so Ultralight can load it directly without the temporary bundler bootstrap, `Blob` URLs, or `DecompressionStream`.

The runtime contract with C++ is URL-based:

- `Новая игра` opens the prehistory/diagnosis screen inside the live HTML menu.
- `Принять диагноз` sends `exo://start-game` after the menu's own transition animation and starts the 3D game.
- `exo://quit-game` closes the game after the menu's quit animation.

The diagnosis screen uses a static story render plus a lighter `story-mode` effect profile. Do not bring back per-character DOM typewriter timers there; Ultralight stutters on that screen when the letter, card, and fullscreen VHS layers update at the same time.

Do not replace this with a PNG or video capture. The engine renders this HTML live every frame through `Exo::HtmlMenu`.
