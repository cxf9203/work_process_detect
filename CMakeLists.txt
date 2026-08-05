cmake_minimum_required(VERSION 3.16)

# 选择推理后端
# 可选值: "TensorRT" 或 "OpenVINO"
set(INFERENCE_BACKEND "TensorRT" CACHE STRING "选择推理后端: TensorRT 或 OpenVINO")

# 根据后端设置项目名
if (INFERENCE_BACKEND STREQUAL "TensorRT")
    set(PROJECT_NAME "work_process_detect")
    message(STATUS "🔧 使用 TensorRT 后端 (${PROJECT_NAME})")
elseif (INFERENCE_BACKEND STREQUAL "OpenVINO")
    set(PROJECT_NAME "workProcessDetect_OV_GPU")
    message(STATUS "🔧 使用 OpenVINO 后端 (${PROJECT_NAME})")
else ()
    message(FATAL_ERROR "❌ 未知的后端: ${INFERENCE_BACKEND}，请选择 TensorRT 或 OpenVINO")
endif ()

project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Qt5路径
set(CMAKE_PREFIX_PATH "D:/Qt/5.14.2/msvc2017_64" ${CMAKE_PREFIX_PATH})

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

# if (ANDROID)
#     set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#     if (ANDROID_ABI STREQUAL "armeabi-v7a")
#         set(ANDROID_EXTRA_LIBS
#             ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#             ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#     endif ()
# endif ()

find_package(Qt5 COMPONENTS Core Widgets Multimedia MultimediaWidgets LinguistTools REQUIRED)

# 根据后端设置翻译文件
if (INFERENCE_BACKEND STREQUAL "TensorRT")
    set(TS_FILES work_process_detect_zh_CN.ts)
else ()
    set(TS_FILES workProcessDetect_OV_GPU.ts)
endif ()

# 处理翻译文件
qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
# 将生成的翻译文件添加到源文件列表中
set_source_files_properties(${QM_FILES} PROPERTIES GENERATED TRUE)

# 基础源文件（通用）
set(BASE_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    camera.cpp
    camera.h
    yolov8_common.cpp
    qdarkstyle/dark/darkstyle.qrc
    app.rc
    ${TS_FILES}
)

# 根据后端添加专用源文件
if (INFERENCE_BACKEND STREQUAL "TensorRT")
    set(BACKEND_SOURCES
        engine.cpp
        engine.h
        yolov8_tensorrt.cpp
        yolov8_tensorrt.h
    )
else ()
    set(BACKEND_SOURCES
        yolov8_openvino.cpp
        yolov8_openvino.h
    )
endif ()

# 合并源文件
add_executable(${PROJECT_NAME}
    ${BASE_SOURCES}
    ${BACKEND_SOURCES}
)

# 宏定义
if (INFERENCE_BACKEND STREQUAL "TensorRT")
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_TENSORRT)
else ()
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENVINO)
endif ()

# 通用头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
    "D:/opencv4.7_cuda/include"
    "D:/libmodbus_dll/include"
    "D:/C++Projects/CH-HCNetSDKV6.1.9.48_build20230410_win64_20250312151005/CH-HCNetSDKV6.1.9.48_build20230410_win64/include"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/include"
)

# 通用库路径
target_link_directories(${PROJECT_NAME} PRIVATE
    "D:/opencv4.7_cuda/x64/vc17/lib"
    "D:/libmodbus_dll/lib"
    "D:/C++Projects/CH-HCNetSDKV6.1.9.48_build20230410_win64_20250312151005/CH-HCNetSDKV6.1.9.48_build20230410_win64/lib"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/lib/x64"
)

# 通用库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    opencv_world470
    modbus
    OpenCL
    GdiPlus
    HCCore
    HCNetSDK
    PlayCtrl
)

# 根据后端添加特定依赖
if (INFERENCE_BACKEND STREQUAL "TensorRT")
    # 添加 TensorRT 头文件路径
    target_include_directories(${PROJECT_NAME} PRIVATE
        "D:/TensorRT-8.6.1.6.Windows10.x86_64.cuda-12.0/TensorRT-8.6.1.6/include"
    )
    # 添加 TensorRT 库路径
    target_link_directories(${PROJECT_NAME} PRIVATE
        "D:/TensorRT-8.6.1.6.Windows10.x86_64.cuda-12.0/TensorRT-8.6.1.6/lib"
    )
    # 链接 TensorRT 库
    target_link_libraries(${PROJECT_NAME} PRIVATE
        nvinfer
        nvinfer_plugin
        nvonnxparser
        nvparsers
        cublas
        cublasLt
        cuda
        cudadevrt
        cudart
        cudart_static
        cudnn
        cudnn64_8
        cudnn_adv_infer
        cudnn_adv_infer64_8
        cudnn_adv_train
        cudnn_adv_train64_8
        cudnn_cnn_infer
        cudnn_cnn_infer64_8
        cudnn_cnn_train
        cudnn_cnn_train64_8
        cudnn_ops_infer
        cudnn_ops_infer64_8
        cudnn_ops_train
        cudnn_ops_train64_8
        cufft
        cufftw
        cufilt
        curand
        cusolver
        cusolverMg
        cusparse
        nppc
        nppial
        nppicc
        nppidei
        nppif
        nppig
        nppim
        nppist
        nppisu
        nppitc
        npps
        nvblas
        nvjpeg
        nvml
        nvptxcompiler_static
        nvrtc-builtins_static
        nvrtc
        nvrtc_static
    )
else ()
    # 添加 OpenVINO 头文件路径
    target_include_directories(${PROJECT_NAME} PRIVATE
        "D:/openvino_toolkit_windows_2025.0.0.17942.1f68be9f594_x86_64/runtime/include"
    )
    # 添加 OpenVINO 库路径
    target_link_directories(${PROJECT_NAME} PRIVATE
        "D:/openvino_toolkit_windows_2025.0.0.17942.1f68be9f594_x86_64/runtime/lib/intel64/Release"
    )
    # 链接 OpenVINO 库
    target_link_libraries(${PROJECT_NAME} PRIVATE
        openvino
    )
endif ()

# Windows平台特定设置
if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif ()
