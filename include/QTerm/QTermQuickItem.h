#ifndef QTERM_QTERMQUICKITEM_H
#define QTERM_QTERMQUICKITEM_H

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QString>
#include <QTimer>

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

    // IME support
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

signals:
    void terminalChanged();
    void fontChanged();
    void metricsChanged();
    void paletteChanged();
    void cursorOpacityChanged();
    void wheelScrolled(int scrollOffset);
    // 请求外部将 text 写入系统剪贴板（QML/C++ 均可连接）
    void copyRequested(const QString &text);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void reconnectSurfaceModel();
    void disconnectSurfaceModel();
    void updateMetrics();
    void scheduleTerminalSizeSync();
    void syncTerminalSize();

    // 根据当前拖拽坐标更新选区（供拖拽和 auto-scroll 共用）
    void updateSelectionFromDrag(qreal x, qreal y);

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

    // ── 鼠标 / 选区内部状态 ───────────────────────────────────────────────
    QTimer *m_selectionAutoScrollTimer = nullptr;
    QTimer *m_clickResetTimer = nullptr;

    int m_clickStreak = 0;
    int m_lastClickRow = -1;
    int m_lastClickColumn = -1;

    int m_selectionAnchorRow = -1;
    int m_selectionAnchorColumn = -1;
    bool m_suppressSelectionRelease = false;

    qreal m_dragX = 0.0;
    qreal m_dragY = 0.0;
    int m_autoScrollDirection = 0; // +1 = 向上, -1 = 向下
};

} // namespace QTerm

#endif // QTERM_QTERMQUICKITEM_H