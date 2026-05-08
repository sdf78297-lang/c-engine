# Asset Import Module

The asset module provides a small OBJ/MTL import path for the ExoEngine
survival-horror runtime and future offline pipeline. It uses only the C++20
standard library and engine-local math types.

## Public API

- `include/ExoEngine/Assets/MeshAsset.h` defines `MeshAsset`, render vertices,
  submeshes, bounds and material data.
- `include/ExoEngine/Assets/ObjImporter.h` exposes `ObjImporter`,
  `ObjImportOptions`, `ObjImportResult` and diagnostics.
- `src/Assets/ObjImporter.cpp` implements OBJ geometry parsing and MTL material
  parsing.

Use:

```cpp
Exo::ObjImporter importer;
Exo::ObjImportResult result = importer.importFile("assets/models/room.obj");
```

`ObjImportResult::diagnostics` contains warnings and errors with source paths
and line numbers. `success()` is true only when renderable geometry was imported
without error diagnostics.

## OBJ Support

The importer handles:

- `v`, `vt`, `vn`;
- `f` with positive and negative OBJ indices;
- fan triangulation for faces with more than three vertices;
- `o` and `g` names for submesh grouping;
- `usemtl` material assignment;
- `mtllib`, including multiple libraries in one statement.

The resulting `MeshAsset` stores:

- original OBJ streams in `sourcePositions`, `sourceNormals` and
  `sourceTexCoords`;
- deduplicated render vertices in `vertices`;
- 32-bit triangle indices in `indices`;
- submeshes split by object/group and material;
- mesh bounds and per-submesh bounds.

## MTL Support

MTL loading supports:

- `newmtl`;
- `Ka`, `Kd`, `Ks`, `Ke`;
- `Ns`, `d`, `Tr`, `illum`;
- `map_Kd`, `map_Ks`, `map_d`, `map_Bump` and `bump`.

Duplicate material names inside one MTL file are ignored with a warning.
Texture paths are resolved relative to the MTL file, so room folders can be
moved without breaking material references.

## Runtime Material Binding

The renderer now keeps OBJ submeshes as draw ranges. Each `usemtl` section binds:

- diffuse `Kd` as `uBaseColor`;
- `map_Kd` as the albedo texture when present;
- the white fallback texture when no albedo map exists.

This means reference rooms can carry material color and albedo texture data through
the same `meshSource` path as GLB props instead of being rendered as one flat color.

## Options

`ObjImportOptions` currently includes:

- `generateMissingNormals`: accumulates face normals for vertices without
  authored normals and normalizes them after import;
- `flipV`: flips the V texture coordinate for renderer conventions;
- `scale`: applies import-time uniform scale to render vertices and bounds.

## Diagnostics

The importer reports:

- unreadable OBJ or MTL files;
- malformed positions, texture coordinates, normals and faces;
- invalid position, texture coordinate or normal indices;
- missing normals and UVs;
- geometry without `usemtl` assignments;
- geometry without an `mtllib` statement;
- MTL references that cannot be loaded;
- material names referenced by geometry but missing from loaded MTL files.

Invalid faces are rejected before they mutate the mesh, so a bad polygon does
not leave orphan vertices behind.
