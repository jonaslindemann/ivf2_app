cmake_minimum_required(VERSION 3.24)

project(flow_field_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 

set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

if(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

set(IVF2_ROOT "${PROJECT_SOURCE_DIR}/../ivf2" CACHE PATH "Path to the ivf2 source tree")
get_filename_component(IVF2_ROOT "${IVF2_ROOT}" ABSOLUTE)

if(NOT EXISTS "${IVF2_ROOT}/include")
    message(FATAL_ERROR "ivf2 headers were not found at '${IVF2_ROOT}/include'. Set IVF2_ROOT to the ivf2 tree.")
endif()

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(Freetype REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ReactPhysics3D REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h" REQUIRED)

if(APPLE OR UNIX)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(_DEBUG)
    endif()
endif()

add_subdirectory("${IVF2_ROOT}" "${CMAKE_BINARY_DIR}/ivf2-build" EXCLUDE_FROM_ALL)

if(WIN32)
    set(APP_TYPE WIN32)
else()
    set(APP_TYPE "")
endif()

add_executable(flow_field_demo ${APP_TYPE}
    src/audio_player.cpp
    src/audio_player.h
    src/creative_scene.h
    src/creative_app.cpp
    src/creative_app.h
    src/flow_field_demo.cpp
    src/particle_scene.cpp
    src/particle_scene.h
    src/tunnel_scene.cpp
    src/tunnel_scene.h
    src/lorenz_scene.cpp
    src/lorenz_scene.h
    src/lattice_scene.cpp
    src/lattice_scene.h
    src/midi_binder.cpp
    src/midi_binder.h
    src/control_center_window.cpp
    src/control_center_window.h
    src/scene_controls_window.cpp
    src/scene_controls_window.h
)

target_include_directories(flow_field_demo PRIVATE
    "${MINIAUDIO_INCLUDE_DIRS}"
)

target_link_libraries(flow_field_demo PRIVATE
    ivf2::ivf2
    nlohmann_json::nlohmann_json
    assimp::assimp
    ReactPhysics3D::ReactPhysics3D
)

if(MSVC)
    target_link_options(flow_field_demo PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

# Assets live once at the repository root and are copied next to the executable at build time.
# $<TARGET_FILE_DIR:...> resolves to bin/Release or bin/Debug for multi-config generators (MSVC)
# and to bin for single-config generators (Makefiles/Ninja on macOS/Linux).
set(ASSETS_DIR "${PROJECT_SOURCE_DIR}/assets")
if(EXISTS "${ASSETS_DIR}")
    add_custom_command(TARGET flow_field_demo POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${ASSETS_DIR}" "$<TARGET_FILE_DIR:flow_field_demo>/assets"
        COMMENT "Copying assets to $<TARGET_FILE_DIR:flow_field_demo>/assets"
        VERBATIM
    )
else()
    message(WARNING "Assets directory not found at '${ASSETS_DIR}'. The example will run without its textures/audio.")
endif()
