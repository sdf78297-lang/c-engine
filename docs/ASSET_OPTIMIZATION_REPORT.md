# Asset Optimization Report

## Ambulance Runtime Snapshot

The ambulance patient compartment no longer loads the heavy Meshy equipment GLBs at runtime.
Those files stay in `assets/rooms/ambulance/imported` as source/reference content.

| Active asset | Format | Size | Tris | Material draw ranges | Runtime role |
|---|---:|---:|---:|---:|---|
| `assets/rooms/ambulance/clean/ambulance_runtime_shell.obj` | OBJ | 46.34 KB | 204 | 9 | ambulance shell, rails, doors, benches, ceiling lights |
| `assets/rooms/ambulance/clean/ambulance_patient_stretcher_runtime.obj` | OBJ | 38.62 KB | 294 | 5 | separate stretcher, blanket, straps, pillow, rails and wheels |
| `assets/rooms/ambulance/clean/ambulance_medical_cabinet.obj` | OBJ | 13.34 KB | 66 | 5 | side medical cabinet with doors, handles and labels |
| `assets/rooms/ambulance/clean/ambulance_equipment_rack.obj` | OBJ | 23.51 KB | 110 | 5 | equipment rack, defib blocks, status screen and cable coil |
| `assets/rooms/ambulance/clean/ambulance_patient_monitor.obj` | OBJ | 13.50 KB | 60 | 3 | separate monitor prop with emissive override |
| `assets/rooms/ambulance/clean/ambulance_oxygen_cylinders.obj` | OBJ | 26.13 KB | 112 | 4 | oxygen cylinders with valves and straps |
| `assets/rooms/ambulance/clean/ambulance_medical_bag.obj` | OBJ | 9.62 KB | 48 | 3 | red medical bag with handle and cross marking |
| `assets/rooms/ambulance/clean/ambulance_iv_stand.obj` | OBJ | 24.91 KB | 108 | 4 | IV stand, fluid bag, hooks and caster feet |
| `assets/characters/nurse_ambulance/nurse_actor_rigged.glb` | GLB | 37.47 MB | 234578 | n/a | story-critical animated nurse |

Runtime ambulance static OBJ geometry is now about 195 KB and about 1000 triangles, excluding the animated nurse. The prop set stays separated by object so the vehicle reads as a real medical compartment instead of a baked blockout.

## Removed From Active Room Load

| Source GLB kept for reference | Size |
|---|---:|
| `assets/rooms/ambulance/imported/patient_stretcher_modern.glb` | 58.82 MB |
| `assets/rooms/ambulance/imported/medical_cabinet.glb` | 31.30 MB |
| `assets/rooms/ambulance/imported/medical_equipment.glb` | 75.95 MB |
| `assets/rooms/ambulance/imported/patient_monitor.glb` | 56.56 MB |
| `assets/rooms/ambulance/imported/oxygen_cylinders.glb` | 67.72 MB |
| `assets/rooms/ambulance/imported/medical_bag.glb` | 44.28 MB |
| `assets/rooms/ambulance/imported/iv_stand.glb` | 21.56 MB |

Previous active static equipment payload removed from scene load: about 356.18 MB.

## Optimization Notes

- `ambulance_runtime_shell.obj` keeps the authored tight ambulance compartment but collapses the previous face-per-object material layout into 9 material ranges.
- Stationary medical props use separate clean OBJ runtime LODs. This preserves individual objects for staging, lighting and future interaction while avoiding the 356 MB Meshy GLB payload.
- `ambulance_patient_monitor.obj` remains separate so the scene can keep the green monitor emissive feedback.
- Point lights were reduced from 5 to 4 and tightened to the playable compartment volume.
- Fixed camera far planes were reduced from 80 to 12 for the small vehicle interior.
- Ambulance ride staging now uses `FixedCinematic` camera mode, so the player watches a controlled in-vehicle scene instead of driving the camera during the heaviest sequence.

## Remaining Risk

The animated nurse GLB is still the largest active asset. It is story-critical and remains on the rigged GLB path until a production character LOD or optimized rig is authored.
