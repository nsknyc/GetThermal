#ifndef UVCVIDEOPRODUCER_H
#define UVCVIDEOPRODUCER_H

#include <QObject>
#include <QVideoSink>
#include "uvcacquisition.h"

class UvcVideoProducer : public QObject
{
    Q_OBJECT
public:
    explicit UvcVideoProducer(QObject *parent = 0);

    Q_PROPERTY(QVideoSink *videoSink MEMBER m_sink WRITE setVideoSink NOTIFY videoSinkChanged)
    void setVideoSink(QVideoSink *sink);

    Q_PROPERTY(UvcAcquisition *uvc MEMBER m_uvc WRITE setUvc NOTIFY uvcChanged)
    void setUvc(UvcAcquisition *uvc);

signals:
    void videoSinkChanged(QVideoSink *sink);
    void uvcChanged(UvcAcquisition *uvc);

public slots:
    void onNewVideoContentReceived(const QVideoFrame &frame);

private:
    QVideoSink *m_sink;
    UvcAcquisition *m_uvc;
};

#endif // UVCVIDEOPRODUCER_H
