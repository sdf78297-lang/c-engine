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

find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG QUIET)
find_package(GLEW CONFIG QUIET)

if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

if(NOT GLEW_FOUND)
    find_package(GLEW REQUIRED)
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
        nlohmann_json::nlohmann_json
)

if(EXO_BUILD_SANDBOX)
    add_executable(ExoSandbox apps/sandbox/main.cpp)
    target_link_libraries(ExoSandbox PRIVATE Exo::Engine)
    set_target_properties(ExoSandbox PROPERTIES OUTPUT_NAME "exo_sandbox")
endif()
