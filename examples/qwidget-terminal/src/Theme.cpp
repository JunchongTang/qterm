#include "Theme.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace {

// QSS has no rgba() with a float alpha in every Qt version, so emit the 8-bit
// form that both the parser and QColor::name(HexArgb) agree on.
QString css(const QColor &c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(QString::number(c.alphaF(), 'f', 3));
}

} // namespace

Theme *Theme::instance()
{
    static Theme theme;
    return &theme;
}

void Theme::setDark(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;

    // Also repaints the native window decoration; QStyleHints only gained a
    // colorScheme setter in Qt 6.8.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark
                                                       : Qt::ColorScheme::Light);
#endif

    emit darkChanged();
}

QString Theme::styleSheet() const
{
    const QString fg = css(foreground());
    const QString mutedFg = css(mutedForeground());
    const QString bg = css(background());
    const QString pop = css(popover());
    const QString brd = css(border());
    const QString inp = css(input());
    const QString mut = css(muted());
    const QString pri = css(primary());
    const QString priFg = css(primaryForeground());
    const QString rng = css(ring());

    // Input fill is the token at reduced opacity, matching bg-input/20|30.
    QColor inputFill = input();
    inputFill.setAlphaF(inputFill.alphaF() * (m_dark ? 0.3 : 0.2));

    return QStringLiteral(R"(
/* Only the window roots carry a fill; plain container widgets stay
   transparent so they inherit whichever surface they sit on. Painting every
   QWidget would tint layout containers with the window color even inside a
   popover-colored dialog. */
QWidget {
    color: %2;
    font-size: %3px;
}
MainWindow, TerminalTab {
    background: %1;
}
QWidget#divider {
    background: %22;
}

QLabel {
    background: transparent;
    font-weight: 500;
}
QLabel[muted="true"] {
    color: %4;
}

/* Button: default variant is the solid primary fill. */
QPushButton {
    background: %5;
    color: %6;
    border: none;
    border-radius: %7px;
    min-height: 28px;
    padding: 0 %8px;
    font-weight: 500;
}
QPushButton:hover {
    background: %9;
}
QPushButton:focus {
    border: 1px solid %10;
}
QPushButton:disabled {
    color: %4;
}

/* Outline and ghost variants are selected by a dynamic property. */
QPushButton[variant="outline"] {
    background: transparent;
    color: %2;
    border: 1px solid %11;
}
QPushButton[variant="outline"]:hover {
    background: %12;
}
QPushButton[variant="ghost"] {
    background: transparent;
    color: %2;
    border: none;
}
QPushButton[variant="ghost"]:hover {
    background: %12;
}

QLineEdit {
    background: %13;
    color: %2;
    border: 1px solid %11;
    border-radius: %7px;
    min-height: 28px;
    padding: 0 %14px;
    selection-background-color: %5;
    selection-color: %6;
}
QLineEdit:focus {
    border: 1px solid %10;
}
QLineEdit[invalid="true"] {
    border: 1px solid %15;
}

QComboBox {
    background: %13;
    color: %2;
    border: 1px solid %11;
    border-radius: %7px;
    min-height: 28px;
    padding: 0 %14px;
}
QComboBox:focus {
    border: 1px solid %10;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
/* Styling a QComboBox switches it to QStyleSheetStyle, which stops drawing the
   built-in arrow, so the indicator has to be supplied explicitly. */
QComboBox::down-arrow {
    image: url(:/assets/chevron-down-%21.svg);
    width: 14px;
    height: 14px;
}
QComboBox QAbstractItemView {
    background: %16;
    color: %2;
    border: 1px solid %11;
    border-radius: %17px;
    padding: %18px;
    outline: none;
    selection-background-color: %12;
    selection-color: %2;
}

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %19;
    border-radius: 3px;
    min-height: 24px;
    margin: 2px;
}
QScrollBar::handle:vertical:hover {
    background: %20;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: none;
    height: 0;
}

QDialog {
    background: %16;
}

QToolTip {
    background: %16;
    color: %2;
    border: 1px solid %11;
    padding: 4px;
}
)")
            .arg(bg)                                   // 1
            .arg(fg)                                   // 2
            .arg(textXs())                             // 3
            .arg(mutedFg)                              // 4
            .arg(pri)                                  // 5
            .arg(priFg)                                // 6
            .arg(radiusMd())                           // 7
            .arg(space2())                             // 8
            .arg(css(primary().lighter(110)))          // 9
            .arg(rng)                                  // 10
            .arg(inp)                                  // 11
            .arg(mut)                                  // 12
            .arg(css(inputFill))                       // 13
            .arg(space2())                             // 14
            .arg(css(destructive()))                   // 15
            .arg(pop)                                  // 16
            .arg(radiusLg())                           // 17
            .arg(space1())                             // 18
            .arg(css(QColor(m_dark ? QColor(255, 255, 255, 64)
                                   : QColor(0, 0, 0, 64))))   // 19
            .arg(css(QColor(m_dark ? QColor(255, 255, 255, 110)
                                   : QColor(0, 0, 0, 110))))   // 20
            .arg(m_dark ? QStringLiteral("dark") : QStringLiteral("light")) // 21
            .arg(brd);                                 // 22
}
