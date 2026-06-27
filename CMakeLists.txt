cmake_minimum_required(VERSION 3.24)

project(ivf_app_standalone LANGUAGES C CXX)

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

add_executable(ivf_app ${APP_TYPE}
    src/audio_player.cpp
    src/audio_player.h
    src/ivf_app.cpp
    src/particle_scene.cpp
    src/particle_scene.h
)

target_include_directories(ivf_app PRIVATE
    "${MINIAUDIO_INCLUDE_DIRS}"
)

target_link_libraries(ivf_app PRIVATE
    ivf2::ivf2
    nlohmann_json::nlohmann_json
    assimp::assimp
    ReactPhysics3D::ReactPhysics3D
)

if(MSVC)
    target_link_options(ivf_app PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

set(PARTICLE_ASSET "${IVF2_ROOT}/bin/assets/particle.jpeg")
if(EXISTS "${PARTICLE_ASSET}")
    add_custom_command(TARGET ivf_app POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:ivf_app>/assets"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PARTICLE_ASSET}" "$<TARGET_FILE_DIR:ivf_app>/assets/particle.jpeg"
        VERBATIM
    )
else()
    message(WARNING "Particle asset not found at '${PARTICLE_ASSET}'. The example will run without its particle texture.")
endif()
