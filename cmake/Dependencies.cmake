include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42F6E95CAD6EC532FD372391373363B62A14AF6D771056DBFC86160E6DFFF7AA
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
    SOURCE_SUBDIR cmake-do-not-add
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 976c5c0f3a77e20c79d5407efcf94bc9047ee8cf
    SOURCE_SUBDIR cmake-do-not-add
)

FetchContent_MakeAvailable(nlohmann_json stb imgui)

add_library(exo_stb_image STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Vendor/stb_image.cpp"
)

target_include_directories(exo_stb_image
    PUBLIC
        "${stb_SOURCE_DIR}"
)

add_library(Exo::stb_image ALIAS exo_stb_image)

add_library(exo_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
)

target_include_directories(exo_imgui
    PUBLIC
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
)

if(TARGET SDL2::SDL2)
    target_link_libraries(exo_imgui PUBLIC SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(exo_imgui PUBLIC SDL2::SDL2-static)
else()
    target_include_directories(exo_imgui PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(exo_imgui PUBLIC ${SDL2_LIBRARIES})
endif()

target_link_libraries(exo_imgui PUBLIC OpenGL::GL)

add_library(Exo::imgui ALIAS exo_imgui)
