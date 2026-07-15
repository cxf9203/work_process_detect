@echo off
chcp 65001 >nul

REM 配置、编译并启动Qt项目 (使用Visual Studio 2022)
REM 支持选择推理后端: TensorRT 或 OpenVINO

echo ========================================
echo 选择推理后端:
echo   1. TensorRT
echo   2. OpenVINO
echo ========================================
set /p backend_choice="请输入数字 (1 或 2): "

if "%backend_choice%"=="1" (
    set INFERENCE_BACKEND=TensorRT
    set EXE_NAME=work_process_detect.exe
) else if "%backend_choice%"=="2" (
    set INFERENCE_BACKEND=OpenVINO
    set EXE_NAME=workProcessDetect_OV_GPU.exe
) else (
    echo 无效选择，使用默认 TensorRT 后端
    set INFERENCE_BACKEND=TensorRT
    set EXE_NAME=work_process_detect.exe
)

echo ========================================
echo 推理后端: %INFERENCE_BACKEND%
echo 可执行文件: %EXE_NAME%
echo ========================================

echo build...
if not exist build mkdir build

echo 正在配置项目...
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/5.14.2/msvc2017_64 -DCMAKE_CXX_STANDARD=17 -DINFERENCE_BACKEND=%INFERENCE_BACKEND%

if %errorlevel% neq 0 (
    echo CMake configure fail!
    pause
    exit /b 1
)

echo 正在编译项目...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo build failed!
    pause
    exit /b 1
)

echo 正在启动 %INFERENCE_BACKEND% 应用程序...
if exist Release\%EXE_NAME% (
    start "" Release\%EXE_NAME%
    echo 启动成功！
    pause
) else (
    echo 错误: 找不到可执行文件 %EXE_NAME%
    pause
    exit /b 1
)