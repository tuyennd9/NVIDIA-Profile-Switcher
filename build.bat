@echo off
setlocal

:: Find MSVC environment
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found.
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
    set "VS_DIR=%%i"
)

if not exist "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" (
    echo [ERROR] vcvars64.bat not found in %VS_DIR%
    pause
    exit /b 1
)

call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0"

echo Building NvProfileSwitcher...
cl /EHsc /O2 /utf-8 main.cpp /Fe:NvProfileSwitcher.exe /link user32.lib gdi32.lib comctl32.lib shell32.lib /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup

if %ERRORLEVEL% equ 0 (
    echo [OK] Build successful: NvProfileSwitcher.exe
) else (
    echo [ERROR] Build failed.
)

endlocal
