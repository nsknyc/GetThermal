# GetThermal

A cross-platform thermal camera viewer application, ported to Qt 6 with native macOS support.

![GetThermal screenshot](https://groupgets-files.s3.amazonaws.com/lepton/getthermal_app.png)

## Supported Cameras

GetThermal supports all FLIR Lepton variants used with the USB
[PureThermal 1](https://groupgets.com/manufacturers/getlab/products/purethermal-1-flir-lepton-smart-i-o-module),
[PureThermal 2](https://groupgets.com/manufacturers/getlab/products/purethermal-2-flir-lepton-smart-i-o-module), or
PureThermal Mini Smart I/O Modules, including the Radiometric Lepton 2.5 and 3.5.

It also supports basic thermal data acquisition from [FLIR Boson](https://groupgets.com/manufacturers/flir/products/boson) 320 and 640 (Linux only).

## Supported Platforms

- **macOS** (Apple Silicon and Intel) — native camera via AVFoundation, Lepton SDK via vendor USB interface
- **Linux** (x64, Raspberry Pi, NVIDIA Tegra) — direct USB access via libuvc
- **Windows** — may work with some effort

Requires **Qt 6.5+** and **libusb-1.0**.

## Prerequisites

### macOS (Homebrew)

    brew install qt libusb pkg-config

No libuvc build needed on macOS — the app uses Qt's native camera API for video and libusb directly for Lepton SDK commands.

### Linux (Debian/Ubuntu)

    sudo apt install cmake libusb-1.0-0-dev pkg-config qt6-base-dev qt6-multimedia-dev qt6-declarative-dev

## Building

### 1. Clone GetThermal

    git clone https://github.com/nsknyc/GetThermal.git
    cd GetThermal

### 2. Build libuvc (Linux only)

Linux requires a fork of `libuvc` with FLIR thermal camera support (Y16/I420 frame formats). macOS does not need this step.

    git clone https://github.com/nsknyc/libuvc.git
    cd libuvc
    mkdir build && cd build
    cmake .. -DBUILD_SHARED_LIBS=ON
    make
    cd ../..

Copy the built library and headers into the GetThermal tree:

    mkdir -p GetThermal/libuvc/build/include/libuvc
    cp libuvc/build/libuvcstatic.a GetThermal/libuvc/build/
    cp libuvc/include/libuvc/libuvc.h GetThermal/libuvc/include/libuvc/
    cp libuvc/build/include/libuvc/libuvc_config.h GetThermal/libuvc/build/include/libuvc/

### 3. Build GetThermal

Shadow builds are required:

    mkdir build && cd build
    qmake6 ../GetThermal.pro
    make -j$(nproc)

Or open `GetThermal.pro` in Qt Creator and build from there.

**Note**: After modifying QML files, force a resource rebuild:

    rm rcc/qrc_qml.cpp obj/qrc_qml.o; make

### macOS Code Signing

For the camera permission dialog to appear, the app should be code-signed:

    codesign --force --deep --sign "Your Developer Identity" build/release/GetThermal.app

## PureThermal Firmware

For full macOS support, use the companion firmware fork which adds a vendor-specific USB interface (interface 2, class 0xFF) for direct Lepton SDK access without conflicting with the macOS UVC camera driver:

https://github.com/nsknyc/purethermal1-firmware

The standard PureThermal firmware works on Linux without modification.

## Architecture

### macOS (`#ifdef __macos__`)
- **Video**: QCamera + QMediaCaptureSession (AVFoundation) — works alongside macOS UVC driver
- **Lepton SDK**: libusb vendor control transfers on USB interface 2
- **Colorization**: Hardware pseudo-color via Lepton's VID module

### Linux
- **Video**: libuvc direct USB access — Y16 raw frames for per-pixel radiometry
- **Lepton SDK**: libuvc UVC extension units on USB interface 0
- **Colorization**: Software AGC + palette mapping in DataFormatter

## Features

- Real-time thermal video display
- Lepton AGC, radiometry, and color LUT controls
- Spot temperature readout (radiometric Leptons)
- Min/max temperature point markers
- Click-to-set / click-to-reset focus point
- Hardware color palette (LUT) selection
- FFC (Flat Field Correction)
- Gain mode selection (High/Low/Auto)

## License

This software is provided "AS IS", without warranty of any kind, express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose and non-infringement. In no event shall the authors or copyright holders be liable for any claim, damages, or other liability, whether in an action of contract, tort, or otherwise, arising from, out of, or in connection with the software or the use or other dealings in the software.

## Acknowledgments

This project is a fork of [groupgets/GetThermal](https://github.com/groupgets/GetThermal), originally developed by Kurt Kiefer and GroupGets. Ported from Qt 5 to Qt 6 with macOS-native camera support and vendor USB interface.

The libuvc fork ([nsknyc/libuvc](https://github.com/nsknyc/libuvc)) ports FLIR-specific frame format support originally developed by Kurt Kiefer in [groupgets/libuvc](https://github.com/groupgets/libuvc) onto the latest official libuvc codebase.
