@echo off
REM 快速启动已编译的Qt应用程序

set EXE_PATH=build\Release\vscodeTest.exe

if exist "%EXE_PATH%" (
    echo 正在启动应用程序...
    start "" "%EXE_PATH%"
) else (
    echo 错误：找不到可执行文件 "%EXE_PATH%"
    echo 请先运行 build_and_run.bat 编译项目
    pause
)
