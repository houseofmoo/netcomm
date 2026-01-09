@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem Usage:
rem   build.bat [--debug | --release] [--clean] [--run]
rem ------------------------------------------------------------
rem Examples:
rem   build.bat --debug
rem   build.bat --release --clean
rem ------------------------------------------------------------
rem Manual cmake build commands:
rem     cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
rem     cmake --build build --parallel
rem ============================================================

set PRESET=debug
set BUILD_DIR=build
set CLEAN=false
set RUN_AFTER_BUILD=false
set PROFILE=false

rem Clean screen
cls

:parse_args
    if "%~1"=="" goto after_args

    if /I "%~1"=="--run" (
        set RUN_AFTER_BUILD=true
    ) else if /I "%~1"=="--debug" (
        set PRESET=debug
    ) else if /I "%~1"=="--release" (
        set PRESET=release
    ) else if /I "%~1"=="--clean" (
        set CLEAN=true
    ) else (
        echo Unknown option: %~1
        echo Usage: build.bat [--debug^|--release] [--clean] [--install]
        exit /b 1
    )
    shift
    goto parse_args
:after_args

rem Optional clean
if "%CLEAN%"=="true" (
    if exist "%BUILD_DIR%" (
        echo Cleaning build directory "%BUILD_DIR%"...
        rmdir /s /q "%BUILD_DIR%"
        if errorlevel 1 (
            echo Failed to remove build directory
            exit /b 1
        )
    ) else (
        echo Build directory "%BUILD_DIR%" does not exist, skipping clean
    )
)

echo.
echo Running CMake configure preset "%PRESET%"
cmake --preset %PRESET% -DPROFILER_ENABLED=%PROFILE%
REM cmake --preset msvc-release

if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

echo.
echo Building project for preset "%PRESET%"
cmake --build --preset %PRESET% --parallel
REM cmake --build --preset release --config Release

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo.
echo Build completed successfully for preset "%PRESET%"

if "%RUN_AFTER_BUILD%"=="true" (
    echo.
    echo Running the built executable...
    call run.bat
    if errorlevel 1 (
        echo Failed to run the built executable
        exit /b 1
    )
)

endlocal