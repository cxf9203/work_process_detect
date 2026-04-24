@echo off
REM 配置、编译并启动Qt项目 (使用Visual Studio 2022)

echo build...
if not exist build mkdir build

echo 正在配置项目...
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/QT/5.14.2/msvc2017_64 -DCMAKE_CXX_STANDARD=17

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

echo 正在启动应用程序...
start "" Release\vscodeTest.exe

cd ..
echo success!
pause