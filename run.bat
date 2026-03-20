@echo off
REM 快速启动已编译的Qt应用程序

set EXE_PATH=build\Release\vscodeTest.exe

if exist "%EXE_PATH%" (
    echo start exe...
    start "" "%EXE_PATH%"
) else (
    echo error:cannot find exr "%EXE_PATH%"
    echo run build_and_run.bat to build project
    pause
)
