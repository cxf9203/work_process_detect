@echo off
chcp 65001 >nul

REM 快速启动已编译的Qt应用程序
REM 支持 TensorRT 和 OpenVINO 两种后端

echo ========================================
echo 选择要运行的后端:
echo   1. TensorRT
echo   2. OpenVINO
echo ========================================
set /p backend_choice="请输入数字 (1 或 2): "

if "%backend_choice%"=="1" (
    set EXE_NAME=work_process_detect.exe
    set BACKEND_NAME=TensorRT
) else if "%backend_choice%"=="2" (
    set EXE_NAME=workProcessDetect_OV_GPU.exe
    set BACKEND_NAME=OpenVINO
) else (
    echo 无效选择，使用默认 TensorRT 后端
    set EXE_NAME=work_process_detect.exe
    set BACKEND_NAME=TensorRT
)

set EXE_PATH=build\Release\%EXE_NAME%

echo ========================================
echo 后端: %BACKEND_NAME%
echo 可执行文件: %EXE_PATH%
echo ========================================

if exist "%EXE_PATH%" (
    echo 正在启动 %BACKEND_NAME% 应用程序...
    start "" "%EXE_PATH%"
    echo 启动成功！
    pause
) else (
    echo 错误: 找不到可执行文件 "%EXE_PATH%"
    echo.
    echo 可能的原因:
    echo   1. 项目尚未编译，请先运行 build_and_run.bat
    echo   2. 选择了错误的后端，请确认编译时使用的后端
    echo   3. 构建目录不存在或构建失败
    echo.
    pause
    exit /b 1
)