@echo off
rem Minimal Nostalgia (main2.c) Build & Run Script for MSVC (cl)
rem This script compiles main2.c and runs the animation.

echo ===================================================
echo   Minimal Nostalgia - Compile ^& Run (main2.c)
echo ===================================================

rem 1. Check if cl is available
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] cl MSVC Compiler was not found in the current PATH.
    echo.
    echo Please run this script inside a Visual Studio Developer Command Prompt:
    echo   - "Developer Command Prompt for VS 2022"
    echo   - "x64 Native Tools Command Prompt for VS 2022"
    echo.
    pause
    exit /b 1
)

rem 2. Compile main2.c
echo [INFO] Compiling main2.c...
cl main2.c /utf-8 /I. /D_CRT_SECURE_NO_WARNINGS /O2 /Fe:MinimalNostalgia.exe

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Compilation failed.
    pause
    exit /b 1
)
echo [SUCCESS] Compilation completed successfully!
echo.

rem 3. Run the animation
echo [INFO] Launching MinimalNostalgia.exe...
MinimalNostalgia.exe
