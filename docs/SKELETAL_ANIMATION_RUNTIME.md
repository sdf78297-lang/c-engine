# Skeletal Animation Runtime

## Status

The engine now has a v1 rigged GLB path while preserving the static OBJ/GLB path.

Supported runtime pieces:

- GLB skins, joints, inverse bind matrices, node hierarchy, and animation clips.
- Vertex `JOINTS_0` and `WEIGHTS_0` with up to four influences per vertex.
- CPU-skinned VBO fallback in the OpenGL 3.3 renderer with a 96-joint palette.
- Clip playback through sequence actions: `playAnimation` and `stopAnimation`.
- Simple facial cue fallback through jaw/head/neck bones via `setFacialCue`.
- Static GLB fallback when an actor has no skin or clips.

Not yet supported:

- Production GPU skinning validation. The shader path exists, but the v1 renderer currently uploads CPU-skinned vertices for reliability.
- Morph target rendering.
- Multi-layer animation graphs.
- IK, ragdoll, root motion, or animation retargeting.

## Actor JSON Contract

Rigged actors still live in `staticMeshes`, but opt into animation:

```json
{
  "name": "nurse_actor",
  "meshSource": "assets/characters/nurse_ambulance/nurse_actor_rigged.glb",
  "animationRig": true,
  "animationSet": "data/characters/nurse_ambulance.performance.json",
  "defaultClip": "sit_idle"
}
```

Sequence actions:

```json
{ "type": "playAnimation", "entityId": "nurse_actor", "clipId": "lean_to_patient", "loop": false, "fadeSeconds": 0.2 }
{ "type": "setFacialCue", "entityId": "nurse_actor", "cueId": "nurse_hold_on_brain_bleed", "duration": 7.2, "intensity": 1.0 }
{ "type": "stopAnimation", "entityId": "nurse_actor", "fadeSeconds": 0.2 }
```

## Nurse Asset Requirements

The previous static file `assets/characters/nurse_ambulance/nurse_actor.glb` has been removed from active project content. The ambulance scene now uses `assets/characters/nurse_ambulance/nurse_actor_rigged.glb`, a first-pass generated rig with the required runtime clip names.

For real body and speech animation, export a rigged replacement with:

- one humanoid skeleton;
- `JOINTS_0` and `WEIGHTS_0`;
- clips named exactly:
  - `sit_idle`
  - `lean_to_patient`
  - `look_to_monitor`
  - `brace_during_jolt`
  - `call_driver_urgent`
- optional jaw/head/neck bones for speech fallback;
- optional morph targets for a later facial renderer pass.

Recommended target path:

```text
assets/characters/nurse_ambulance/nurse_actor_rigged.glb
```
