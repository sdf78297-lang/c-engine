# Asset and Scene Validation

`AssetValidator` is an offline-facing validation layer for imported mesh data and
authored scenes. It does not mutate runtime objects and is not wired into the
application loop.

## Public API

- `include/ExoEngine/Assets/AssetValidator.h` exposes `AssetValidator`,
  `AssetValidationReport`, `AssetValidationDiagnostic`,
  `AssetValidationSeverity` and `AssetValidationCode`.
- `src/Assets/AssetValidator.cpp` validates `MeshAsset`, `Scene` or both in one
  call.

Use:

```cpp
Exo::AssetValidator validator;
Exo::AssetValidationReport report = validator.validate(mesh);

if (!report.passed()) {
    for (const Exo::AssetValidationDiagnostic& diagnostic : report.diagnostics) {
        emitDiagnostic(diagnostic.path, diagnostic.code, diagnostic.message);
    }
}
```

## Mesh Rules

Mesh validation reports:

- empty render data when vertices or indices are missing;
- invalid mesh or submesh bounds when bounds were never built, are inverted or
  contain non-finite values;
- missing normals when `MeshVertex::hasNormal` is false;
- missing UVs when `MeshVertex::hasTexCoord` is false;
- missing submeshes;
- invalid submesh index ranges when a range is empty, not triangle-aligned,
  outside the index buffer or references a vertex outside the vertex buffer;
- missing material and submesh material names.

Normals, UVs and material names are warnings by default because the importer can
produce fallback data. Empty geometry, invalid bounds and invalid index ranges
are errors.

## Scene Rules

Scene validation reports:

- static mesh instances without `meshAsset`;
- static mesh instances without `materialAsset`;
- scenes with no fixed cameras;
- invalid point lights: non-finite positions or colors, negative colors,
  non-positive radius, negative intensity;
- invalid camera activation bounds: inverted or non-finite bounds;
- invalid camera parameters: non-finite camera vectors, FOV outside 1-179
  degrees, invalid near/far planes or negative blend time.

`materialAsset` diagnostics are warnings by default to keep compatibility with
the current scene format, where the field can be omitted. Production room
packages should still name materials explicitly so tooling can audit the full
content graph.

## Options

`AssetValidationOptions` lets offline tools relax checks while importing rough
work-in-progress content:

- `requireNormals`;
- `requireTexCoords`;
- `requireSubmeshMaterialNames`;
- `requireStaticMeshMaterialNames`;
- `requireCameras`.

The default options are strict enough for authored room packages while avoiding
runtime behavior changes.
