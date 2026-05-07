# C++ Survival-Horror Engine Architecture

## Назначение

Документ описывает целевую архитектуру C++ engine для survival-horror в духе классических Resident Evil: сцены-комнаты, фиксированные камеры, плотная режиссура, ограниченные ресурсы, предметные загадки, тревожный свет и читаемая навигация. Это не набор разрозненных технических идей, а рабочий стандарт для команды разработки и контента.

Текущая runtime-среда проекта выглядит как desktop-сборка на SDL2, SDL2_mixer, GLEW и OpenGL 3.3. Наличие `assets/shaders/world.vert` и `assets/shaders/world.frag` задает базовый контракт рендера: вершинные позиции, UV, нормали, albedo-текстура, tint, масштаб UV, основной свет, до 32 point lights, туман, виньетка, нестабильность изображения и debug-режим геометрии.

## Продуктовый фокус

Engine должен поддерживать не просто перемещение персонажа по 3D-комнатам, а постановочную хоррор-сцену:

- фиксированные камеры работают как режиссура, а не как техническое ограничение;
- каждая комната читается за одну-две секунды после входа;
- предметы, двери, интерактивные зоны и угрозы имеют визуальные акценты без лишнего UI;
- темп строится на дефиците ресурсов, звуке, расстоянии до опасности и смене ракурсов;
- система данных позволяет собирать новые комнаты без изменения C++ кода.

## Основные принципы

- **Data-driven first**: сцены, камеры, свет, интерактивные объекты, двери, флаги и триггеры описываются данными.
- **Small runtime contracts**: C++ загружает строгие форматы, валидирует их и не пытается исправлять плохие ассеты на лету.
- **Room-based streaming**: активна одна основная сцена-комната и небольшой набор заранее прогретых ресурсов соседних переходов.
- **Predictable horror pacing**: смена камер, звук и блокировка управления на переходах должны быть воспроизводимыми.
- **Debuggability**: у каждой сцены должны быть режимы проверки камер, коллизий, триггеров, света, навигации и интерактивных зон.

## Системная схема

```text
Application
  PlatformLayer
    Window, input, timers, filesystem, audio device
  Engine
    GameLoop
    ResourceManager
    SceneManager
    Renderer
    AudioSystem
    PhysicsCollision
    GameplaySystems
    SaveSystem
    DebugTools
  Game
    Rooms
    Actors
    Inventory
    Puzzles
    ScriptedEvents
```

## Runtime flow

1. `Application` инициализирует SDL2, OpenGL context, аудио, пути ресурсов и настройки.
2. `ResourceManager` загружает манифесты, шейдеры, дефолтные материалы и системные ассеты.
3. `SceneManager` открывает стартовую сцену, создает actor registry, коллизии, камеры, свет и ambient audio.
4. `GameLoop` выполняет фиксированный gameplay update и отдельный render update.
5. `CameraDirector` выбирает активную фиксированную камеру по объемам, приоритетам и состоянию сцены.
6. `Renderer` рисует комнату, динамические объекты, отладочные overlay-слои и постэффекты.
7. `SaveSystem` сохраняет только устойчивое состояние игры: флаги, инвентарь, состояние комнат, позицию входа и версию данных.

## Подсистемы

| Подсистема | Ответственность | Ключевые требования |
| --- | --- | --- |
| `PlatformLayer` | Окно, input, high-resolution time, filesystem paths | Изолировать SDL2 от gameplay-кода |
| `GameLoop` | Порядок update/render, frame pacing | Фиксированный simulation step, защита от spiral of death |
| `ResourceManager` | Загрузка и кеширование ассетов | Handle-based API, ref-count или lifetime scopes, понятные ошибки |
| `SceneManager` | Загрузка комнат, переходы, reset, preload | Никаких raw-путей в gameplay-коде |
| `Renderer` | Shaders, materials, lights, render queues | Поддержка текущего world shader и расширяемость материалов |
| `CameraDirector` | Фиксированные камеры, cut rules, приоритеты | Без дрожания при пересечении объемов, ручные override-события |
| `PhysicsCollision` | Static collision, triggers, raycasts, character capsule | Быстрое разделение render mesh и collision mesh |
| `InteractionSystem` | Интерактивные зоны, подсказки, use item | Стабильный focus target и сценарные проверки |
| `InventorySystem` | Предметы, combine, inspect, ключи | Данные предметов отдельно от UI |
| `AudioSystem` | Music, ambience, positional SFX, stingers | SDL2_mixer facade, приоритеты каналов, fade rules |
| `SaveSystem` | Сохранения, миграции версий | Версионированные слоты, checksum, атомарная запись |
| `DebugTools` | Runtime views, validation reports | Включается без изменения игровых данных |

