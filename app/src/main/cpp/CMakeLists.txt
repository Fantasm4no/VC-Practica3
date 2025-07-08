cmake_minimum_required(VERSION 3.22.1)

project("aplicacionnativa")

set(OpenCV_STATIC on)

# Aquí pones la ruta completa a la carpeta jni dentro del SDK
set(OpenCV_DIR /home/f4ntasmano/Escritorio/opencv_androidStudio/OpenCV-android-sdk/sdk/native/jni)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_DIR}/include)

add_library(aplicacionnativa SHARED native-lib.cpp)

find_library(jnigraphics-lib jnigraphics)
find_library(log-lib log)

target_link_libraries(aplicacionnativa
        ${OpenCV_LIBS}
        ${jnigraphics-lib}
        ${log-lib}
        log)
