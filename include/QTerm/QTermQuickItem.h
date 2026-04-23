#ifndef QTERM_QTERMQUICKITEM_H
#define QTERM_QTERMQUICKITEM_H

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QString>

#include <QTerm/QTermTerminal.h>

namespace QTerm {

class QTermQuickItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QTerm::QTermTerminal *terminal READ terminal WRITE setTerminal NOTIFY terminalChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY fontChanged)
    Q_PROPERTY(qreal cellWidth READ cellWidth NOTIFY metricsChanged)
    Q_PROPERTY(qreal cellHeight READ cellHeight NOTIFY metricsChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY paletteChanged)
    Q_PROPERTY(qreal cursorOpacity READ cursorOpacity WRITE setCursorOpacity NOTIFY cursorOpacityChanged)

public:
    explicit QTermQuickItem(QQuickItem *parent = nullptr);

    QTermTerminal *terminal() const noexcept;
    void setTerminal(QTermTerminal *terminal);

    QString fontFamily() const;
    void setFontFamily(const QString &fontFamily);

    int fontPixelSize() const noexcept;
    void setFontPixelSize(int fontPixelSize);

    qreal cellWidth() const noexcept;
    qreal cellHeight() const noexcept;

    QColor foregroundColor() const;
    void setForegroundColor(const QColor &foregroundColor);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &backgroundColor);

    QColor selectionColor() const;
    void setSelectionColor(const QColor &selectionColor);

    QColor cursorColor() const;
    void setCursorColor(const QColor &cursorColor);

    qreal cursorOpacity() const noexcept;
    void setCursorOpacity(qreal cursorOpacity);

    Q_INVOKABLE int rowAtPosition(qreal y) const;
    Q_INVOKABLE int columnAtPosition(qreal x) const;

    void paint(QPainter *painter) override;

signals:
    void terminalChanged();
    void fontChanged();
    void metricsChanged();
    void paletteChanged();
    void cursorOpacityChanged();
    void wheelScrolled(int scrollOffset);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void reconnectSurfaceModel();
    void disconnectSurfaceModel();
    void updateMetrics();
    void scheduleTerminalSizeSync();
    void syncTerminalSize();

    QPointer<QTermTerminal> m_terminal;
    QMetaObject::Connection m_surfaceSizeConnection;
    QMetaObject::Connection m_surfaceCursorConnection;
    QMetaObject::Connection m_surfaceSelectionConnection;
    QMetaObject::Connection m_surfaceVisibleRunsConnection;
    QMetaObject::Connection m_surfaceDestroyedConnection;
    QTimer *m_resizeDebounceTimer = nullptr;
    QString m_fontFamily = QStringLiteral("Menlo");
    int m_fontPixelSize = 18;
    qreal m_cellWidth = 1.0;
    qreal m_cellHeight = 1.0;
    QColor m_foregroundColor = QColor(QStringLiteral("#d2f7d0"));
    QColor m_backgroundColor = QColor(QStringLiteral("#0b1016"));
    QColor m_selectionColor = QColor(QStringLiteral("#214f76"));
    QColor m_cursorColor = QColor(QStringLiteral("#d7fbe0"));
    qreal m_cursorOpacity = 1.0;
};

} // namespace QTerm

#endif // QTERM_QTERMQUICKITEM_H