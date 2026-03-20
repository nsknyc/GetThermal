#include "uvcvideoproducer.h"
#include "uvcacquisition.h"

UvcVideoProducer::UvcVideoProducer(QObject *parent)
    : QObject(parent)
    , m_sink(NULL)
    , m_uvc(NULL)
{
}

void UvcVideoProducer::setVideoSink(QVideoSink *sink)
{
    m_sink = sink;
    emit videoSinkChanged(m_sink);
}

void UvcVideoProducer::setUvc(UvcAcquisition *uvc)
{
    if (m_uvc)
        disconnect(m_uvc, &UvcAcquisition::frameReady,
                   this, &UvcVideoProducer::onNewVideoContentReceived);

    m_uvc = uvc;
    emit uvcChanged(uvc);

    connect(m_uvc, &UvcAcquisition::frameReady,
            this, &UvcVideoProducer::onNewVideoContentReceived);
}

void UvcVideoProducer::onNewVideoContentReceived(const QVideoFrame &frame)
{
    if (m_sink)
        m_sink->setVideoFrame(frame);
}
