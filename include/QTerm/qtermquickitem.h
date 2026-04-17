#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QSizeF>

#include <QTerm/qtermsession.h>
#include <QTerm/qtermsurfacemodel.h>

class QSGNode;
class QSGTexture;
class QWheelEvent;

class QTermQuickItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QSizeF cellSize READ cellSize WRITE setCellSize NOTIFY cellSizeChanged)
    Q_PROPERTY(QTermSurfaceModel *model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QTermSession *session READ session WRITE setSession NOTIFY sessionChanged)

public:
    explicit QTermQuickItem(QQuickItem *parent = nullptr);
    ~QTermQuickItem() override;

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);

    QSizeF cellSize() const;
    void setCellSize(const QSizeF &size);

    QTermSurfaceModel *model() const;
    void setModel(QTermSurfaceModel *model);

    QTermSession *session() const;
    void setSession(QTermSession *session);

signals:
    void backgroundColorChanged();
    void cellSizeChanged();
    void modelChanged();
    void sessionChanged();
    void terminalSizeChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updateData) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void connectModelSignals();
    void synchronizeTerminalSize();
    void releaseTexture();

    QColor m_backgroundColor;
    QSizeF m_cellSize;
    QPointer<QTermSurfaceModel> m_model;
    QPointer<QTermSession> m_session;
    qreal m_accumulatedScrollPixels = 0.0;
    QSGTexture *m_texture = nullptr;
};