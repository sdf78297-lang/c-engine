# Ambulance Nurse Animation Setup

## Purpose

This document defines the tech-art contract for the ambulance nurse asset. The previous static Meshy GLB has been removed from active room content and replaced by a first-pass generated rig.

Current runtime state:

- `assets/characters/nurse_ambulance/nurse_actor_rigged.glb` is loaded in the ambulance room.
- Nurse voice cues are bound to skeletal animation and broad staging transforms.
- The engine has a v1 rigged GLB path for skins, joints, and clips.

Future animation target:

- Keep the same actor id: `nurse_actor`.
- Add a rigged GLB when the replacement character is ready.
- Preserve the clip names listed in this document and in `data/characters/nurse_ambulance.performance.json`.

## Static GLB vs Rigged GLB

### Static GLB

A static GLB contains renderable geometry and materials only. It may look like a finished character, but it has no usable animation data for the runtime.

Expected contents:

- mesh geometry;
- UVs and normals;
- material assignments and textures;
- applied scale and rotation;
- no required skeleton;
- no required animation clips;
- no required facial morph targets.

Runtime behavior:

- The whole object can be moved, rotated, and scaled by scene or sequence data.
- The current ambulance nurse performance uses this mode.
- Body lean, emergency motion, and speech energy are simulated by object transform and procedural offsets.
- Mouth, eyes, fingers, and cloth cannot animate independently.

Use a static GLB when:

- the character is a temporary or background figure;
- the scene only needs broad staging;
- the engine is not ready for skeletal animation;
- Meshy generated a good visual model but no reliable rig.

### Rigged GLB

A rigged GLB contains a skinned mesh bound to a skeleton, plus named animation clips. This is the required format for a believable close character performance.

Expected contents:

- one main armature/skeleton;
- skinned body mesh with stable weights;
- named animation clips stored in the GLB animation list;
- optional facial blendshapes/morph targets for mouth and emotion;
- applied object transforms before export;
- clean bind pose and no duplicate helper rigs in the exported file.

Runtime behavior after engine support:

- the engine loads the same character asset slot;
- animation clips are selected by exact clip id;
- body clips can be blended by performance data;
- facial morph targets can be sampled from the viseme timeline;
- object-level sequence transforms remain useful for staging and camera composition.

Use a rigged GLB when:

- the nurse must speak on camera;
- the player can read head, shoulder, hand, and mouth motion;
- the scene needs professional acting instead of whole-object motion;
- the same character will appear in later rooms or sequences.

## Required Nurse Clips

The engine-side performance data expects these exact lowercase clip ids. Do not rename them in Blender, Meshy, or post-export tools.

| Clip name | Purpose | Acting direction | Looping |
| --- | --- | --- | --- |
| `sit_idle` | Baseline seated pose in the moving ambulance. | Stable seated posture, controlled breathing, slight bracing from vehicle motion. | Loop |
| `lean_to_patient` | Nurse leans toward Anton while speaking. | Upper body and head move closer to the patient/camera, professional urgency without melodrama. | One-shot or blendable hold |
| `look_to_monitor` | Nurse checks the patient monitor. | Head and eyes turn away briefly; torso stays controlled so she still feels present beside the patient. | One-shot |
| `brace_during_jolt` | Nurse reacts to ambulance motion. | One hand or shoulder braces against stretcher, rail, or cabinet; short impact response. | One-shot |
| `call_driver_urgent` | Nurse turns toward the cabin and calls out. | Head and torso rotate toward driver/front cabin; urgency rises for "We're losing him." | One-shot |

Minimum delivery for a first rigged pass:

1. `sit_idle`
2. `lean_to_patient`
3. `call_driver_urgent`

Full delivery for the ambulance sequence:

1. `sit_idle`
2. `lean_to_patient`
3. `look_to_monitor`
4. `brace_during_jolt`
5. `call_driver_urgent`

Optional facial morph targets for later lip-sync:

- `mouth_closed`
- `mouth_open_small`
- `mouth_open_medium`
- `mouth_open_wide`
- `mouth_round_o`
- `mouth_fv`
- `mouth_l`
- `mouth_mbp`
- `jaw_clench`

## Expected Asset Names

Keep file and data names stable:

- actor id: `nurse_actor`
- future visual asset: `assets/characters/nurse_ambulance/nurse_actor_rigged.glb`
- performance data: `data/characters/nurse_ambulance.performance.json`
- first voice cue: `nurse_hold_on_brain_bleed`
- second voice cue: `nurse_losing_him`

Expected animation clip names:

```text
sit_idle
lean_to_patient
look_to_monitor
brace_during_jolt
call_driver_urgent
```