## Сцены и комнаты

Базовая единица контента - `Room`. Комната загружается как цельный пакет данных, но внутри разделяется на визуальную геометрию, коллизии, камеры, свет, интерактивные объекты и сценарные события.

Рекомендуемая структура scene package:

```text
assets/scenes/<room_id>/
  room.scene.json
  geometry.obj
  collision.obj
  cameras.json
  lights.json
  interactions.json
  doors.json
  materials.json
  textures/
```

Минимальный состав `room.scene.json`:

```json
{
  "id": "mansion_hall_1f",
  "displayName": "Main Hall 1F",
  "geometry": "geometry.obj",
  "collision": "collision.obj",
  "materials": "materials.json",
  "cameras": "cameras.json",
  "lights": "lights.json",
  "interactions": "interactions.json",
  "doors": "doors.json",
  "ambientTrack": "audio/ambience/mansion_hall_loop.ogg",
  "spawnPoints": [
    { "id": "from_front_door", "position": [0.0, 0.0, -4.8], "yaw": 0.0 }
  ]
}
```

### Lifecycle комнаты

1. Validate manifest.
2. Load geometry, materials and textures.
3. Build static collision acceleration structure.
4. Load actors and gameplay state overrides from save.
5. Preload adjacent door targets marked as high probability.
6. Fade from transition screen or door animation.
7. Activate camera director and room ambience.
8. On exit, persist dirty room state and release non-shared resources.

## Фиксированные камеры

Камеры являются частью дизайна комнаты. Они задают страх, читаемость и направление движения. Camera system должна быть предсказуемой для игрока и удобной для level designer.

### Camera volume

Каждая камера привязана к объему действия. При входе player capsule в объем `CameraDirector` выбирает камеру с максимальным приоритетом.

```json
{
  "id": "hall_wide_stairs",
  "position": [4.2, 2.1, -6.0],
  "target": [0.6, 1.1, -1.2],
  "fov": 42.0,
  "near": 0.05,
  "far": 35.0,
  "priority": 20,
  "volume": {
    "type": "box",
    "center": [0.0, 0.8, -2.4],
    "size": [7.5, 2.4, 5.0]
  },
  "cut": "hard",
  "composition": {
    "playerScreenMin": [0.18, 0.18],
    "playerScreenMax": [0.82, 0.86]
  }
}
```

### Правила переключения

- Основной стиль - hard cut. Он поддерживает классическую постановку и не размазывает угрозу.
- Blend разрешен только для спокойных зон, лестниц, лифтов и scripted reveal.
- Если игрок стоит на границе двух объемов, используется hysteresis: камера не меняется, пока игрок не выйдет из текущей зоны с запасом.
- Сценарные события могут временно заблокировать камеру: осмотр двери, появление врага, pickup, puzzle close-up.
- Камера обязана держать персонажа читаемым. Нельзя допускать постоянное перекрытие стеной, колонной или foreground-пропом.

### Требования к постановке

- В каждом ракурсе должен быть понятен ближайший путь движения.
- Двери и важные интерактивные объекты читаются силуэтом или светом.
- Опасность может быть частично скрыта, но игрок должен понимать, что управление остается честным.
- Резкая смена направления управления после cut компенсируется camera-relative movement lock на 120-180 мс.

## Управление персонажем

Engine должен поддерживать два режима:

- **Classic tank controls**: вперед/назад относительно персонажа, поворот влево/вправо.
- **Camera-relative controls**: движение относительно текущего ракурса с сохранением направления через camera cut.

