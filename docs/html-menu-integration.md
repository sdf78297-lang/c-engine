# HTML Menu Integration

Этот документ описывает, как в движок встроено живое HTML-меню `VOID ECHO`.
Меню не является картинкой или видео. Это настоящий HTML/CSS/JS-интерфейс,
который рендерится внутри C++/OpenGL приложения и может запускать игру.

## Где лежит

- Исходный HTML-экспорт: `assets/ui/main_menu/void_echo_menu_source.html`
- Рабочий HTML для движка: `assets/ui/main_menu/web/index.html`
- Шрифты меню: `assets/ui/main_menu/web/assets/*.woff2`
- C++-обертка меню: `include/ExoEngine/UI/HtmlMenu.h`
- Реализация меню: `src/UI/HtmlMenu.cpp`
- Точка подключения в игре: `src/Core/Application.cpp`
- CMake-флаг: `EXO_ENABLE_HTML_UI=ON`
- Локальный SDK: `local_deps/ultralight-sdk`

`local_deps/` не коммитится в Git. В репозитории хранится код интеграции,
HTML, ассеты меню и runtime DLL, нужные для запуска собранного `ExoEngine.exe`.

## Как это работает

Движок использует Ultralight как встроенный offscreen-браузер:

1. `Application` при запуске создает `Exo::HtmlMenu`.
2. `HtmlMenu` создает Ultralight renderer и offscreen view размером с окно игры.
3. В view загружается `file:///assets/ui/main_menu/web/index.html`.
4. Каждый кадр Ultralight обновляет HTML, CSS-анимации и JavaScript.
5. Полученная BGRA-картинка копируется в OpenGL texture.
6. Texture рисуется fullscreen поверх окна игры.
7. Пока меню активно, 3D-управление игроком отключено.
8. После `Новая игра` открывается экран предыстории/диагноза внутри HTML-меню.
9. После `Принять диагноз` меню закрывается, и C++ запускает 3D-сцену.

Так меню остается живым: hover, анимации, VHS-слои, переходы и JS работают
как в обычном браузере, но поверх нашего C++ движка.

## Ввод

`HtmlMenu::handleEvent` пересылает события SDL в Ultralight:

- движение мыши;
- левый/правый/средний клик;
- `ArrowUp`;
- `ArrowDown`;
- `Enter`;
- `Escape`.

HTML сам решает, какой пункт выбран и что делать при клике. C++ не знает
координаты кнопок и не содержит ручных click-zone.

## Bridge HTML -> C++

Кнопки меню отправляют команды в движок через маленький bridge.

В HTML:

- `Новая игра` открывает `screen-story` и печатает предысторию;
- `Принять диагноз` вызывает `window.exoMenuBridge.startGame()`;
- `Выход` вызывает `window.exoMenuBridge.quitGame()`;
- команда отправляется двумя способами:
  - через `document.title = "exo:start-game:<nonce>"`;
  - через `window.location.href = "exo://start-game?nonce=<nonce>"`.

В C++:

- `ulViewSetChangeURLCallback` ловит переходы `exo://...`;
- `ulViewSetChangeTitleCallback` ловит `exo:...` через title;
- оба пути идут в `HtmlMenu::handleBridgeMessage`;
- `start-game` ставит `startRequested_ = true`;
- `quit-game` ставит `quitRequested_ = true`.

Два канала нужны специально. В embedded-браузерах custom URL scheme иногда
ведет себя по-разному, а title callback срабатывает стабильно. Поэтому кнопки
не завязаны на один хрупкий механизм.

## Что делает Application

В `Application::runWindowed`:

- если обычный запуск или `--menu`, создается `HtmlMenu`;
- пока `mainMenuActive == true`, события уходят в меню;
- каждый кадр вызывается `mainMenu.update()` и `mainMenu.render()`;
- если `mainMenu.startRequested()`:
  - меню уничтожается;
  - `mainMenuActive = false`;
  - при обычном запуске включается relative mouse mode;
  - начинается 3D-геймплей;
- если `mainMenu.quitRequested()`:
  - главный цикл завершается;
  - игра закрывается.

## Центрирование меню

