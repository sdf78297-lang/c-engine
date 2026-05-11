# Performance Optimization: Ambulance and Main Menu

## Root Cause

The ambulance vertical slice was loading several Meshy source GLB files directly at runtime. Some of those files are tens or hundreds of megabytes and contain hundreds of thousands to millions of triangles. That is too expensive for a gameplay scene, especially while the nurse rig is also being animated.

The main menu uses Ultralight HTML UI. Ultralight is useful for rich menus, but the menu should not drive the whole application loop at unrestricted speed while the player is only looking at static UI.

## Immediate Runtime Fix

- The ambulance room now uses optimized gameplay proxy OBJ meshes for stretcher and medical equipment.
- The original high-poly Meshy GLB files remain in the project as source/reference assets, but they are no longer the active runtime mesh sources for the ambulance equipment.
- The nurse stays on the rigged GLB path because she is the story-critical animated character.
- The HTML main menu is paced to 45 FPS in normal interactive runs. Ultralight itself still updates at a lower steady cadence and refreshes immediately after input.

## Current Asset Policy

Runtime rooms should not reference raw generated source assets when those assets exceed the scene budget. Use this target budget for the current engine:

- small prop: under 10k triangles
- medium prop: under 30k triangles
- room-critical character: under 80k triangles until GPU skinning is fully reliable
- full room section: under 150k triangles

High-poly GLB files can stay in `assets/.../imported` as source assets, but the room JSON should point to optimized runtime assets.

## Next Production Step

The proxy meshes are a stabilizing fix, not final art. The next art pass should replace them with authored low-poly GLB assets with baked normal maps and packed textures. That keeps the visual quality close to the Meshy models while preserving frame time.
