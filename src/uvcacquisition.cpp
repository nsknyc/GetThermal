#include "uvcacquisition.h"
#include <QList>

#ifdef __macos__
#include <QPermissions>
#include <QCoreApplication>
#endif

#include "leptonvariation.h"
#ifndef __macos__
#include "bosonvariation.h"
#endif
#include "dataformatter.h"

//#define PLANAR_BUFFER 1
//#define ACQ_RGB 1
#define ACQ_Y16 1

#define PT1_VID 0x1e4e
#define PT1_PID 0x0100
#define FLIR_VID 0x09cb

UvcAcquisition::UvcAcquisition(QObject *parent)
    : QObject(parent)
#ifdef __macos__
    , m_camera(NULL)
    , m_captureSession(NULL)
    , m_captureSink(NULL)
    , m_libusbCtx(NULL)
    , m_libusbDevh(NULL)
#else
    , ctx(NULL)
    , dev(NULL)
    , devh(NULL)
    , m_uvcFrameFormat(UVC_FRAME_FORMAT_UNKNOWN)
#endif
    , m_cci(NULL)
{
    _ids.append({ PT1_VID, PT1_PID });
    _ids.append({ FLIR_VID, 0x0000 }); // any flir camera
    init();
}

UvcAcquisition::UvcAcquisition(QList<UsbId> ids)
    : QObject()
#ifdef __macos__
    , m_camera(NULL)
    , m_captureSession(NULL)
    , m_captureSink(NULL)
    , m_libusbCtx(NULL)
    , m_libusbDevh(NULL)
#else
    , ctx(NULL)
    , dev(NULL)
    , devh(NULL)
    , m_uvcFrameFormat(UVC_FRAME_FORMAT_UNKNOWN)
#endif
    , m_cci(NULL)
    , _ids(ids)
{
    init();
}

UvcAcquisition::~UvcAcquisition()
{
    if (m_cci != NULL)
    {
        delete m_cci;
    }

#ifdef __macos__
    if (m_camera != NULL)
    {
        m_camera->stop();
    }

    if (m_libusbDevh != NULL)
    {
        libusb_release_interface(m_libusbDevh, 2);
        libusb_close(m_libusbDevh);
    }

    if (m_libusbCtx != NULL)
    {
        libusb_exit(m_libusbCtx);
    }
#else
    if (devh != NULL)
    {
        uvc_stop_streaming(devh);
        puts("Done streaming.");

        /* Release our handle on the device */
        uvc_close(devh);
        puts("Device closed");
    }

    if (dev != NULL)
    {
        /* Release the device descriptor */
        uvc_unref_device(dev);
    }

    if (ctx != NULL)
    {
        /* Close the UVC context. This closes and cleans up any existing device handles,
         * and it closes the libusb context if one was not provided. */
        uvc_exit(ctx);
        puts("UVC exited");
    }
#endif
}

