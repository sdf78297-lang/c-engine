# Asset Pipeline for Fixed-Camera Survival-Horror

## Назначение

Этот документ задает правила подготовки сцен, OBJ-моделей, материалов и текстур для C++ survival-horror engine с комнатами и фиксированными камерами. Цель пайплайна - не просто загрузить ассет в runtime, а гарантировать дорогой, чистый и управляемый визуальный результат без технического мусора в сценах.

Текущий shader package (`assets/shaders/world.vert`, `assets/shaders/world.frag`) ожидает у геометрии позиции, UV и нормали. Материал использует albedo texture, tint и UV scale, а освещение строится на основном источнике, point lights, тумане, виньетке и image instability. Эти требования считаются базовым runtime contract.

## Общий поток

```text
DCC source files
  Blender, Maya, Substance, Photoshop, Krita
        |
        v
Authoring review
  scale, silhouette, camera readability, texel density
        |
        v
Technical export
  OBJ, PNG/TGA, material JSON, scene manifests
        |
        v
Validation
  names, paths, topology, UV, texture sizes, budgets
        |
        v
Runtime package
  assets/scenes/<room_id>/...
```

Source-файлы остаются в рабочем хранилище контента и не обязаны попадать в runtime package. В playable build должны попадать только проверенные файлы, нужные engine.

## Рекомендуемая структура ассетов

```text
assets/
  shaders/
    world.vert
    world.frag
  scenes/
    <room_id>/
      room.scene.json
      geometry.obj
      collision.obj
      cameras.json
      lights.json
      interactions.json
      doors.json
      materials.json
      textures/
        <room_id>_wall_plaster_albedo.png
        <room_id>_floor_tile_albedo.png
  props/
    <prop_id>/
      model.obj
      materials.json
      textures/
  characters/
    <character_id>/
      model.obj
      textures/
  audio/
    ambience/
    sfx/
    stingers/
```

В текущем репозитории уже есть `assets/shaders`. Остальная структура описывает целевую организацию, которую нужно вводить без смешивания source-файлов и runtime-файлов.

## Именование

Правила:

- только lowercase latin, цифры и underscore;
- без пробелов, кириллицы, спецсимволов и временных суффиксов;
- id ассета совпадает с именем папки или явно указан в manifest;
- файл не должен называться `final`, `new`, `test`, `copy`, `temp`, `untitled`.

Примеры:

```text
mansion_hall_1f
door_wood_ornate_a
key_clock_bronze
enemy_patient_01
mansion_hall_1f_wall_plaster_albedo.png
```

## Единицы, оси и масштаб

Единый стандарт:

- 1 world unit = 1 meter;
- Y axis = up;
- персонаж взрослого роста: 1.75-1.85 units;
- стандартная дверь: 2.0-2.2 units height, 0.85-1.1 units width;
- ступень: 0.14-0.19 units height;
- толщина стены моделируется, если она видна камерой или влияет на силуэт;
- apply scale обязателен перед экспортом;
- отрицательный scale запрещен в runtime mesh.

Pivot:

- у комнаты origin совпадает с логическим origin сцены;
- у props pivot ставится в точку естественной установки или вращения;
- у дверей pivot ставится по петлям;
- у pickup-предметов pivot ставится в центр визуального баланса.

## OBJ: технические требования

OBJ используется как runtime-friendly exchange format. Он должен быть чистым, предсказуемым и одинаково интерпретируемым на всех машинах.

### Обязательные свойства

- triangulated mesh;
- positions, UV0 и vertex normals присутствуют у всех render vertices;
- no n-gons;
- no non-manifold geometry;
- no zero-area triangles;
- no duplicate faces;
- no hidden или helper geometry в export;
- transforms applied;
- object/group names сохранены и осмысленны;
- material slots имеют стабильные имена;
- face winding consistent;
- UV coordinates не NaN и не бесконечные;
- mesh не содержит случайных объектов далеко от комнаты.

### Запрещено

- экспортировать high-poly source mesh как runtime mesh;
- использовать один OBJ одновременно как render mesh и collision mesh;
- оставлять modifiers unapplied, если результат должен попасть в runtime;
- хранить камеры, lights и triggers внутри OBJ как единственный источник правды;
- полагаться на MTL как на production material definition;
- использовать процедурные DCC-материалы без bake в texture files.

## Разделение геометрии

### `geometry.obj`

Render mesh комнаты:

- архитектура, видимая в fixed-camera ракурсах;
- крупные декорации, если они не переиспользуются как props;
- UV и normals обязательны;
- материал назначается через `materials.json`.

Рекомендуемые группы:

```text
room_floor_main
room_wall_north
room_wall_east
room_ceiling
trim_baseboards
prop_static_chandelier
```

### `collision.obj`

Collision mesh:

- только поверхности, по которым движется player/enemy или которые блокируют движение;
- низкая детализация;
- без мелких декоративных выступов;
- без тонких щелей, где может застрять capsule;
- отдельные группы для floor, wall, stairs, blockers.

Рекомендуемые группы:

```text
col_floor
col_wall_north
col_stairs_main
col_blocker_railing
```

### Trigger volumes

Triggers лучше хранить в JSON, а не в OBJ. Это упрощает валидацию и дает точные ids.

```json
{
  "id": "door_to_library",
  "type": "door",
  "shape": "box",
  "center": [2.4, 1.0, -5.6],
  "size": [1.2, 2.0, 0.45],
  "targetRoom": "library_1f",
  "targetSpawn": "from_hall"
}
```

## Полигональные бюджеты

Бюджеты уточняются профилированием, но стартовые лимиты должны быть жесткими:

| Тип ассета | Цель | Верхний предел без отдельного согласования |
| --- | ---: | ---: |
| Комната, visible geometry | 30k-80k triangles | 120k triangles |
| Комната, collision geometry | 300-3k triangles | 5k triangles |
| Крупный prop | 2k-12k triangles | 20k triangles |
| Малый pickup | 500-4k triangles | 8k triangles |
| Дверь | 1k-6k triangles | 10k triangles |
| Враг humanoid | 15k-40k triangles | 60k triangles |

Fixed-camera подход позволяет тратить детали там, где камера действительно их видит. Невидимая задняя сторона, нижняя часть пропа и мелкие bevels вне кадра должны упрощаться.

## UV requirements

- UV0 обязателен для каждого render mesh.
- UV islands не должны пересекаться, если материал не рассчитан на tiling или mirror.
- Для light-independent albedo нельзя запекать жесткие directional shadows в базовый цвет.
- Padding между UV islands: минимум 8 px для 1K, 16 px для 2K, 32 px для 4K.
- Для tiling материалов UV scale должен быть осознанно задан в `materials.json`.
- Нельзя оставлять растянутые UV на видимых стенах, дверях, полу и крупных props.

Texel density:

- hero props и puzzle close-up: 512-1024 px/m;
- двери, крупные предметы, ближние стены: 256-512 px/m;
- общая архитектура комнаты: 128-256 px/m;
- дальние или редко видимые поверхности: 64-128 px/m.

## Нормали

- Vertex normals обязательны.
- Hard edges должны соответствовать UV seams и реальным граням формы.
- Smoothing groups должны быть проверены в fixed-camera ракурсах.
- Нельзя скрывать плохую геометрию чрезмерно мягкими normals.
- Если в будущем добавляется normal map, tangent basis должен быть единым для exporter и runtime.

## Текстуры

### Форматы

Базовый production format:

- albedo: PNG, 8-bit per channel, sRGB;
- alpha texture: PNG с alpha channel;
- grayscale masks: PNG 8-bit;
- source painting files: PSD, KRA или Substance source хранятся вне runtime package.

Если появится offline packer, runtime может перейти на DDS/KTX2, но source contract для художников остается прежним: чистые source textures и валидируемый export.

### Размеры

| Назначение | Рекомендуемый размер | Верхний предел |
| --- | ---: | ---: |
| Hero prop close-up | 2048 | 4096 |
| Room wall/floor tiling | 1024-2048 | 4096 |
| Door или крупный prop | 1024-2048 | 2048 |
| Малый pickup | 512-1024 | 2048 |
| UI inspect image | 1024 | 2048 |
| Utility mask | 256-1024 | 1024 |

Power-of-two размеры обязательны для tiling textures и рекомендованы для всех runtime textures.

### Художественные требования

- Albedo должен содержать материал, возраст, грязь и локальные вариации, но не должен заменять lighting.
- Ужас строится на контрасте, силуэте и деталях, а не на равномерной темной каше.
- Важные интерактивные предметы не должны сливаться с фоном в своей главной камере.
- Следы износа должны соответствовать логике помещения: руки у дверных ручек, грязь у пола, влага в углах, пыль на горизонтальных поверхностях.
- Повторяемость tiling-текстур должна быть скрыта variation props, decals или разными UV scales.

## Material definition

MTL можно использовать как промежуточный экспорт, но runtime material задается в `materials.json`.