Оба режима используют один `PlayerMotor`: capsule collision, acceleration, turn speed, footstep surface events, interaction focus ray или focus volume.

## Actor model

Actor - runtime-сущность с уникальным id и набором компонентов. Для horror engine важна не универсальная ECS ради ECS, а прозрачная модель, которую легко сохранять и отлаживать.

Базовые компоненты:

- `TransformComponent`: позиция, rotation, scale.
- `RenderableComponent`: mesh handle, material overrides, visibility flags.
- `ColliderComponent`: capsule, box, trigger или ссылка на static mesh.
- `InteractableComponent`: focus rules, prompt id, action command.
- `InventoryItemComponent`: item id, pickup text, inspect model.
- `DoorComponent`: target room, target spawn, lock state, transition type.
- `PuzzleComponent`: local state, required flags, solved event.
- `AudioEmitterComponent`: loop/stinger, radius, priority.
- `EnemyComponent`: state machine, perception, attack ranges.

## События и сценарии

Для первой версии достаточно event-command системы поверх C++:

```json
{
  "onEnter": [
    { "ifFlagMissing": "hall_intro_seen", "do": "play_stinger", "sound": "stingers/hall_intro.ogg" },
    { "setFlag": "hall_intro_seen" }
  ],
  "onInteract:grandfather_clock": [
    { "ifHasItem": "clock_key", "do": "open_panel" },
    { "else": "show_text", "textId": "clock_locked" }
  ]
}
```

Сценарии не должны превращаться в неограниченный скриптовый слой. Каждая команда имеет C++ реализацию, строгую схему данных и понятный debug output.

## Рендеринг

Текущий shader contract ориентирован на атмосферные комнаты: albedo, нормали, tint, UV scale, основной источник света, массив point lights, fog, vignette, instability и debug geometry. Архитектура рендера должна сохранить этот контракт и расширять его без ломки данных.

### Render pipeline

1. Collect visible room geometry and actors.
2. Resolve material instances.
3. Upload camera matrices and per-frame uniforms.
4. Render opaque geometry front-to-back.
5. Render alpha-tested details, если они используются в сцене.
6. Render debug overlays по выбранному режиму.
7. Apply image treatment: vignette, grain, instability, chromatic shift approximation.

### Материалы

Material instance содержит:

- shader id;
- albedo texture;
- tint;
- uv scale;
- alpha mode;
- surface type для footsteps и impact sounds;
- debug color для валидации геометрии.

Normal/roughness/metallic maps можно добавить позже, но базовый horror look должен работать на сильном albedo, нормалях геометрии, свете, тумане и грамотной композиции.

### Свет

- До 32 point lights в текущем shader contract.
- Комната должна иметь один понятный key light или dominant practical source.
- Point lights используются как управляемые акценты, а не как попытка равномерно осветить все.
- Для хоррора важна читаемая темнота: игрок видит путь, силуэт угрозы и интерактивные элементы.

## Коллизии и триггеры

Render mesh и collision mesh всегда разделены. Collision mesh должен быть проще, чище и стабильнее.

Типы геометрии:

- `StaticWorld`: стены, полы, лестницы, крупная архитектура.
- `CollisionWorld`: упрощенная навигационная и физическая поверхность.
- `CameraTrigger`: объемы выбора камер.
- `InteractionTrigger`: зоны осмотра и использования предметов.
- `EnemyNavBlocker`: блокеры и разрешенные зоны перемещения врагов.
- `DoorVolume`: переходы между комнатами.

Character collision:

- player capsule с radius/height из tuning data;
- sliding по стенам;
- step height для низких порогов;
- slope limit для наклонных поверхностей;
- raycasts для интерактивного focus и line of sight.

## ResourceManager

Ресурсы должны открываться через стабильные id, а не через raw path в игровом коде.

```text
TextureHandle LoadTexture(ResourceId id)
MeshHandle    LoadMesh(ResourceId id)
SoundHandle   LoadSound(ResourceId id)
RoomHandle    LoadRoom(RoomId id)
```