void UvcAcquisition::init()
{
#ifdef __macos__
    int ret;

    /* Initialize libusb and open vendor interface for Lepton SDK */
    ret = libusb_init(&m_libusbCtx);
    if (ret < 0) {
        printf("libusb_init failed: %d\n", ret);
        return;
    }
    puts("libusb initialized");

    m_libusbDevh = libusb_open_device_with_vid_pid(m_libusbCtx, PT1_VID, PT1_PID);
    if (m_libusbDevh == NULL) {
        printf("PureThermal device not found\n");
        return;
    }
    puts("PureThermal device opened");

    ret = libusb_claim_interface(m_libusbDevh, 2);
    if (ret < 0) {
        printf("Failed to claim vendor interface: %d\n", ret);
        libusb_close(m_libusbDevh);
        m_libusbDevh = NULL;
        return;
    }
    puts("Vendor interface claimed");

    m_cci = new LeptonVariation(m_libusbDevh);

    /* Find PureThermal camera in system camera list */
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    QCameraDevice ptCamera;

    for (const QCameraDevice &cam : cameras) {
        if (cam.description().contains("PureThermal", Qt::CaseInsensitive) ||
            cam.description().contains("GroupGets", Qt::CaseInsensitive) ||
            cam.description().contains("0x1e4e", Qt::CaseInsensitive)) {
            ptCamera = cam;
            printf("Found camera: %s\n", qPrintable(cam.description()));
            break;
        }
    }

    if (ptCamera.isNull() && !cameras.isEmpty()) {
        /* Fallback: list all cameras and use the first non-FaceTime one */
        for (const QCameraDevice &cam : cameras) {
            printf("Available camera: %s\n", qPrintable(cam.description()));
            if (!cam.description().contains("FaceTime", Qt::CaseInsensitive)) {
                ptCamera = cam;
                break;
            }
        }
    }

    if (ptCamera.isNull()) {
        printf("No suitable camera found\n");
        /* Still allow the app to run for SDK control even without video */
    }

    if (!ptCamera.isNull()) {
        if (m_cci != NULL)
        {
            setVideoFormat(m_cci->getDefaultFormat());
        }

        /* Request camera permission before creating camera objects */
        QCameraDevice savedCamera = ptCamera;
        QCameraPermission cameraPermission;

        auto startCamera = [this, savedCamera]() {
            m_camera = new QCamera(savedCamera, this);
            m_captureSession = new QMediaCaptureSession(this);
            m_captureSink = new QVideoSink(this);

            m_captureSession->setCamera(m_camera);
            m_captureSession->setVideoSink(m_captureSink);

            connect(m_captureSink, &QVideoSink::videoFrameChanged,
                    this, &UvcAcquisition::onCameraFrameReceived);

            m_camera->start();
            puts("Camera streaming started");
        };

        switch (qApp->checkPermission(cameraPermission)) {
        case Qt::PermissionStatus::Granted:
            startCamera();
            break;
        case Qt::PermissionStatus::Undetermined:
            qApp->requestPermission(cameraPermission, this, [startCamera](const QPermission &permission) {
                if (permission.status() == Qt::PermissionStatus::Granted) {
                    startCamera();
                } else {
                    puts("Camera permission denied");
                }
            });
            break;
        default:
            puts("Camera permission denied");
            break;
        }
    }
    else if (m_cci != NULL)
    {
        setVideoFormat(m_cci->getDefaultFormat());
    }

#else
    uvc_error_t res;

    /* Initialize a UVC service context. Libuvc will set up its own libusb
     * context. Replace NULL with a libusb_context pointer to run libuvc
     * from an existing libusb context. */
    res = uvc_init(&ctx, NULL);

    if (res < 0) {
      uvc_perror(res, "uvc_init");
      return;
    }

    puts("UVC initialized");

    /* Locates the first attached UVC device, stores in dev */
    for (int i = 0; i < _ids.size(); ++i) {
        res = uvc_find_device(ctx, &dev, _ids[i].vid, _ids[i].pid, NULL);
        if (res >= 0)
            break;
    }

    if (res < 0) {
        uvc_perror(res, "uvc_find_device"); /* no devices found */
        return;
    }

    puts("Device found");

    /* Try to open the device: requires exclusive access */
    res = uvc_open(dev, &devh);

    if (res < 0) {
        uvc_perror(res, "uvc_open"); /* unable to open device */

        /* Release the device descriptor */
        uvc_unref_device(dev);
        dev = NULL;
        return;
    }

    puts("Device opened");

    /* Print out a message containing all the information that libuvc
     * knows about the device */
    uvc_print_diag(devh, stderr);

    uvc_device_descriptor_t *desc;
    uvc_get_device_descriptor(dev, &desc);

    switch (desc->idVendor)
    {
    case PT1_VID:
        m_cci = new LeptonVariation(ctx, dev, devh);
        break;
    case FLIR_VID:
        m_cci = new BosonVariation(ctx, dev, devh);
        break;
    default:
        break;
    }

    uvc_free_device_descriptor(desc);

    if (m_cci != NULL)
    {
        setVideoFormat(m_cci->getDefaultFormat());
    }
#endif
}

void UvcAcquisition::setVideoFormat(const QVideoFrameFormat &format)
{
#ifdef __macos__
    /* On macOS, QCamera handles format negotiation. We just track the format
     * for the UI (video size, format change signals). */
    m_format = format;
    emit formatChanged(m_format);
    emit videoSizeChanged(m_format.frameSize());
#else
    uvc_error_t res;

    uvc_stop_streaming(devh);

    switch(format.pixelFormat())
    {
    case QVideoFrameFormat::Format_YUV420P:
        m_uvcFrameFormat = UVC_FRAME_FORMAT_I420;
        m_format = QVideoFrameFormat(format.frameSize(), format.pixelFormat());
        break;
    case QVideoFrameFormat::Format_RGBX8888:
        // Sentinel for UVC RGB24 source (no Format_RGB24 in Qt 6)
        m_uvcFrameFormat = UVC_FRAME_FORMAT_RGB;
        m_format = QVideoFrameFormat(format.frameSize(), QVideoFrameFormat::Format_BGRX8888);
        break;
    case QVideoFrameFormat::Format_Y16:
        m_uvcFrameFormat = UVC_FRAME_FORMAT_Y16;
        m_format = QVideoFrameFormat(format.frameSize(), QVideoFrameFormat::Format_BGRX8888);
        break;
    default:
        m_uvcFrameFormat = UVC_FRAME_FORMAT_UNKNOWN;
        m_format = QVideoFrameFormat(format.frameSize(), QVideoFrameFormat::Format_Invalid);
        break;
    }

    res = uvc_get_stream_ctrl_format_size(
                devh, &ctrl, /* result stored in ctrl */
                m_uvcFrameFormat,
                format.frameSize().width(), format.frameSize().height(), 0);

    /* Print out the result */
    uvc_print_stream_ctrl(&ctrl, stderr);

    if (res < 0) {
        uvc_perror(res, "get_mode"); /* device doesn't provide a matching stream */
        return;
    }

    // Notify connections of format change
    emit formatChanged(m_format);
    emit videoSizeChanged(m_format.frameSize());

    /* Start the video stream. The library will call user function cb:
     *   cb(frame, (void*) 12345)
     */
    res = uvc_start_streaming(devh, &ctrl, UvcAcquisition::cb, this, 0);

    if (res < 0) {
        uvc_perror(res, "start_streaming"); /* unable to start stream */
        uvc_close(devh);
        puts("Device closed");

        return;
    }

    puts("Streaming...");
#endif
}

