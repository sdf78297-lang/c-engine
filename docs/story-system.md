# Story Runtime

`StoryRuntime` is the first gameplay layer for **НУЛЕВОЙ ПАЦИЕНТ**. It keeps the narrative graph outside C++ code so writers, designers and AI agents can edit scenes without touching the renderer.

Runtime file:

```text
samples/zero_patient_story.json
```

## Format

```json
{
  "title": "НУЛЕВОЙ ПАЦИЕНТ",
  "startNode": "prologue_parking",
  "identity": 100,
  "nodes": []
}
```

Each node has:

- `id`: stable machine id;
- `title`: UI title;
- `location`: optional player-facing location;
- `body`: ordered paragraphs;
- `choices`: player choices;
- `identityOverride`: optional hard set for the "Я" scale;
- `ending`: marks terminal endings.

Choice format:

```json
{
  "label": "Рассказать все.",
  "nextNode": "ending_still_here",
  "identityDelta": 75
}
```

`identityDelta` is applied before entering the next node. Then `identityOverride`, if present on the target node, wins.

## Runtime Controls

In the sandbox:

- `1`, `2`, `3`: select the visible story choice;
- `WASD`: move the debug camera;
- mouse: look around;
- `Tab`: release or capture the mouse;
- `Esc`: quit.

The story panel is rendered through Dear ImGui next to the engine debug panel. It shows:

- current story node;
- location;
- paragraphs;
- choices;
- the "Я" percentage bar.

## Current Narrative Slice

The current JSON includes the full first playable structure:

- prologue at the clinic parking lot;
- office arrival;
- Zhenya scene;
- restroom mirror;
- meeting room phone call;
- elevator anomaly;
- Nadya call;
- late office confrontation with the Other;
- home sequence;
- bathroom mirror;
- bedroom night sequence;
- office breakdown;
- impossible child room;
- final choice with Nadya;
- three endings: `Я еще здесь`, `Добро пожаловать домой`, `Нулевой пациент`.

This is not yet the final game scripting system. It is a stable vertical slice for narrative, UI, identity scale and choice flow.

## Next Upgrade

The next correct step is to connect story nodes to world triggers:

```text
player enters trigger -> story node opens -> choice updates story flags -> scene state changes
```

Examples:

- mirror trigger opens `restroom_mirror`;
- elevator trigger opens `elevator`;
- home kitchen trigger opens `kitchen`;
- office window trigger spawns the Other;
- final Nadya trigger opens the ending branch.
