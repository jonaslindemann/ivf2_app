cmake_minimum_required(VERSION 3.24)

project(ivf_app_standalone)

enable_language(CXX)
enable_language(C)
set(CMAKE_CXX_STANDARD 20)

# Set output directories
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# Enable solution folders for Visual Studio
IF(MSVC)
  SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
ENDIF()

# Find required packages using vcpkg
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(Freetype REQUIRED)
find_package(Assimp CONFIG REQUIRED)

# Define debug macro for non-Windows platforms
if (APPLE OR UNIX)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_compile_definitions(_DEBUG)
	endif()
endif()

# Include directories
include_directories(
    # Include directories for ivf2 library
    ${IVF2_ROOT}/include
    
    # External dependencies
    ${GLM_DIR}
    ${GENERATOR_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${STB_DIR}
    ${Stb_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
)

# Link directories for ivf2 library
if(WIN32)
    link_directories(${IVF2_ROOT}/lib/${CMAKE_BUILD_TYPE})
else()
    link_directories(${IVF2_ROOT}/lib)
endif()

# Add executable
# Platform-specific configurations
if(WIN32)
    set(APP_TYPE WIN32)
    set(SUBSYSTEM_OPTION "/SUBSYSTEM:WINDOWS")
    set(ENTRY_POINT "/ENTRY:mainCRTStartup")
else()
    set(APP_TYPE "")
    set(SUBSYSTEM_OPTION "")
    set(ENTRY_POINT "")
endif()

# Check for build type and set appropriate target names and libraries
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(ivf_app ${APP_TYPE} src/ivf_app.cpp)
    
    # For Windows, we need to define the entry point
    if(MSVC)
        target_link_options(ivf_app PRIVATE ${SUBSYSTEM_OPTION} ${ENTRY_POINT})
    endif()
    
    # Link with debug libraries
    target_link_libraries(ivf_app PRIVATE 
        ivfd ivfuid generatord gladd
        glfw imgui::imgui PNG::PNG JPEG::JPEG ZLIB::ZLIB
        GLEW::GLEW OpenGL::GL
        assimp::assimp
    )
else()
    add_executable(ivf_app ${APP_TYPE} src/ivf_app.cpp)
    
    # For Windows, we need to define the entry point
    if(MSVC)
        target_link_options(ivf_app PRIVATE ${SUBSYSTEM_OPTION} ${ENTRY_POINT})
    endif()
    
    # Link with release libraries
    target_link_libraries(ivf_app PRIVATE 
        ivf ivfui generator glad
        glfw imgui::imgui PNG::PNG JPEG::JPEG ZLIB::ZLIB
        GLEW::GLEW OpenGL::GL
        assimp::assimp
    )
endif()

# Include directories specifically for the executable
target_include_directories(ivf_app PRIVATE ${Stb_INCLUDE_DIR})