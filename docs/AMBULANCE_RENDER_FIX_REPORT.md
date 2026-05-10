# Ambulance Render Fix Report

## Problem

`ambulance_patient_compartment` could load without showing a readable ambulance because the room file was still a temporary transition hold room. It used a reference room plus cube proxies, not the actual patient compartment composition. It also missed the `name` field required by `SceneLoader`, and the old `loadRoomScene(..., required=false)` path could report success after a scene-load exception, leaving the previous scene active. The runtime also only rendered `staticMeshes` with a valid `meshSource`; missing or unsupported mesh paths could silently leave a scene with no useful geometry.

## Active Room

- Room file: `data/rooms/ambulance_patient_compartment/room.json`
- SceneLoader fields used: `name`, `renderEnvironment`, `staticMeshes`, `pointLights`, `fixedCameras`
- Static meshes: 7
- Point lights: 4
- Fixed cameras: 2
- Primary spawn: `stretcher_head_spawn`
- Lying camera anchor: `stretcher_head_anchor`

## Active Mesh Sources

- `ambulance_interior` -> `assets/rooms/ambulance/imported/ambulance_interior.glb`
- `stretcher` -> `assets/rooms/ambulance/imported/stretcher.glb`
- `medical_cabinet` -> `assets/rooms/ambulance/imported/medical_cabinet.glb`
- `oxygen_cylinders` -> `assets/rooms/ambulance/imported/oxygen_cylinders.glb`
- `patient_monitor` -> `assets/rooms/ambulance/imported/patient_monitor.glb`
- `rear_doors_inside` -> `assets/rooms/ambulance/imported/rear_doors.glb`
- `medical_bag` -> `assets/rooms/ambulance/imported/medical_bag.glb`

## Real Meshy Assets Found

These Meshy GLB files were copied into `assets/rooms/ambulance/imported/` and are active in the room:

- `C:/Users/Vladimir/Desktop/Meshy_AI_Realistic_inside_view_0510090511_texture.glb` -> `ambulance_interior.glb`
- `C:/Users/Vladimir/Downloads/Meshy_AI_Realistic_foldable_si_0510085914_texture.glb` -> `stretcher.glb`
- `C:/Users/Vladimir/Downloads/Meshy_AI_Realistic_oxygen_cyli_0510085919_texture.glb` -> `oxygen_cylinders.glb`
- `C:/Users/Vladimir/Downloads/Meshy_AI_Realistic_portable_pa_0510085924_texture.glb` -> `patient_monitor.glb`
- `C:/Users/Vladimir/Downloads/Meshy_AI_Realistic_ambulance_c_0510085933_texture.glb` -> `medical_cabinet.glb`
- `C:/Users/Vladimir/Desktop/Meshy_AI_Realistic_ambulance_d_0510090111_texture.glb` -> `rear_doors.glb`
- `C:/Users/Vladimir/Desktop/Meshy_AI_Realistic_red_emergen_0510090100_texture.glb` -> `medical_bag.glb`

## Runtime Safety

Room loading now logs:

- absolute room JSON path;
- loaded scene name and room id;
- `staticMeshes`, `pointLights`, `fixedCameras` counts;
- every static mesh name, `meshSource`, and path existence.

If a `meshSource` is missing or fails to load, the runtime loads `assets/debug/debug_missing_mesh.obj` instead of silently drawing nothing.

Scene-load exceptions now return failure instead of pretending that the transition succeeded.

## Smoke Test

Run these from the project root:

```powershell
C:\msys64\mingw64\bin\cmake.exe --build build
.\build\exo_validate_content.exe
.\build\exo_sandbox.exe --room ambulance_patient_compartment --frames 1
.\build\exo_sandbox.exe --headless
```

Expected smoke result: the ambulance room shows the imported Meshy interior, stretcher, medical cabinet, oxygen cylinders, patient monitor, rear doors, and medical bag. If one GLB fails, the visible debug fallback mesh replaces only that missing object.
