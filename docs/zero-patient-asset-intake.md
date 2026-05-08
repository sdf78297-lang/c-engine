# Zero Patient Asset Intake

This is the current intake map for assets found on the desktop. Do not import the whole folder blindly: the set is close to 800 MB and contains duplicated Meshy exports.

## Desktop Sources

Main folder:

```text
C:\Users\Vladimir\Desktop\текстуры
```

Additional folder:

```text
C:\Users\Vladimir\Desktop\текстры 2
```

Archives:

```text
C:\Users\Vladimir\Desktop\текстуры.zip
C:\Users\Vladimir\Desktop\exo1.zip
C:\Users\Vladimir\Desktop\с3.zip
```

## Useful Runtime Candidates

| Asset | Source | Runtime role | Notes |
| --- | --- | --- | --- |
| CRT television | `Meshy_AI_Old_1990s_CRT_televis_0506115543_texture.glb` | source reference only | Already copied as `samples/tv.glb`; very heavy at ~1.77M triangles. Runtime office scene uses the low-poly authored proxy `assets/props/office_tv.obj` instead. |
| Office table | `Meshy_AI_Realistic_3D_model_of_0508101102_texture.glb` | office workstation prop | Imported as `assets/props/office_table_meshy.glb`; used 8 times in `data/rooms/office/room.json`. Renderer caches it as one loaded mesh. |
| Office PC | `Meshy_AI_Realistic_3D_model_of_0508101516_texture.glb` | office workstation prop | Imported as `assets/props/office_pc_meshy.glb`; used 8 times in `data/rooms/office/room.json`. Renderer caches it as one loaded mesh. |
| Old institution door | `Meshy_AI_A_single_old_institut_0507142921_texture.glb` | office corridor / apartment threshold | Already copied as `samples/door.glb`; pivot is centered, scene raises it by `0.951` on Y. |
| Abandoned psychiatric room | `Meshy_AI_Abandoned_psychiatric_*_texture.glb` | clinic/late-game room candidate | Choose one variant only after visual review. |
| Medical deprivation object | `Meshy_AI_Medical_sensory_depri_*_texture.glb` | clinic prop | Keep as candidate, not core office prop. |
| Tall hospital cabinet | `Meshy_AI_Tall_1990s_hospital_a_*_texture.glb` | office/archive prop | Good for office dressing if scale is fixed. |
| Faceless humanoid | `Meshy_AI_tall_faceless_humanoi_0505223330_texture.glb` | the Other / silhouette | Imported as `assets/characters/other_anton/other_anton.glb`. |
| Shadow FBX | `new_shadow.fbx`, `shadow walk.fbx` | animation source | Convert to GLB in tools; do not load FBX in runtime. |
| Seamless PBR albedo | `c3.zip/..._texture.png` | temporary wall/floor material | Imported as `assets/textures/zero_patient/worn_institution_albedo.png`. |

## Git Policy

Large binary game assets must use Git LFS. `.gitattributes` now tracks:

```text
*.glb
*.gltf
*.fbx
*.blend
*.ktx2
*.tga
assets/**/*.png
assets/**/*.jpg
assets/**/*.jpeg
assets/**/*.webp
*.psd
*.exr
```

Do not add every desktop asset. Import only a selected, named runtime candidate.

## Quality Rules

Keep the source quality intact during intake:

- do not resize embedded textures during the first pass;
- keep GLB as the runtime format for static props;
- convert FBX to GLB offline before runtime use;
- preserve original source files outside the runtime package;
- document scale, pivot and rotation fixes in the scene manifest;
- optimize later with a controlled pass, not by destructive edits.

## Current Technical Risks

- `tv.glb` is far too dense for production and needs decimation/LOD later.
- `door.glb` has a centered pivot; `reference_scene.json` currently compensates with Y `0.951`.
- OBJ materials now bind `Kd` tint and `map_Kd` albedo through submesh draw ranges.
- The runtime shader still uses embedded GLSL in `Renderer.cpp`; `assets/shaders/world.*` is not the active shader path.

## Next Import Recommendation

The first antagonist proxy has been imported:

```text
assets/characters/other_anton/other_anton.glb
```

Scene instance:

```text
other_anton_proxy
```

Verify the current imported slice with:

```powershell
C:\msys64\mingw64\bin\cmake.exe --build build
.\build\exo_sandbox.exe --headless
.\build\exo_sandbox.exe --frames 1
```