В Ultralight оказалась проблема с CSS-сокращением `inset: 0`: слой `.screen`
не всегда растягивался на весь viewport, из-за чего меню выглядело сдвинутым
влево.

Поэтому в runtime HTML используется явное позиционирование:

```css
.screen {
  position: fixed;
  left: 0;
  top: 0;
  right: 0;
  bottom: 0;
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
}
```

То же правило применено к VHS-слоям, transition overlay и fullscreen overlay.
Это надежнее для Ultralight, чем `inset`.

Пункты меню выровнены отдельно: номер и стрелка стоят absolute-слева, а сам
текст кнопки центрируется внутри фиксированной ширины. Поэтому визуальная ось
логотипа и пунктов совпадает.

## Шрифты

Заголовок `VOID ECHO` использует локальный `SpecialElite-Regular.ttf`.
Это важно: ранний перенос использовал только `.woff2`, но Ultralight 1.3 в
нашем offscreen-рендере мог не подхватить WOFF2 и уходил в fallback serif.
Визуально это выглядело так, будто шрифт "не перенесся": буквы становились
маленькими и гладкими.

Для стабильного результата runtime HTML подключает TTF первым:

```css
src:
  url("assets/SpecialElite-Regular.ttf") format("truetype"),
  url("assets/7e96dbe3-0a73-4e85-8a84-6392e65403d9.woff2") format("woff2");
```

Также для заголовка нельзя использовать `clamp(...)`: в текущем Ultralight оно
не отработало как в браузере. Поэтому размер задан явным `font-size`, а
`VOID` и `ECHO` вынесены в отдельные строки через `<span>`.

## Почему не screenshot

Скриншот потерял бы главное:

- hover;
- VHS-анимации;
- flicker/glitch;
- переходы;
- клики;
- настройки;
- возможность запускать игру из меню.

Поэтому меню перенесено как живой HTML, а не как фон-картинка.

## Как менять меню дальше

Если нужно поменять внешний вид, текст, расположение или анимации:

1. Менять `assets/ui/main_menu/web/index.html`.
2. Не трогать C++ ради одной надписи или отступа.
3. Не удалять `window.exoMenuBridge`.
4. Не менять команды `start-game` и `quit-game` без правки C++.
5. После правки проверять реальное SDL-окно, а не только браузер.

Если нужно добавить новую кнопку:

1. Добавить кнопку в HTML с `data-action`.
2. Добавить обработку в JS.
3. Если кнопка должна влиять на движок, добавить новую команду в
   `exoCommand(...)`.
4. Добавить обработку команды в `HtmlMenu::handleBridgeMessage`.

## Сборка

```powershell
$env:Path = 'C:\msys64\mingw64\bin;' + $env:Path
cmake -S . -B build -G Ninja -DEXO_ENABLE_HTML_UI=ON -DEXO_ULTRALIGHT_SDK="C:/Users/Vladimir/Desktop/exo1/local_deps/ultralight-sdk"
cmake --build build
```

После сборки корневой exe обновляется так:

```powershell
Copy-Item .\build\exo_sandbox.exe .\ExoEngine.exe -Force
```

## Проверка

Минимальный набор проверок:

```powershell
$env:Path = 'C:\msys64\mingw64\bin;' + $env:Path
.\build\exo_validate_content.exe
.\ExoEngine.exe --headless --frames 1
.\ExoEngine.exe --menu --frames 60
```

Ручная проверка:

- запустить `ExoEngine.exe`;
- убедиться, что меню стоит по центру;
- нажать `Новая игра`;
- убедиться, что открыт экран предыстории/диагноза;
- нажать `Принять диагноз`;
- убедиться, что запускается 3D-сцена;
- запустить снова;
- нажать `Выход`;
- убедиться, что игра закрывается.

Автоматическая проверка, которую делал Codex:

- нашел реальное SDL-окно процесса;
- сделал крупный скриншот именно этого окна;
- кликнул по `Новая игра` и проверил экран предыстории;
- кликнул по `Принять диагноз`;
- проверил лог `HTML menu requested game start`;
- кликнул по `Выход`;
- проверил, что процесс завершился.
