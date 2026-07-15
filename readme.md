# work_process_detect Qt项目

这是一个使用CMake构建的Qt应用程序项目，支持 TensorRT 和 OpenVINO 两种推理后端。

## 项目结构

- `main.cpp`: 应用程序入口点
- `mainwindow.h/cpp`: 主窗口类
- `mainwindow.ui`: UI界面设计文件
- `camera.h/cpp`: 相机控制类
- `yolov8_common.h/cpp`: YOLOv8 公共基类
- `yolov8_tensorrt.h/cpp`: YOLOv8 TensorRT 实现
- `yolov8_openvino.h/cpp`: YOLOv8 OpenVINO 实现
- `engine.h/cpp`: TensorRT 引擎封装类（仅 TensorRT 后端）
- `work_process_detect_zh_CN.ts`: 中文翻译文件（TensorRT 后端）
- `workProcessDetect_OV_GPU.ts`: 中文翻译文件（OpenVINO 后端）
- `CMakeLists.txt`: CMake构建配置文件
- `build_and_run.bat`: Windows下快速构建和运行脚本
- `run.bat`: Windows下快速运行已编译应用程序的脚本

## 推理后端选择

项目支持两种推理后端，通过 CMake 变量 `INFERENCE_BACKEND` 切换：

| 后端 | 项目名 | 可执行文件 |
|------|--------|-----------|
| TensorRT | `work_process_detect` | `work_process_detect.exe` |
| OpenVINO | `workProcessDetect_OV_GPU` | `workProcessDetect_OV_GPU.exe` |

## 构建和运行

### 方法1：使用批处理脚本（Windows）

#### 首次构建或代码修改后

直接双击运行 `build_and_run.bat` 脚本，它将自动完成以下步骤：
1. 创建构建目录
2. 配置项目（可选择 TensorRT 或 OpenVINO 后端）
3. 编译项目
4. 启动应用程序

#### 快速运行已编译的应用程序

如果您已经编译过项目，只需双击运行 `run.bat` 脚本，它将直接启动已编译的应用程序，而不重新编译。

### 方法2：使用VSCode调试

项目已配置好VSCode调试环境，可以直接在VSCode中进行调试：

1. **首次使用前配置项目**：
   - 打开命令面板（Ctrl+Shift+P）
   - 输入"Tasks: Run Task"
   - 选择对应的配置任务

2. **开始调试**：
   - 按F5或点击调试按钮
   - 选择对应的调试配置
   - 程序将在外部终端中运行，可以查看调试输出信息

3. **调试说明**：
   - 调试过程中可以设置断点、查看变量、单步执行等
   - 每次开始调试前，VSCode会自动检查并重新编译项目
   - 调试输出会显示在外部终端中
   - 如需添加调试信息，可在代码中使用 `qDebug()` 语句

4. **调试后运行**：
   - 调试后如果代码没有新的修改，可以直接运行 `build_and_run.bat` 或 `run.bat`
   - 如果有新的代码修改，则任一方式都会触发编译

### 方法3：手动构建

#### Windows (Visual Studio 2022) - TensorRT 后端

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/5.14.2/msvc2017_64 -DINFERENCE_BACKEND=TensorRT
cmake --build . --config Release
Release\work_process_detect.exe
```

#### Windows (Visual Studio 2022) - OpenVINO 后端

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/5.14.2/msvc2017_64 -DINFERENCE_BACKEND=OpenVINO
cmake --build . --config Release
Release\workProcessDetect_OV_GPU.exe
```

#### Windows (Visual Studio 2019) - TensorRT 后端

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/5.14.2/msvc2017_64 -DINFERENCE_BACKEND=TensorRT
cmake --build . --config Release
Release\work_process_detect.exe
```

#### Windows (Visual Studio 2019) - OpenVINO 后端

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/5.14.2/msvc2017_64 -DINFERENCE_BACKEND=OpenVINO
cmake --build . --config Release
Release\workProcessDetect_OV_GPU.exe
```

#### Linux/macOS（仅支持 OpenVINO 后端）

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DINFERENCE_BACKEND=OpenVINO
make
./workProcessDetect_OV_GPU
```

## 环境要求

- CMake 3.16 或更高版本
- Qt5 5.14.2 MSVC2017_64 开发环境
- Visual Studio 2022 或 2019 (支持C++17)
- OpenCV 4.7 (CUDA 版本)
- libmodbus
- 海康威视 HCNetSDK
- TensorRT 后端：TensorRT 8.6.1.6 + CUDA 12.2
- OpenVINO 后端：OpenVINO 2025.0

## 翻译

项目包含中文翻译文件：
`work_process_detect_zh_CN.ts`（TensorRT 后端）
`workProcessDetect_OV_GPU.ts`（OpenVINO 后端）
翻译文件会在构建过程中自动处理。

要手动更新翻译：

1. 使用 Qt Linguist 工具打开并编辑对应的翻译文件：
   ```bash
   linguist work_process_detect_zh_CN.ts
   ```

2. 重新构建项目以生成翻译文件(.qm)

## 注意事项

- 确保Qt5的安装路径已添加到系统环境变量中
- 使用Visual Studio 2022编译器，确保已安装Visual Studio 2022和C++桌面开发组件
- 确保Qt5的MSVC2017_64版本已正确安装在 D:/Qt/5.14.2/msvc2017_64
- 如果遇到编译问题，请检查 Qt5_DIR 环境变量是否指向正确的 Qt5Config.cmake 文件位置
- Visual Studio 2022可以兼容使用MSVC2017编译的Qt5库，但可能需要额外的运行时库支持
- 版本控制不同步 build 目录文件
- 切换后端时建议删除 build 目录重新配置，避免缓存冲突