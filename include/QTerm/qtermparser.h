#pragma once

#include <QByteArray>
#include <QObject>
#include <QSize>

class QTermSurfaceModel;

class QTermParser : public QObject
{
    Q_OBJECT

public:
    explicit QTermParser(QObject *parent = nullptr);
    ~QTermParser() override;

    QTermSurfaceModel *surfaceModel() const;
    virtual void setSurfaceModel(QTermSurfaceModel *surfaceModel);

    virtual void reset() = 0;
    virtual void setTerminalSize(const QSize &size) = 0;
    virtual void processBytes(const QByteArray &data) = 0;

protected:
    QTermSurfaceModel *m_surfaceModel = nullptr;
};