# AI Workflow

## Быстрый маршрут

Этот процесс нужен для AI-агентов, которые расширяют content-data слой **Zero Patient**. Он защищает проект от хаотичных структур и случайных правок вне зоны задачи.

1. Открой `docs/AI_PROJECT_BIBLE.md`.
2. Открой `data/project_manifest.json`.
3. Открой `data/ai_context.json`.
4. Найди существующие ID в `data/**`.
5. Внеси минимальное изменение в нужный JSON.
6. Проверь валидность JSON.
7. Проверь, что русский текст в `data/**` есть только в `title`, `body`, `choiceText`.
8. В финале перечисли измененные пути и коротко объясни, что внутри.

## Рабочая зона

Для AI/content задач разрешены:

```text
docs/AI_PROJECT_BIBLE.md
docs/ai-workflow.md
data/**
```

Без отдельного запроса не редактировать:

```text
CMakeLists.txt
cmake/**
src/**
include/**
samples/**
assets/**
apps/**
build/**
```

## Нейминг

Технические ID:

- `snake_case`;
- ASCII;
- без кириллицы;
- стабильные после первого коммита;
- предметы: `clinic_results`, `temporary_access_badge`;
- комнаты: `office_open_space_3f`;
- загадки: `office_badge_repair`;
- флаги: `flag_office_badge_repaired`.

Пользовательский русский текст в JSON:

- `title`;
- `body`;
- `choiceText`.

Все остальные строковые значения должны оставаться техническими.

## Проверка JSON

PowerShell проверка без сторонних зависимостей:

```powershell
Get-ChildItem data -Recurse -Filter *.json | ForEach-Object {
    $raw = Get-Content $_.FullName -Raw
    $null = $raw | ConvertFrom-Json
    $_.FullName
}
```

Если установлен Node.js:

```powershell
node -e "const fs=require('fs'); const path=require('path'); function walk(d){for(const f of fs.readdirSync(d)){const p=path.join(d,f); const s=fs.statSync(p); if(s.isDirectory()) walk(p); else if(p.endsWith('.json')) JSON.parse(fs.readFileSync(p,'utf8'));}} walk('data'); console.log('json ok')"
```

## Проверка русских строк

Ручной критерий: в `data/**` кириллица допустима только внутри ключей `title`, `body`, `choiceText`.

Если AI добавляет поле вроде `description`, `note`, `location`, `label`, `hint`, `failureText` с русским текстом, это ошибка. Нужно заменить структуру на вложенный объект с `body` или `choiceText`.

Пример:

```json
{
  "id": "office_power_panel",
  "title": "Щиток",
  "body": [
    "Внутри пустое гнездо предохранителя."
  ],
  "choiceText": "Осмотреть щиток."
}
```

## Добавление предмета

1. Добавь объект в `data/items.json`.
2. Укажи `id`, `type`, `category`, `title`, `body`, `choiceText`.
3. Если предмет появляется в комнате, добавь ссылку в `placedItems` комнаты.
4. Если предмет нужен загадке, добавь его в `requiredItems` соответствующей загадки.
5. Если предмет комбинируется, добавь `combineRules`.

## Добавление загадки

1. Добавь объект в `data/puzzles.json`.
2. Укажи `roomId`, `requiredItems`, `requiredFlags`, `setsFlags`.
3. Добавь пользовательские тексты только через `title`, `body`, `choiceText`.
4. Свяжи загадку с интерактивным объектом комнаты через `puzzleRefs`.
5. Добавь результат в `unlocks` или `rewards`.

## Добавление комнаты

1. Создай `data/rooms/<room_id>/room.json`.
2. Сохрани совместимость с текущим SceneLoader: `name`, `staticMeshes`, `pointLights`, `fixedCameras`.
3. Добавь будущий gameplay слой: `spawnPoints`, `doors`, `interactables`, `placedItems`, `puzzleRefs`, `scriptedEvents`.
4. Проверь, что каждая камера имеет понятную задачу.
5. Не добавляй ассеты в `assets/**` или `samples/**` в рамках content-data задачи.

## Финальный отчет

Финал должен быть коротким:

- список созданных или измененных путей;
- что внутри каждого файла;
- чем это усиливает проект;
- как проверялась валидность.
