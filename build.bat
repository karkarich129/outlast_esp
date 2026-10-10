@echo off
setlocal enabledelayedexpansion

echo ====================================================
echo    Building Outlast 1 Internal ESP (DirectX 11 DLL)
echo ====================================================

REM Locate MSBuild from Visual Studio
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD_EXE=%%i"
)

if not defined MSBUILD_EXE (
    echo [ERROR] MSBuild.exe was not found. Please ensure Visual Studio is installed.
    pause
    exit /b 1
)

echo Found MSBuild: "!MSBUILD_EXE!"
echo Building Solution OutlastInternal.sln (Release x64)...
echo.

"!MSBUILD_EXE!" "OutlastInternal.sln" /p:Configuration=Release /p:Platform=x64 /v:minimal

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed! Check compiler output above.
    pause
    exit /b 1
)

echo.
echo ====================================================
echo [SUCCESS] Build completed!
echo Internal DLL: bin\Release\OutlastESP.dll
echo ====================================================
pause
