#pragma once

#include <QColor>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>

namespace QTerm {

class QTermViewController;

class QTermWidget : public QWidget
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
    Q_PROPERTY(QTerm::QTermWidget::CursorStyle cursorStyle READ cursorStyle WRITE setCursorStyle NOTIFY cursorStyleChanged)
    Q_PROPERTY(qreal scrollPosition READ scrollPosition WRITE setScrollPosition NOTIFY scrollChanged)
    Q_PROPERTY(qreal scrollSize READ scrollSize NOTIFY scrollChanged)
    Q_PROPERTY(QTerm::QTermTheme theme READ theme WRITE setTheme NOTIFY themeChanged)

public:
    enum CursorStyle {
        Block     = 0,
        Underline = 1,
        Bar       = 2
    };
    Q_ENUM(CursorStyle)

    explicit QTermWidget(QWidget *parent = nullptr);

    QTermTerminal *terminal() const noexcept;
    void setTerminal(QTermTerminal *terminal);

    QString fontFamily() const;
    void setFontFamily(const QString &family);

    int fontPixelSize() const noexcept;
    void setFontPixelSize(int size);

    qreal cellWidth() const noexcept;
    qreal cellHeight() const noexcept;

    QColor foregroundColor() const;
    void setForegroundColor(const QColor &color);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);

    QColor selectionColor() const;
    void setSelectionColor(const QColor &color);

    QColor cursorColor() const;
    void setCursorColor(const QColor &color);

    qreal cursorOpacity() const noexcept;
    void setCursorOpacity(qreal opacity);

    CursorStyle cursorStyle() const noexcept;
    void setCursorStyle(CursorStyle style);

    qreal scrollPosition() const noexcept;
    void setScrollPosition(qreal position);
    qreal scrollSize() const noexcept;

    Q_INVOKABLE int rowAtPosition(qreal y) const;
    Q_INVOKABLE int columnAtPosition(qreal x) const;

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    QSize sizeHint() const override;

    QTermTheme theme() const;
    void setTheme(const QTermTheme &theme);

signals:
    void terminalChanged();
    void fontChanged();
    void metricsChanged();
    void paletteChanged();
    void cursorOpacityChanged();
    void cursorStyleChanged();
    void scrollChanged();
    void wheelScrolled(int scrollOffset);
    void copyRequested(const QString &text);
    void hyperlinkActivated(const QString &url);
    void themeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void updateMouseAcceptance();

    QTermViewController *m_controller  = nullptr;

    QTermTheme  m_theme;   // current theme; individual color members follow it
    QColor      m_foregroundColor = QColor(QStringLiteral("#d2f7d0"));
    QColor      m_backgroundColor = QColor(QStringLiteral("#0b1016"));
    QColor      m_selectionColor  = QColor(QStringLiteral("#214f76"));
    QColor      m_cursorColor     = QColor(QStringLiteral("#d7fbe0"));
    qreal       m_cursorOpacity   = 1.0;
    CursorStyle m_cursorStyle     = Block;

    // Incremental dirty-row set; non-empty only between contentRowsDirty and paint.
    QVector<int> m_dirtyRows;
};

} // namespace QTerm
