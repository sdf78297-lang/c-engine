cmake_minimum_required(VERSION 3.24)

project(ExoEngine
    VERSION 0.1.0
    DESCRIPTION "C++ survival horror engine foundation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXO_BUILD_SANDBOX "Build the engine sandbox application" ON)
option(EXO_BUILD_VALIDATOR "Build the content validation command line tool" ON)
option(EXO_ENABLE_HTML_UI "Enable live HTML/CSS/JS UI through Ultralight" OFF)
option(EXO_ENABLE_AUDIO "Enable SDL2_mixer one-shot and ambience audio" ON)
set(EXO_ULTRALIGHT_SDK "${CMAKE_CURRENT_SOURCE_DIR}/local_deps/ultralight-sdk" CACHE PATH "Path to the Ultralight SDK")

find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG QUIET)
if(EXO_ENABLE_AUDIO)
    find_package(SDL2_mixer CONFIG QUIET)
endif()
find_package(GLEW CONFIG QUIET)

if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

if(NOT GLEW_FOUND)
    find_package(GLEW REQUIRED)
endif()

if(EXO_ENABLE_AUDIO AND NOT SDL2_mixer_FOUND)
    find_package(SDL2_mixer REQUIRED)
endif()

include(cmake/Dependencies.cmake)

file(GLOB_RECURSE EXO_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(FILTER EXO_ENGINE_SOURCES EXCLUDE REGEX ".*/src/Vendor/.*")

add_library(ExoEngine STATIC ${EXO_ENGINE_SOURCES})
add_library(Exo::Engine ALIAS ExoEngine)

target_include_directories(ExoEngine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(ExoEngine
    PUBLIC
        EXO_ENGINE_VERSION="${PROJECT_VERSION}"
        EXO_ENGINE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        EXO_DEBUG_UI_WITH_IMGUI
)

if(EXO_ENABLE_HTML_UI)
    if(NOT EXISTS "${EXO_ULTRALIGHT_SDK}/include/Ultralight/CAPI.h")
        message(FATAL_ERROR "EXO_ENABLE_HTML_UI is ON, but Ultralight SDK was not found at '${EXO_ULTRALIGHT_SDK}'")
    endif()

    target_compile_definitions(ExoEngine PUBLIC EXO_ENABLE_HTML_UI=1)
    target_include_directories(ExoEngine PRIVATE "${EXO_ULTRALIGHT_SDK}/include")
    target_link_libraries(ExoEngine
        PRIVATE
            "${EXO_ULTRALIGHT_SDK}/lib/UltralightCore.lib"
            "${EXO_ULTRALIGHT_SDK}/lib/Ultralight.lib"
            "${EXO_ULTRALIGHT_SDK}/lib/WebCore.lib"
            "${EXO_ULTRALIGHT_SDK}/lib/AppCore.lib"
    )
else()
    target_compile_definitions(ExoEngine PUBLIC EXO_ENABLE_HTML_UI=0)
endif()

if(EXO_ENABLE_AUDIO)
    target_compile_definitions(ExoEngine PUBLIC EXO_ENABLE_AUDIO=1)
else()
    target_compile_definitions(ExoEngine PUBLIC EXO_ENABLE_AUDIO=0)
endif()

if(MSVC)
    target_compile_options(ExoEngine PRIVATE /W4 /permissive-)
else()
    target_compile_options(ExoEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(ExoEngine PRIVATE OpenGL::GL)

if(TARGET SDL2::SDL2)
    target_link_libraries(ExoEngine PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(ExoEngine PRIVATE SDL2::SDL2-static)
else()
    target_include_directories(ExoEngine PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(ExoEngine PRIVATE ${SDL2_LIBRARIES})
endif()

if(EXO_ENABLE_AUDIO)
    if(TARGET SDL2_mixer::SDL2_mixer)
        target_link_libraries(ExoEngine PRIVATE SDL2_mixer::SDL2_mixer)
    elseif(TARGET SDL2_mixer::SDL2_mixer-static)
        target_link_libraries(ExoEngine PRIVATE SDL2_mixer::SDL2_mixer-static)
    else()
        message(FATAL_ERROR "SDL2_mixer target was not provided by find_package")
    endif()
endif()

if(TARGET GLEW::GLEW)
    target_link_libraries(ExoEngine PRIVATE GLEW::GLEW)
else()
    target_include_directories(ExoEngine PRIVATE ${GLEW_INCLUDE_DIRS})
    target_link_libraries(ExoEngine PRIVATE ${GLEW_LIBRARIES})
endif()

target_link_libraries(ExoEngine
    PRIVATE
        Exo::stb_image
        Exo::imgui
        Exo::cgltf
        nlohmann_json::nlohmann_json
)

if(EXO_BUILD_SANDBOX)
    add_executable(ExoSandbox apps/sandbox/main.cpp)
    target_link_libraries(ExoSandbox PRIVATE Exo::Engine)
    set_target_properties(ExoSandbox PROPERTIES OUTPUT_NAME "exo_sandbox")
    if(EXO_ENABLE_HTML_UI)
        add_custom_command(TARGET ExoSandbox POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${EXO_ULTRALIGHT_SDK}/bin" "$<TARGET_FILE_DIR:ExoSandbox>"
        )
    endif()
    if(WIN32)
        add_executable(ExoSandboxWindowed WIN32 apps/sandbox/main.cpp)
        target_link_libraries(ExoSandboxWindowed PRIVATE Exo::Engine)
        set_target_properties(ExoSandboxWindowed PROPERTIES OUTPUT_NAME "exo_sandbox_windowed")
        if(EXO_ENABLE_HTML_UI)
            add_custom_command(TARGET ExoSandboxWindowed POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${EXO_ULTRALIGHT_SDK}/bin" "$<TARGET_FILE_DIR:ExoSandboxWindowed>"
            )
        endif()
    endif()
endif()

if(EXO_BUILD_VALIDATOR)
    add_executable(ExoContentValidator apps/validator/main.cpp)
    target_compile_definitions(ExoContentValidator PRIVATE EXO_ENGINE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(ExoContentValidator PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(ExoContentValidator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set_target_properties(ExoContentValidator PROPERTIES OUTPUT_NAME "exo_validate_content")
endif()
