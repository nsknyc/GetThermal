#ifndef ABSTRACTCCINTERFACE_H
#define ABSTRACTCCINTERFACE_H

#include <QObject>
#include <QVideoFrameFormat>

class AbstractCCInterface : public QObject
{
    Q_OBJECT
public:
    AbstractCCInterface(QObject *parent = 0);
    AbstractCCInterface(const AbstractCCInterface &intf);
    virtual ~AbstractCCInterface() { }

    virtual const AbstractCCInterface &operator =(const AbstractCCInterface &format) = 0;

    Q_PROPERTY(const QVideoFrameFormat defaultFormat READ getDefaultFormat)
    virtual const QVideoFrameFormat getDefaultFormat() = 0;

public slots:
    virtual void performFfc() = 0;
};

#endif // ABSTRACTCCINTERFACE_H
