#include "Theme.h"

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QPixmap>
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

/* Tab close button: the ghost look from the Qt Quick demo's IconButton. */
QToolButton#tabClose {
    background: transparent;
    border: none;
    border-radius: %7px;
    padding: 0;
    margin: 0;
}
QToolButton#tabClose:hover {
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
   built-in arrow, so the indicator has to be supplied explicitly. A style sheet
   can only point at a file, so unlike Theme::icon() this one needs a
   pre-tinted asset per palette. */
QComboBox::down-arrow {
    image: url(:/assets/icons/chevron-down-%21.svg);
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

/* Split button: one shared outline, each half hovering on its own. The halves
   square off the edge they meet at so they read as a single control. */
QWidget#splitButton {
    background: transparent;
    border: 1px solid %11;
    border-radius: %7px;
}
QWidget#splitSeparator {
    background: %11;
    margin: 4px 0;
}
QToolButton#splitPrimary, QToolButton#splitMenu {
    background: transparent;
    border: none;
}
QToolButton#splitPrimary {
    border-top-left-radius: %23px;
    border-bottom-left-radius: %23px;
}
QToolButton#splitMenu {
    border-top-right-radius: %23px;
    border-bottom-right-radius: %23px;
}
QToolButton#splitPrimary:hover, QToolButton#splitMenu:hover {
    background: %12;
}

/* Menus: the popover surface from the Qt Quick demo. QMenu draws its own frame,
   so the padding keeps the items clear of the rounded corners. */
QMenu {
    background: %16;
    color: %2;
    border: 1px solid %11;
    border-radius: %17px;
    padding: %18px;
}
QMenu::item {
    background: transparent;
    border-radius: %24px;
    padding: 6px %8px;
    /* Reserves the gap between the label and the right-aligned shortcut hint. */
    margin: 0;
}
QMenu::item:selected {
    background: %12;
}
QMenu::item:disabled {
    color: %4;
}
QMenu::separator {
    height: 1px;
    background: %22;
    margin: %18px %8px;
}

/* Find bar: a floating popover over the terminal rather than a layout row,
   which would resize the terminal and reflow the text being searched. */
QWidget#searchBar {
    background: %16;
    border: 1px solid %11;
    border-radius: %7px;
}
QLabel[invalid="true"] {
    color: %15;
}
QToolButton#barButton, QToolButton#caseToggle {
    background: transparent;
    border: 1px solid transparent;
    border-radius: %24px;
    color: %4;
}
QToolButton#barButton:hover, QToolButton#caseToggle:hover {
    background: %12;
}
QToolButton#barButton:disabled {
    /* QSS cannot fade an icon, so the disabled state reads from the frame. */
    background: transparent;
}
QToolButton#caseToggle:checked {
    background: %12;
    border: 1px solid %10;
    color: %2;
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
            .arg(brd)                                  // 22
            .arg(radiusMd() - 1)                       // 23: inside the 1px outline
            .arg(radiusSm());                          // 24
}

QIcon Theme::icon(const QString &name, const QColor &color, int size) const
{
    const QColor tint = color.isValid() ? color : foreground();

    QFile file(QStringLiteral(":/assets/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};

    // The assets carry stroke="#ffffff"; swapping that literal is enough to
    // retint them, and rendering from the patched SVG keeps the result vector
    // sharp at the requested size.
    QByteArray svg = file.readAll();
    svg.replace("#ffffff", tint.name(QColor::HexRgb).toUtf8());

    QBuffer buffer(&svg);
    QImageReader reader(&buffer, "svg");
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    reader.setScaledSize(QSize(size, size) * dpr);
    const QImage image = reader.read();
    if (image.isNull())
        return {};

    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(dpr);
    return QIcon(pixmap);
}
