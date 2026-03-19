@echo off
REM 配置、编译并启动Qt项目 (使用Visual Studio 2022)

echo 正在创建构建目录...
if not exist build mkdir build

echo 正在配置项目...
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/5.14.2/msvc2017_64 -DCMAKE_CXX_STANDARD=17

if %errorlevel% neq 0 (
    echo CMake配置失败！
    pause
    exit /b 1
)

echo 正在编译项目...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo 编译失败！
    pause
    exit /b 1
)

echo 正在启动应用程序...
start "" Release\vscodeTest.exe

cd ..
echo 完成！
pause
