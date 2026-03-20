#ifndef UVCACQUISITION_H
#define UVCACQUISITION_H

#include <QList>
#include <QObject>
#include <QVideoFrame>
#include <QVideoFrameFormat>

#include <libuvc/libuvc.h>
#include <unistd.h>

#include "abstractccinterface.h"
#include "dataformatter.h"

class UvcAcquisition : public QObject
{
    Q_OBJECT

public:
    struct UsbId {
        int vid;
        int pid;
    };

    UvcAcquisition(QObject *parent = 0);
    UvcAcquisition(QList<UsbId> ids);
    virtual ~UvcAcquisition();

    Q_PROPERTY(const QVideoFrameFormat& videoFormat READ videoFormat WRITE setVideoFormat NOTIFY formatChanged)
    const QVideoFrameFormat& videoFormat() const { return m_format; }

    Q_PROPERTY(AbstractCCInterface* cci MEMBER m_cci NOTIFY cciChanged)

    Q_PROPERTY(DataFormatter* dataFormatter READ getDataFormatter() NOTIFY dataFormatterChanged)
    DataFormatter* getDataFormatter() { return &m_df; }

    Q_PROPERTY(const QSize& videoSize READ getVideoSize NOTIFY videoSizeChanged)
    const QSize getVideoSize() { return m_format.frameSize(); }

signals:
    void frameReady(const QVideoFrame &frame);
    void formatChanged(const QVideoFrameFormat &format);
    void cciChanged(AbstractCCInterface *format);
    void dataFormatterChanged(AbstractCCInterface *format);
    void videoSizeChanged(const QSize &size);

public slots:
    void setVideoFormat(const QVideoFrameFormat &format);

    void pauseStream();
    void resumeStream();

protected:
    uvc_context_t *ctx;
    uvc_device_t *dev;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    QVideoFrameFormat m_format;
    uvc_frame_format m_uvcFrameFormat;
    AbstractCCInterface *m_cci;
    DataFormatter m_df;

private:
    static void cb(uvc_frame_t *frame, void *ptr);
    void emitFrameReady(const QVideoFrame &frame);
    void init();
    QList<UsbId> _ids;
};

#endif // UVCACQUISITION_H
