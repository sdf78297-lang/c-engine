# Mesh Runtime Contract

`MeshBuffer` is the GPU-side runtime representation of `MeshAsset`.
It owns one VAO, one vertex buffer, and one unsigned-int index buffer.

## Upload Source

`MeshBuffer::upload(const MeshAsset&)` expects the already-imported mesh produced by
the asset pipeline or OBJ importer. The CPU mesh remains the source of truth for:

- `vertices`
- `indices`
- `bounds`
- `sourcePath` / `name`

The upload rejects empty meshes, invalid indices, and index counts that cannot be
issued through a single OpenGL 3.3 `glDrawElements` call.

## Vertex Layout

The VAO uses this layout:

| Location | Data | Type |
| ---: | --- | --- |
| 0 | position | `vec3` |
| 1 | texCoord | `vec2` |
| 2 | normal | `vec3` |
| 3 | flags: `hasNormal`, `hasTexCoord` | `vec2` |

Locations 0-2 match `assets/shaders/world.vert`.
Location 3 gives future shaders a direct way to handle imperfect imported assets
without guessing whether zero UVs or zero normals were authored deliberately.

## Missing Attributes

When a `MeshVertex` has `hasNormal == false`, the uploaded normal is `{0, 0, 0}` and
the first flag component is `0`. When `hasTexCoord == false`, the uploaded UV is
`{0, 0}` and the second flag component is `0`.

Missing normals or UVs are accepted so debug and tooling paths can inspect the
asset, but the importer and production validation should still treat them as asset
quality issues for render meshes.

## Draw

`MeshBuffer::draw()` binds the VAO and issues:

```cpp
glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
```

The caller is responsible for binding the shader, material textures, uniforms, and
render state before calling `draw()`.