Do not use display names such as `Idle`, `Take 001`, `mixamo.com`, `ArmatureAction`, `Nurse Lean`, or `final_animation`. The runtime and validation path should treat clip ids as exact technical ids.

## Meshy Export Guidance

Meshy is useful for fast concept and textured character generation, but its output must be reviewed before it becomes a production character.

For the current static nurse:

1. Export GLB with textures embedded or stored in a predictable local texture set.
2. Keep the model visually close to a real emergency nurse: medical clothing, readable silhouette, no fantasy accessories.
3. Avoid extreme baked lighting in albedo textures; the ambulance scene lighting must still control the mood.
4. Check that the model stands or sits at a believable human scale before importing into Blender.
5. Treat the Meshy GLB as visual source only if it has no skeleton, clips, or morph targets.

For a future rigged nurse:

1. Prefer a Meshy export that includes a clean humanoid body without fused arms, broken hands, or distorted face topology.
2. Do not trust generated rig names or clip names as final.
3. Bring the GLB into Blender for cleanup, scale correction, rig validation, and animation naming.
4. If Meshy provides animation, rename each exported action to the exact clip ids above.
5. If Meshy cannot produce reliable deformation, use it as a retopology/texture reference and rig the cleaned model in Blender.

Reject or rework the asset if:

- hands are unreadable in first-person camera angles;
- the face collapses during mouth shapes;
- the shoulders deform badly during `lean_to_patient`;
- the mesh has large hidden parts or excessive triangle count;
- material names and texture paths are random or unstable.

## Blender Export Guidance

Use Blender as the final authority before the runtime GLB is committed.

Scene setup:

1. Unit scale: meters.
2. Axis contract: Y is up in runtime expectations; verify export/import orientation in-engine.
3. Character height: roughly 1.65-1.80 meters before seated staging.
4. Apply object scale and rotation before export.
5. Keep the character origin predictable, ideally at floor contact or the authored character root.

Rig setup for a rigged GLB:

1. One main armature.
2. One root bone for whole-body motion.
3. Stable spine, neck, head, clavicle, arm, and hand chains.
4. No unused control rigs, IK widgets, cameras, lights, or helper meshes in the export collection.
5. Mesh weights normalized and checked in the extreme poses used by the ambulance sequence.

Action and clip setup:

1. Put each required motion in its own Blender Action.
2. Name each Action exactly: `sit_idle`, `lean_to_patient`, `look_to_monitor`, `brace_during_jolt`, `call_driver_urgent`.
3. Mark actions with Fake User if needed so they survive file reload.
4. Keep the first and last frame of looping clips compatible.
5. Keep one-shot clips short and readable; avoid long dead frames before the motion starts.

Recommended GLB export settings:

```text
Format: glTF Binary (.glb)
Include: Selected Objects
Transform: Apply Modifiers enabled
Geometry: UVs and Normals enabled
Geometry: Tangents optional until normal maps require them
Animation: enabled for rigged export
Animation Mode: Actions or NLA Tracks, but clip names must remain exact
Skinning: enabled for rigged export
Shape Keys: enabled only if facial morph targets are present
Cameras/Lights: disabled
```

After export, re-import the GLB into a clean Blender scene and verify:

- mesh appears at correct scale;
- textures are assigned;
- skeleton deforms the body correctly;
- all required animation clips are visible by exact name;
- no helper objects, cameras, or hidden test meshes were exported.

## Runtime Acceptance

A static nurse GLB is acceptable for the current ambulance pass when:

- the model renders in `ambulance_patient_compartment`;
- the nurse reads clearly from the patient-camera angle;
- transform keyframes sell the lean, monitor check, and urgent turn;
- there is no implication that mouth animation exists.

A rigged nurse GLB is acceptable for the future animation runtime when:

- all required clip ids are present exactly once;
- `sit_idle` loops without a visible pop;
- one-shot clips start quickly and land in controlled poses;
- scale, orientation, and origin match the current static asset closely enough that room staging does not need to be rewritten;
- facial morph targets, if included, match the expected names and default to neutral at weight 0.

## Current Sequence Notes

The current ambulance sequence uses generic actions:

- `setEntityTransform`
- `animateEntityTransform`
- `clearEntityTransform`
- `startCharacterPerformance`
- `stopCharacterPerformance`

These actions are actor-agnostic and intentionally work with the static GLB. They provide:

- seated staging beside Anton;
- subtle breathing and vehicle sway;
- speech emphasis during nurse voice cues;
- stronger emergency movement for `nurse_losing_him`;
- safe fallback behavior until real skeletal animation is implemented.

Do not modify C++ runtime or `data/rooms/ambulance_patient_compartment/room.json` only to rename clips or stage this asset. The current document is the contract for art delivery; runtime integration should happen in a separate engine task.