Требования:

- понятный log при ошибке загрузки;
- аварийный resource только для editor/debug mode;
- release по lifetime scope комнаты;
- prewarm ресурсов соседних дверей;
- проверка версий manifest-файлов.

## Audio

Звук для survival-horror равен по важности картинке. AudioSystem должен поддерживать:

- looping ambience на комнату;
- positional SFX для дверей, механизмов, шагов и врагов;
- one-shot stingers с приоритетом;
- music state с fade in/out;
- surface-based footsteps;
- ducking ambience при важных событиях.

Каждая комната обязана иметь audio identity: тишина, гул, электрический шум, вода, дальний металл, ветер или другой управляемый слой. Нельзя полагаться только на музыку.

## Inventory и предметы

Inventory data отделяется от UI. Предмет описывает:

- id;
- локализуемое имя и описание;
- inspect model или image;
- stack policy;
- combine rules;
- use rules по room/object id;
- flags, которые выставляются после pickup/use.

Для классической структуры важны `inspect`, `combine`, `use`, `discard policy`, `key item lock`. Эти правила должны быть тестируемыми без запуска всей сцены.

## Save system

Сохранение должно быть версионированным и устойчивым к изменению данных.

Сохраняется:

- player profile и настройки;
- текущая комната и spawn point;
- позиция, yaw и health;
- inventory;
- global flags;
- room-local flags;
- состояние дверей, предметов, puzzles и врагов;
- версия схемы сохранения.

Не сохраняется:

- raw pointers;
- GPU handles;
- временные аудио-каналы;
- transient camera state, если он не нужен для точного восстановления.

Запись слота должна быть атомарной: временный файл, flush, rename. При ошибке слот не должен повреждаться.

## Debug и tooling

Минимальные debug modes:

- camera volumes;
- active camera id and priority;
- collision mesh;
- interaction zones;
- door links;
- light spheres and radii;
- room state flags;
- resource lifetime report;
- frame timing: update, render, GPU wait.

Для сборки контента критичен `room validation report`: список проблем с путями, id, пересечениями камер, отсутствующими материалами, пустыми UV, слишком тяжелой геометрией и невалидными переходами.

## Качество и performance budget

Цели первой стабильной версии:

- 60 FPS на mid-range desktop GPU при 1920x1080;
- room transition без видимого stutter после preload;
- cold room load до 2 секунд для production-sized комнаты;
- не более 32 dynamic point lights в комнате;
- все ассеты проходят offline validation до попадания в playable build;
- runtime error сообщает точный asset id и поле, которое не прошло проверку.

## Риски архитектуры

- Смешивание render mesh и collision mesh быстро делает комнаты нестабильными.
- Камеры без hysteresis создают дерганую смену ракурсов на границах.
- Сценарии без схемы данных превращаются в набор частных случаев.
- Texture-heavy подход без бюджета ломает загрузку комнат и память.
- Плохой audio priority приводит к потере важных сигналов: шагов, врагов, замков и stingers.

## Ближайший порядок реализации

1. Зафиксировать schema для room package, cameras, lights, interactions и materials.
2. Добавить validator, который запускается отдельно от игры и выдает читаемый report.
3. Реализовать `SceneManager` с загрузкой одной комнаты и переходом через дверь.
4. Ввести `CameraDirector` с volumes, priorities и hysteresis.
5. Разделить render mesh, collision mesh и trigger volumes.
6. Подключить resource handles и lifetime scopes.
7. Добавить debug overlays для камер, коллизий, света и интерактивных зон.
8. После стабильной комнаты расширять inventory, puzzles, save system и enemy AI.

## Definition of Done для engine feature

Фича считается готовой, когда:

- данные описаны схемой и проходят validation;
- runtime не требует ручных raw path;
- ошибка загрузки понятна дизайнеру и разработчику;
- есть debug visualization или диагностический log;
- поведение воспроизводимо после reload комнаты;
- состояние корректно сохраняется или явно отмечено как transient;
- feature не ломает fixed-camera читаемость и темп survival-horror.
