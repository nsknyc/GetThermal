#include "uvcacquisition.h"
#include <QList>
#include <libuvc/libuvc.h>

#include "leptonvariation.h"
#include "bosonvariation.h"
#include "dataformatter.h"

//#define PLANAR_BUFFER 1
//#define ACQ_RGB 1
#define ACQ_Y16 1

#define PT1_VID 0x1e4e
#define PT1_PID 0x0100
#define FLIR_VID 0x09cb

UvcAcquisition::UvcAcquisition(QObject *parent)
    : QObject(parent)
    , ctx(NULL)
    , dev(NULL)
    , devh(NULL)
    , m_uvcFrameFormat(UVC_FRAME_FORMAT_UNKNOWN)
    , m_cci(NULL)
{
    _ids.append({ PT1_VID, PT1_PID });
    _ids.append({ FLIR_VID, 0x0000 }); // any flir camera
    init();
}

UvcAcquisition::UvcAcquisition(QList<UsbId> ids)
    : ctx(NULL)
    , dev(NULL)
    , devh(NULL)
    , m_uvcFrameFormat(UVC_FRAME_FORMAT_UNKNOWN)
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
}

void UvcAcquisition::init()
{
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
}

void UvcAcquisition::setVideoFormat(const QVideoFrameFormat &format)
{
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
}

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

void UvcAcquisition::emitFrameReady(const QVideoFrame &frame)
{
    emit frameReady(frame);
}

void UvcAcquisition::pauseStream() {
    uvc_stop_streaming(devh);
}

void UvcAcquisition::resumeStream() {
    uvc_error_t res = uvc_start_streaming(devh, &ctrl, UvcAcquisition::cb, this, 0);

    if (res < 0) {
        uvc_perror(res, "start_streaming"); /* unable to start stream */
        uvc_close(devh);
        puts("Device closed");

        return;
    }
}
