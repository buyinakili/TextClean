@echo off
setlocal

REM ============================================================
REM  CleanText Native 一键构建脚本
REM  输出: build\x64\Release\CleanText.exe
REM  要求: Visual Studio 2019/2022 (含 Build Tools) + Windows SDK
REM ============================================================

REM 切到脚本所在目录，并去掉 %~dp0 末尾的反斜杠
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

REM ============================================================
REM 用 vswhere 探测任意位置的 VS / BuildTools 安装
REM vswhere 是 VS Installer 自带的官方工具，支持任意安装位置
REM ============================================================
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe 未找到。请安装 Visual Studio 2019/2022：
    echo   https://visualstudio.microsoft.com/downloads/
    echo   选择：Build Tools for Visual Studio - 工作负载 "使用 C++ 的桌面开发"
    popd
    exit /b 1
)

REM 查询最新版本、且包含 MSVC 编译器与 Windows SDK 的 VS 实例
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -requires Microsoft.VisualStudio.Component.Windows10SDK -property installationPath`) do (
    set "VS_INSTALL=%%I"
)

if "%VS_INSTALL%"=="" (
    echo [ERROR] 未找到带 MSVC + Windows SDK 的 VS 实例。请确认已安装：
    echo   - 工作负载："使用 C++ 的桌面开发"
    echo   - 组件："Windows 11 SDK" 或 "Windows 10 SDK"
    popd
    exit /b 1
)

echo [INFO] Using VS install: %VS_INSTALL%
call "%VS_INSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] vcvars64.bat 调用失败
    popd
    exit /b 1
)

REM 把 OutDir / IntDir 重定向到仓库根的 build/ 目录
REM 用 %CD% 而非 %SCRIPT_DIR%，避免 ..\ 拼接时被当字面字符串处理
pushd ..
set "REPO_ROOT=%CD%"
popd
msbuild CleanTextNative.vcxproj /t:Rebuild ^
    /p:Configuration=Release /p:Platform=x64 ^
    /p:OutDir="%REPO_ROOT%\build\x64\Release\\" ^
    /p:IntDir="%REPO_ROOT%\build\obj\\" ^
    /m /v:minimal
set "RC=%errorlevel%"

popd
endlocal & exit /b %RC%