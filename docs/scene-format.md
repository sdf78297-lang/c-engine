# Scene JSON Format

Scene files describe one fixed-camera survival-horror room. The runtime loader lives in
`ExoEngine/Scene/SceneLoader.h` and parses JSON through `nlohmann/json.hpp`.

## Root Object

```json
{
  "name": "engine_reference_room",
  "staticMeshes": [],
  "pointLights": [],
  "fixedCameras": []
}
```

`name` is required and must be a non-empty string. The arrays are optional; an omitted
array is treated as empty. Unknown fields are ignored so authoring tools can carry
editor-only metadata without breaking runtime loading.

## Coordinates

All vectors are numeric arrays in engine XYZ order:

```json
[0.0, 1.8, 0.7]
```

Camera bounds are axis-aligned world-space boxes:

```json
{
  "min": [-3.0, -0.5, -3.0],
  "max": [3.0, 2.5, 3.0]
}
```

`min` must be less than or equal to `max` on every axis.

## Static Meshes

```json
{
  "name": "reference_room",
  "meshAsset": "engine/reference_room.mesh",
  "materialAsset": "engine/reference_room.material",
  "transform": {
    "position": [0.0, 0.0, 0.0],
    "rotation": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0]
  }
}
```

`name` and `meshAsset` are required. `materialAsset` is optional and defaults to an
empty string. `transform` is optional; missing transform values default to identity.
Scale components must be greater than zero.

## Point Lights

```json
{
  "position": [0.0, 1.8, 0.7],
  "color": [1.0, 0.78, 0.52],
  "radius": 5.5,
  "intensity": 1.0
}
```

`position` is required. `color`, `radius` and `intensity` are optional and use the
engine `PointLight` defaults when omitted. Color components and intensity cannot be
negative. Radius must be greater than zero.

## Fixed Cameras

```json
{
  "name": "entry_angle",
  "bounds": {
    "min": [-3.0, -0.5, -3.0],
    "max": [3.0, 2.5, 3.0]
  },
  "position": [4.6, 2.35, 5.2],
  "target": [0.0, 0.85, 0.0],
  "fovDeg": 45.0,
  "near": 0.05,
  "far": 80.0,
  "priority": 10,
  "blendSeconds": 0.0
}
```

Every fixed camera field shown above is required. `fovDeg` is authored in degrees and
converted to radians for `FixedCameraShot`. `near` must be greater than zero, `far`
must be greater than `near`, and `blendSeconds` cannot be negative. Higher priority
cameras win when activation bounds overlap.

See `samples/reference_scene.json` for a complete reference room manifest.
