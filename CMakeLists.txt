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
    ${TS_FILES}
  )
endif()

target_link_libraries(vscodeTest PRIVATE Qt5::Core Qt5::Widgets)

# Windows平台特定设置
if(WIN32)
    set_target_properties(vscodeTest PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