```json
{
  "materials": [
    {
      "id": "wall_plaster_old",
      "shader": "world",
      "albedo": "textures/mansion_hall_1f_wall_plaster_albedo.png",
      "tint": [0.86, 0.88, 0.82],
      "uvScale": 1.0,
      "surface": "plaster",
      "alphaMode": "opaque"
    },
    {
      "id": "floor_tile_wet",
      "shader": "world",
      "albedo": "textures/mansion_hall_1f_floor_tile_albedo.png",
      "tint": [0.78, 0.82, 0.80],
      "uvScale": 1.35,
      "surface": "stone_wet",
      "alphaMode": "opaque"
    }
  ]
}
```

Каждый material id должен быть уникальным в пределах package. Если материал переиспользуется между комнатами, его нужно вынести в shared library с явной версией.

## Fixed-camera art pass

Каждая комната проходит отдельную проверку по основным камерам:

- игрок читается на полу, у стен, возле двери и рядом с props;
- foreground-объекты не перекрывают управление дольше 0.5 секунды;
- дверь, ключевой предмет или puzzle element имеют понятный визуальный акцент;
- источник света логически связан с окружением;
- силуэты не разваливаются из-за плохих normals или чрезмерно шумных текстур;
- опасность может быть скрыта частично, но не должна выглядеть как случайная ошибка камеры;
- screen composition не оставляет главный путь движения в визуально мертвой зоне.

## Экспорт из Blender

Перед экспортом:

1. Set unit scale to meters.
2. Apply rotation and scale.
3. Проверить origin и pivots.
4. Triangulate runtime mesh через modifier или export option.
5. Удалить hidden helper objects из export collection.
6. Проверить UV и normals.
7. Убедиться, что material slot names совпадают с `materials.json`.

OBJ export settings:

```text
Selection Only: enabled
Apply Modifiers: enabled
Forward/Up: project preset, Y up in runtime contract
Write Normals: enabled
Include UVs: enabled
Triangulate Faces: enabled
Keep Vertex Order: enabled when supported
Write Materials: optional for review, not authoritative
```

## Scene manifest checklist

Перед попаданием в build у комнаты должны быть:

- `room.scene.json` с id, путями и spawn points;
- `geometry.obj`;
- `collision.obj`;
- `cameras.json`;
- `lights.json`;
- `materials.json`;
- textures, на которые есть ссылки;
- door links с существующими target rooms или явной пометкой blocked;
- interaction ids без дублей;
- surface types для footsteps;
- audio ambience или осознанная настройка silence.

## Validation rules

Автоматический validator должен падать на ошибках:

- missing file;
- duplicate ids;
- invalid JSON schema;
- OBJ без UV или normals;
- OBJ с n-gons, zero-area triangles или NaN coordinates;
- texture выше лимита без allowlist;
- texture path не совпадает с naming convention;
- material id из OBJ не найден в `materials.json`;
- camera volume не пересекает walkable area;
- door target room или spawn point отсутствует;
- collision mesh тяжелее допустимого бюджета.

Warnings допустимы, но должны попадать в report:

- texture не power-of-two;
- texel density ниже стандарта;
- слишком много point lights;
- камера часто переключается на маленькой площади;
- foreground prop перекрывает player capsule в ключевом ракурсе;
- albedo выглядит слишком темным или слишком контрастным для текущего lighting pass.

## Runtime acceptance

Ассет считается принятым, когда:

- сцена загружается без warning уровня error;
- камера показывает room intent с первой секунды;
- player не застревает в collision mesh;
- все двери и интерактивные зоны имеют стабильный focus;
- материалы не выглядят растянутыми, размытыми или случайно повторенными;
- point lights не создают плоскую равномерную засветку;
- frame time остается в бюджете;
- debug geometry mode помогает проверить floor, wall и ceiling classes.

## Что не проходит art direction

- пустая комната без визуальной истории;
- одинаковый грязный noise на всех поверхностях;
- предметы, которые выглядят временными или не прошли art pass;
- текстуры с узнаваемым stock-паттерном;
- слишком светлая сцена, где horror mood держится только на виньетке;
- слишком темная сцена, где игрок не понимает маршрут;
- дверь или puzzle object без акцента;
- случайная детализация вне камеры при бедном главном ракурсе.

## Definition of Done для ассета

1. Название и расположение соответствуют conventions.
2. OBJ проходит topology, scale, UV и normals checks.
3. Collision mesh отделен от render mesh.
4. Textures имеют правильный формат, размер и цветовое пространство.
5. Material JSON ссылается на существующие файлы.
6. Ассет проверен в ключевых fixed-camera ракурсах.
7. Runtime load проходит без ошибок.
8. Художественный результат усиливает доверие к проекту: сцена выглядит намеренно, дорого и профессионально.
