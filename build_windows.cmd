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
    echo Example: build_windows.cmd release path\to\ivf2
    exit /b 1
)

if not exist "%IVF2_PATH%\CMakeLists.txt" (
    echo ERROR: IVF2 CMakeLists.txt not found at: %IVF2_PATH%\CMakeLists.txt
    exit /b 1
)

if not exist "%IVF2_PATH%\lib\Release" (
    echo ERROR: IVF2 release library directory not found at: %IVF2_PATH%\lib\Release
    exit /b 1
)

if not exist "%IVF2_PATH%\lib\Debug" (
    echo ERROR: IVF2 debug library directory not found at: %IVF2_PATH%\lib\Debug
    exit /b 1
)

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
