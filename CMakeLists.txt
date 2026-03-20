cmake_minimum_required(VERSION 3.16)

project(vscodeTest LANGUAGES CXX)

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

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(Qt5 COMPONENTS Core Widgets LinguistTools REQUIRED)

set(TS_FILES vscodeTest_zh_CN.ts)

# 处理翻译文件
qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
# 将生成的翻译文件添加到源文件列表中
set_source_files_properties(${QM_FILES} PROPERTIES GENERATED TRUE)

if(ANDROID)
  add_library(vscodeTest SHARED
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ${TS_FILES}
  )
else()
  add_executable(vscodeTest
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    camera.cpp
    camera.h
    engine.cpp
    engine.h
    imagecleaner.cpp
    imagecleaner.h
    reliableimagesaver.cpp
    reliableimagesaver.h
    yolov8.cpp
    yolov8.h
    ${TS_FILES}
  )
endif()

# 添加头文件路径
target_include_directories(vscodeTest PRIVATE
    "D:/opencv4.7_cuda/include"
    "D:/libmodbus_dll/include"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/include"
    "D:/C++Projects/CH-HCNetSDKV6.1.9.48_build20230410_win64_20250312151005/CH-HCNetSDKV6.1.9.48_build20230410_win64/include"
    "D:/TensorRT-8.6.1.6.Windows10.x86_64.cuda-12.0/TensorRT-8.6.1.6/include"
)

# 添加库文件路径
target_link_directories(vscodeTest PRIVATE
    "D:/C++Projects/CH-HCNetSDKV6.1.9.48_build20230410_win64_20250312151005/CH-HCNetSDKV6.1.9.48_build20230410_win64/LIB"
    "D:/opencv4.7_cuda/x64/vc17/lib"
    "D:/libmodbus_dll/lib"
    "D:/TensorRT-8.6.1.6.Windows10.x86_64.cuda-12.0/TensorRT-8.6.1.6/lib"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/lib/x64"
)

# 链接库文件
target_link_libraries(vscodeTest PRIVATE
    Qt5::Core Qt5::Widgets
    opencv_world470
    modbus
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
    OpenCL
    GdiPlus
    HCCore
    HCNetSDK
    PlayCtrl
)

# Windows平台特定设置
if(WIN32)
    set_target_properties(vscodeTest PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
