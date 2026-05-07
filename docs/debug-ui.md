# Debug UI

`Exo::DebugOverlay` is the engine-facing Dear ImGui layer for SDL2 and OpenGL3.
The public API is intentionally small:

```cpp
bool initialize(SDL_Window* window, void* glContext);
void shutdown();
void beginFrame();
void drawEngineOverlay(const Scene& scene, const RenderStats& stats);
void endFrame();
```

The implementation is a no-op until the engine target is compiled with
`EXO_DEBUG_UI_WITH_IMGUI`. This keeps the current engine build valid before
Dear ImGui is wired into CMake.

## CMake wiring

Connect Dear ImGui with SDL2 and OpenGL3 backends, then enable the compile
definition for `ExoEngine`.

```cmake
find_package(imgui CONFIG REQUIRED)

target_compile_definitions(ExoEngine PRIVATE EXO_DEBUG_UI_WITH_IMGUI)
target_link_libraries(ExoEngine PRIVATE imgui::imgui)
```

Some ImGui packages do not compile backend sources into `imgui::imgui`.
In that case add the backend `.cpp` files to `ExoEngine` as well:

```cmake
target_sources(ExoEngine PRIVATE
    path/to/imgui/backends/imgui_impl_sdl2.cpp
    path/to/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ExoEngine PRIVATE
    path/to/imgui
    path/to/imgui/backends
)
```

For vcpkg, install ImGui with SDL2 and OpenGL3 binding features enabled. For
source-vendored ImGui, include `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
`imgui_widgets.cpp`, and the two backend files above.

## Application wiring

Add a `DebugOverlay` member near the renderer lifetime owner:

```cpp
#include <ExoEngine/Debug/DebugOverlay.h>

Exo::DebugOverlay debugOverlay_;
```

Initialize it after the SDL window and OpenGL context exist. If the window class
does not expose its GL context, SDL can provide the current one:

```cpp
debugOverlay_.initialize(window_.nativeHandle(), SDL_GL_GetCurrentContext());
```

Render the overlay after the world pass and before `swapBuffers()`:

```cpp
renderer_.beginFrame(makeCurrentView());
renderer_.drawReferenceRoom();
renderer_.endFrame();

debugOverlay_.beginFrame();
debugOverlay_.drawEngineOverlay(scene_, renderer_.stats());
debugOverlay_.endFrame();

window_.swapBuffers();
```

Shutdown the overlay before the renderer and window destroy OpenGL resources:

```cpp
debugOverlay_.shutdown();
renderer_.shutdown();
window_.destroy();
```

The requested API has no SDL event hook. The current panel is designed for
engine inspection and live metrics; if text input, mouse wheel, or richer editor
controls become necessary, add a small event-forwarding method around
`ImGui_ImplSDL2_ProcessEvent`.
