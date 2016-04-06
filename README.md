# VideoMagnification - Magnify motions and detect heartbeats
This application allows you to magnify motion and detect heartbeats from videos and webcam video streams. It is an implementation of Hao-Yu Wu, Michael Rubinstein, Eugene Shih, John Guttag, Frédo Durand, William T. Freeman: **Eulerian Video Magnification for Revealing Subtle Changes in the World** in C++11. It utilizes OpenCV 3.1 and QT 5.

## Examples
The application supports video input from either a live webcam stream or a video file in any format OpenCV supports on your machine:

![Video input and output](/screenshots/io.jpg)

It allows you to magnify the video while adjusting all parameters described in the paper:

![Video input and output](/screenshots/magnification.jpg)

Furthermore, it can display time and frequency plots of the current signal and extract a heartbeat value:

![Video input and output](/screenshots/heartbeat_analysis.jpg)

## Features
* Use video input from webcams and video files
* Experiment with color magnification (ideal temporal filtering) and motion magnification (IIR temporal filtering); all parameters including the number of processed layers and buffered seconds are customizable
* Analyze the video stream to detect heartbeats
* Write the current video stream to file or convert the whole input video
* Experimental: Set camera parameters directly from within the application with V4L2

## Prerequisites
You need a compiler that supports both C++11 and OpenMP. CMake version 3.1 or higher is also required.

Furthermore, this application depends on the following libraries:
* OpenCV 3.1 (built from master at the [OpenCV git repository](https://github.com/Itseez/opencv) as the packaged version includes GUI symbols that may conflict with the QT GUI) provides basic algorithms and datatypes
* The QT framework in version 5 provides the necessary GUI features
* QCustomPlot, a Qt C++ widget for plotting and data visualization. See http://www.qcustomplot.com/.
* FFTW 3, the fastest fourier transform in the West (http://fftw.org)

Some additional notes if you are using Ubuntu:
* Unfortunately, the packages available in Ubuntu 14.04 are too old to use with this application. As I could not find workarounds for all issues within reasonable time, only Ubuntu >= 15.10 is supported.
* Apparently, qcustomplot has an issue that was solved in version 1.3.2 which is not available on Ubuntu prior to version 16.04. However, you can fix the issue manually by executing `sudo sed -i "s/<QtPrintSupport>/<QtPrintSupport\/QtPrintSupport>/" /usr/include/qcustomplot.h` after you installed the package from universe.

This application was developed on Arch Linux and tested on Arch Linux as well as Ubuntu 15.10 (However, as this project does not use any Linux-exclusive libraries, it should also work on Windows and Mac OSX)

## Installation
* Clone OpenCV 3.1, (optionally) checkout a stable tag (such as 3.1.0), disable GTK (as it conflicts with the QT GUI), then make and install to `/opt/opencv` (or any other path, but be sure to set this path in the project's CMakeLists.txt):
```bash
git clone https://github.com/itseez/opencv
cd opencv
git checkout 3.1.0
mkdir build && cd build
sed -i "s/\(OCV_OPTION(WITH_GTK .*$\)/OCV_OPTION(WITH_GTK \"Include GTK support\" OFF)/" ../CMakeLists.txt
cmake ..
make -j8
sudo make install DESTDIR=/opt/opencv
```
* Install QT5: `sudo apt install qt5-default` on Ubuntu or `sudo pacman -S qt5-base` on Arch Linux

* Install QCustomPlot: `sudo apt install libqcustomplot-dev` on Ubuntu from universe or `qcustomplot-qt5` from the AUR on Arch Linux

* Install FFTW3: `sudo apt install libfftw3-dev` on Ubuntu or `sudo pacman -S fftw` on Arch Linux

## Compilation and Usage
Set this line in CMakeLists.txt to the path you installed OpenCV to (`/opt/opencv/usr/local` if you used `DESTDIR=/opt/opencv` before)
```cmake
set(CUSTOM_OPENCV_PATH "/opt/opencv/usr/local")
```
If you are on Linux and want to try the experimental V4L2 interface which allows you to edit available camera parameters from within the application, set the option `USE_V4L2` to `ON` in CMakeLists.txt.

Then you should be able to compile and run the application:
```bash
mkdir build && cd build
cmake ..
make
./VideoMagnification
```

## License
This application is licensed under GPLv3.
