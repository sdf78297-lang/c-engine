# ExoEngine

ExoEngine is the new C++ foundation for a fixed-camera survival horror game.
The old game content has been removed; this repository now keeps the engine
runtime, renderer, scene layer, and asset pipeline as separate systems.

## Current Scope

- C++20 engine library with a sandbox executable.
- SDL2 windowing and OpenGL rendering.
- Fixed-camera scene foundation for classic survival horror staging.
- OBJ/MTL import pipeline prepared for high-quality 3D assets.
- JSON scene loading for rooms, lights, meshes and fixed cameras.
- Texture loading through `stb_image`.
- Dear ImGui debug overlay for engine/runtime inspection.
- Runtime assets separated from game-specific content.

## Dependencies

The project uses CMake `FetchContent` for engine-side libraries:

- `nlohmann/json` for scene and data files.
- `stb_image` for PNG/TGA/JPG texture loading.
- `Dear ImGui` for debug UI.

The dependency revisions are pinned in `cmake/Dependencies.cmake` so builds do not drift when upstream repositories change.

## Build

On this machine the working toolchain is MSYS2 MinGW:

```powershell
C:\msys64\mingw64\bin\cmake.exe -S . -B build -G Ninja
C:\msys64\mingw64\bin\cmake.exe --build build
```

Run the headless project check:

```powershell
.\build\exo_sandbox.exe --headless
```

Run the render sandbox:

```powershell
.\build\exo_sandbox.exe
```

For a short OpenGL smoke test that closes automatically:

```powershell
.\build\exo_sandbox.exe --frames 1
```

## Asset Direction

The engine is being prepared around source-quality assets: OBJ/MTL meshes,
stable material names, authored normals, authored UVs, and lossless texture
handoff. The game concept can be added later without mixing story content into
the engine layer.
