# Main Menu Runtime

`void_echo_menu_source.html` is the untouched source exported by the HTML tool.

`web/index.html` is the runtime version used by the engine. It is unpacked from the standalone bundle so Ultralight can load it directly without the temporary bundler bootstrap, `Blob` URLs, or `DecompressionStream`.

The runtime contract with C++ is URL-based:

- `exo://start-game` starts the 3D game after the menu's own transition animation.
- `exo://quit-game` closes the game after the menu's quit animation.

Do not replace this with a PNG or video capture. The engine renders this HTML live every frame through `Exo::HtmlMenu`.
