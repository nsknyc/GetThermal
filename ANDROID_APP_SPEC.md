# GetThermal Android App Specification

## Overview
Android thermal camera viewer for FLIR Lepton sensors on PureThermal (GroupGets) USB boards. The PureThermal board connects via USB-C/OTG and presents as a UVC camera with vendor-specific extensions for Lepton SDK control.

## Hardware
- **Board**: PureThermal1/2 (VID: 0x1e4e, PID: 0x0100)
- **Sensor**: FLIR Lepton 2.x (80x60) or Lepton 3.x (160x120)
- **Connection**: USB Host (OTG) on Android

## USB Interface Layout
The PureThermal firmware exposes 3 USB interfaces:

| Interface | Class | Purpose |
|-----------|-------|---------|
| 0 | 0x0E (Video) | UVC Video Control — extension units for Lepton SDK |
| 1 | 0x0E (Video) | UVC Video Streaming — thermal frame data |
| 2 | 0xFF (Vendor) | Direct Lepton SDK commands via vendor control transfers |

On Android, you can use either:
- **Interfaces 0+1** (standard UVC path) — use a UVC library like UVCCamera/libuvc-android
- **Interface 2** (vendor path) — use Android USB Host API for control transfers
- **Both** simultaneously — UVC for video, vendor interface for SDK commands

## Video Streaming Formats
The firmware supports these UVC formats (negotiated via stream control):
- **Y16**: Raw 16-bit thermal data. Each pixel = temperature value. This is what you want for radiometry.
- **YUYV**: YUV 4:2:2, hardware-colorized by the Lepton's VID module
- **GREY**: 8-bit grayscale
- **RGB565**: 16-bit color
- **BGR3**: 24-bit BGR

### Y16 Format Details
- Resolution: 80x60 (Lepton 2) or 160x120 (Lepton 3)
- Each pixel: uint16_t in Kelvin × 100 (high-res) or Kelvin × 10 (low-res)
- Example: pixel value 29515 = 295.15K = 22.0°C (high-res mode)
- To convert: `tempC = (pixelValue - 27315) / 100.0` (high-res) or `(pixelValue - 2731.5) / 10.0` (low-res)
- TLinear resolution is configurable via `LEP_SetRadTLinearResolution`

## Lepton SDK Control Protocol

### Via Vendor Interface (Interface 2)
USB control transfers on EP0:
- **GET attribute**: `bmRequestType=0xC1, bRequest=0, wValue=commandID, wIndex=2, wLength=dataBytes`
- **SET attribute**: `bmRequestType=0x41, bRequest=0, wValue=commandID, wIndex=2, wLength=dataBytes`
- **RUN command**: `bmRequestType=0x41, bRequest=0, wValue=commandID, wIndex=2, wLength=0`

### Via UVC Extension Units (Interface 0)
Standard UVC GET_CUR/SET_CUR on extension units with Lepton-specific GUIDs:
- Unit 3: AGC module (GUID: "pt1-lep-agc-0000")
- Unit 4: OEM module (GUID: "pt1-lep-oem-0000")
- Unit 5: RAD module (GUID: "pt1-lep-rad-0000")
- Unit 6: SYS module (GUID: "pt1-lep-sys-0000")
- Unit 7: VID module (GUID: "pt1-lep-vid-0000")

### Key Lepton Command IDs
```
LEP_CID_AGC_ENABLE_STATE     = 0x0100  // GET/SET: AGC on/off (4 bytes, 0=off 1=on)
LEP_CID_AGC_POLICY           = 0x0104  // GET/SET: Linear(0) or HEQ(1)
LEP_CID_SYS_FLIR_SERIAL_NUM  = 0x0208  // GET: 8-byte serial number
LEP_CID_SYS_FFC_NORMALIZATION = 0x0242 // RUN: Flat Field Correction
LEP_CID_SYS_GAIN_MODE        = 0x0248  // GET/SET: High(0)/Low(1)/Auto(2)
LEP_CID_VID_LUT_SELECT       = 0x0304  // GET/SET: Pseudo-color LUT (4 bytes)
LEP_CID_OEM_SW_VERSION       = 0x0820  // GET: Software version struct
LEP_CID_OEM_PART_NUMBER      = 0x081C  // GET: Part number string
LEP_CID_RAD_SPOTMETER_ROI    = 0x0ECC  // GET/SET: Spotmeter ROI (startRow, startCol, endRow, endCol)
LEP_CID_RAD_SPOTMETER_VALUE  = 0x0ED0  // GET: Spotmeter temperature (Kelvin×100)
LEP_CID_RAD_TLINEAR_RESOLUTION = 0x0EC4 // GET/SET: 0=0.1K, 1=0.01K
```

## App Features
1. **Live thermal video** — Y16 frames with software AGC + colorization
2. **Per-pixel temperature** — tap any pixel to read its temperature
3. **Min/Max markers** — show hottest/coldest points on the image
4. **Spot temperature** — configurable ROI for focused temperature reading
5. **Color palettes** — Iron Black, Rainbow, Grayscale (software) or hardware LUTs
6. **AGC controls** — enable/disable, policy, histogram parameters
7. **FFC** — flat field correction (recalibrate sensor)
8. **Gain mode** — High/Low/Auto sensitivity
9. **Radiometry** — TLinear resolution selection

## Software AGC + Colorization Pipeline
For Y16 frames:
1. **Find min/max** across all pixels
2. **Scale**: `scaled = ((pixel - min) / (max - min)) * 255`
3. **Colorize**: Look up scaled value (0-255) in a 256-entry RGB palette
4. Display the resulting RGB image

## Reference Implementation
- **Desktop app** (Qt 6 / C++): `/Users/nsk/Documents/github/GetThermal/`
  - `src/dataformatter.cpp` — AGC and colorization code
  - `src/leptonvariation.cpp` — Lepton SDK communication
  - `inc/leptonvariation_types.h` — Lepton enum definitions for QML
- **Firmware**: `/Users/nsk/Documents/github/purethermal1-firmware/`
  - `Middlewares/.../usbd_uvc.c` — USB descriptor and vendor request handler
  - `Src/lepton_i2c_task.c` — I2C communication with Lepton sensor

## Modified libuvc
The official libuvc (libuvc/libuvc) was forked to `nsk/libuvc` with these additions for FLIR thermal camera support:
- `UVC_FRAME_FORMAT_Y16` — `#define` alias for `GRAY16` (same GUID `'Y','1','6',' '`)
- `UVC_FRAME_FORMAT_I420` — new enum value with GUID `'I','4','2','0'` for planar YUV 4:2:0
- `uvc_i4202rgb()` / `uvc_i4202bgr()` — I420 frame conversion functions
- I420 step calculation in `stream.c`
- `device.c`: treat `LIBUSB_ERROR_ACCESS` as non-fatal in `uvc_claim()` (macOS compatibility)

Source: `/Users/nsk/Documents/github/libuvc-official/` (points to `nsk/libuvc` on GitHub)

The original groupgets/libuvc fork had these features but was based on very old libuvc code. These patches port the essential additions onto the latest official libuvc.

## Android-Specific Notes
- Use `UsbManager.requestPermission()` for USB access
- `UsbDeviceConnection.controlTransfer()` for vendor interface commands
- For UVC video streaming, consider [UVCCamera](https://github.com/saki4510t/UVCCamera) library or implement isochronous transfers directly
- USB isochronous transfers for video require Android NDK (not available in Java USB API)
- Alternative: use the Android Camera2 API if the device appears as a standard camera (some Android versions support UVC cameras natively)
