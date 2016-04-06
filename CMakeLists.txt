cmake_minimum_required(VERSION 3.1)

# Set project name and version
set(PROJECT_NAME "VideoMagnification")
set(VideoMagnification_VERSION_MAJOR 1)
set(VideoMagnification_VERSION_MINOR 0)

project(${PROJECT_NAME})

# Activate OpenMP
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# Set C++ standard to 11
set(CMAKE_CXX_STANDARD 11)

# Set this path to an OpenCV version that was compiled without GUI features; tested with OpenCV 3.1.0
set(CUSTOM_OPENCV_PATH "/opt/opencv/usr/local")

# Set this to the file you want to use for the simple facetracking method (OpenCV's CascadeClassifier::detectMultiScale);
# if you chose a valid path above, this pre-set value should be fine
set(FACE_CLASSIFIER_FILE "(\"${CUSTOM_OPENCV_PATH}/share/OpenCV/haarcascades/haarcascade_frontalface_default.xml\")")

# Experimental support for direct Video4Linux2
option(USE_V4L2
        "Directly use V4L2 on Linux systems; this allows the user to manually control all available camera parameters"
        OFF)

# QT specific options
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Custom macros to allow dynamic adding of sources and libs
macro (add_sources)
    foreach (_src ${ARGN})
        list (APPEND SRCS "${_src}")
    endforeach()
endmacro()

macro (add_libs)
    foreach (_lib ${ARGN})
        list (APPEND LIBS ${_lib})
    endforeach()
endmacro()

# Using QT5
find_package(Qt5Widgets)
QT5_WRAP_CPP(MOC include/mainwindow.h include/helpers/QImageWidget.h)

# --- SOURCES ---
add_sources(src/main.cpp
        src/processing/spatial_filter.cpp src/processing/temporal_filter.cpp src/processing/analysis.cpp
        src/helpers/data_container.cpp src/helpers/QImageWidget.cpp
        src/mainwindow.cpp)

# --- LIBRARIES ---
# OpenCV components
add_libs(opencv_core opencv_videoio opencv_imgproc opencv_objdetect opencv_imgcodecs)
# FFTW3, pthread and qcustomplot
add_libs(fftw3f pthread qcustomplot)

if(USE_V4L2)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL Linux)
        message(SEND_ERROR "You can only use Video4Linux2 on Linux systems")
        add_sources(src/video_source/video_opencv.cpp)
    else()
        add_definitions(-DV4L2_CAPTURE)
        add_sources(src/video_source/video_v4l2.cpp)
        add_libs(v4l2)
    endif()
else()
    add_sources(src/video_source/video_opencv.cpp)
endif()

# Set path for face detector file
add_definitions(-DFACE_CLASSIFIER_FILE=${FACE_CLASSIFIER_FILE})

# Project internal includes and custom OpenCV includes
include_directories("include" "${CUSTOM_OPENCV_PATH}/include")

# Custom OpenCV libs
link_directories("${CUSTOM_OPENCV_PATH}/lib")

# Compile
add_executable(${PROJECT_NAME} ${SRCS} ${MOC})

# Link
target_link_libraries(${PROJECT_NAME} ${LIBS} Qt5::Widgets)