#ifdef __macos__
void UvcAcquisition::onCameraFrameReceived(const QVideoFrame &frame)
{
    static int frameCount = 0;
    if (frameCount++ % 100 == 0)
        printf("Camera frame %d: %dx%d format=%d\n", frameCount,
               frame.width(), frame.height(), (int)frame.pixelFormat());
    emit frameReady(frame);
}
#else
/* This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */
void UvcAcquisition::cb(uvc_frame_t *frame, void *ptr) {

    UvcAcquisition *_this = static_cast<UvcAcquisition*>(ptr);

    Q_ASSERT((int)frame->width == _this->m_format.frameWidth());
    Q_ASSERT((int)frame->height == _this->m_format.frameHeight());

    // Need to reshape UVC input to display format
    if (_this->m_uvcFrameFormat == UVC_FRAME_FORMAT_Y16 ||
        _this->m_uvcFrameFormat == UVC_FRAME_FORMAT_RGB)
    {
        // we don't have a reason to handle frame buffers other than BGRX8888 for now
        Q_ASSERT(_this->m_format.pixelFormat() == QVideoFrameFormat::Format_BGRX8888);

        QVideoFrame qframe(QVideoFrameFormat(_this->m_format.frameSize(),
                                             _this->m_format.pixelFormat()));

        if (_this->m_uvcFrameFormat == UVC_FRAME_FORMAT_Y16)
        {
            _this->m_df.AutoGain(frame);
            _this->m_df.Colorize(frame, qframe);
        }
        else if (_this->m_uvcFrameFormat == UVC_FRAME_FORMAT_RGB)
        {
            qframe.map(QVideoFrame::WriteOnly);
            for (int i = 0; i < qframe.height(); i++)
            {
                uchar* rgb_line = &((uchar*)frame->data)[frame->step * i];
                uchar* rgba_line = &qframe.bits(0)[qframe.bytesPerLine(0) * i];

                for (int j = 0; j < qframe.width(); j++)
                {
                    rgba_line[j * 4 + 0] = rgb_line[j * 3 + 0];
                    rgba_line[j * 4 + 1] = rgb_line[j * 3 + 1];
                    rgba_line[j * 4 + 2] = rgb_line[j * 3 + 2];
                    rgba_line[j * 4 + 3] = 0;
                }
            }
            qframe.unmap();
        }
        _this->emitFrameReady(qframe);
    }
    else
    {
        // Passthrough (e.g. YUV420P) — copy UVC buffer into Qt-managed frame
        QVideoFrame qframe(QVideoFrameFormat(_this->m_format.frameSize(),
                                             _this->m_format.pixelFormat()));
        qframe.map(QVideoFrame::WriteOnly);
        memcpy(qframe.bits(0), frame->data, frame->data_bytes);
        qframe.unmap();
        _this->emitFrameReady(qframe);
    }
}
#endif

void UvcAcquisition::emitFrameReady(const QVideoFrame &frame)
{
    emit frameReady(frame);
}

void UvcAcquisition::pauseStream() {
#ifdef __macos__
    if (m_camera) m_camera->stop();
#else
    uvc_stop_streaming(devh);
#endif
}

void UvcAcquisition::resumeStream() {
#ifdef __macos__
    if (m_camera) m_camera->start();
#else
    uvc_error_t res = uvc_start_streaming(devh, &ctrl, UvcAcquisition::cb, this, 0);

    if (res < 0) {
        uvc_perror(res, "start_streaming"); /* unable to start stream */
        uvc_close(devh);
        puts("Device closed");

        return;
    }
#endif
}
