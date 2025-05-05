@echo off
setlocal

REM Check if CMake is installed
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake is not installed or not in the PATH!
    echo Please install CMake and try again.
    exit /b 1
)

REM Default build type
set BUILD_TYPE=release
if not "%1"=="" set BUILD_TYPE=%1

REM Default IVF2 path - relative to the current directory
set DEFAULT_IVF2_PATH=..\ivf2
set IVF2_PATH=%DEFAULT_IVF2_PATH%

REM Check if an IVF2 path was provided as second argument
if not "%2"=="" set IVF2_PATH=%2

REM Convert to absolute path
set IVF2_PATH=%~dp0%IVF2_PATH%

echo.
echo Build configuration:
echo - Build type: %BUILD_TYPE%
echo - IVF2 path: %IVF2_PATH%
echo.

REM Validate IVF2 path
if not exist "%IVF2_PATH%" (
    echo ERROR: IVF2 directory does not exist at: %IVF2_PATH%
    echo Please specify the correct path as the second argument.
    echo Example: build_windows.bat release path\to\ivf2
    exit /b 1
)

if not exist "%IVF2_PATH%\include" (
    echo ERROR: IVF2 include directory not found at: %IVF2_PATH%\include
    exit /b 1
)

if not exist "%IVF2_PATH%\lib" (
    echo ERROR: IVF2 lib directory not found at: %IVF2_PATH%\lib
    exit /b 1
)

REM Check for libraries based on build type
set LIB_SUBDIR=Release
if /i "%BUILD_TYPE%"=="debug" (
    set EXPECTED_LIBRARIES=ivfd.lib ivfuid.lib generatord.lib gladd.lib
    set LIB_SUBDIR=Debug
) else (
    set EXPECTED_LIBRARIES=ivf.lib ivfui.lib generator.lib glad.lib
    set LIB_SUBDIR=Release
)

set MISSING_LIBRARIES=0
for %%L in (%EXPECTED_LIBRARIES%) do (
    if not exist "%IVF2_PATH%\lib\%LIB_SUBDIR%\%%L" (
        echo WARNING: Required library not found: %IVF2_PATH%\lib\%LIB_SUBDIR%\%%L
        set /a MISSING_LIBRARIES+=1
    )
)

if %MISSING_LIBRARIES% GTR 0 (
    echo WARNING: %MISSING_LIBRARIES% required libraries not found!
    echo Make sure you have built the ivf2 library with the correct configuration.
)

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Set build preset based on build type
set CONFIG_PRESET=windows
if /i "%BUILD_TYPE%"=="debug" set CONFIG_PRESET=windows-debug

REM Configure using the specified preset
echo Configuring project with CMake...
cmake --preset %CONFIG_PRESET% -DIVF2_ROOT="%IVF2_PATH%"

if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed!
    exit /b 1
)

REM Build the project
echo Building in %BUILD_TYPE% mode...
cmake --build --preset %BUILD_TYPE%

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed!
    exit /b 1
) else (
    echo Build completed successfully!
    echo You can find the executable in the bin directory.
)

endlocal