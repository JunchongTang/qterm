#include <QApplication>

#include "src/MainWindow.h"
#include "src/Theme.h"

#include <QGuiApplication>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qwidget-terminal"));

    // Keep the native title bar in step with the in-app palette. QStyleHints
    // only gained a colorScheme setter in Qt 6.8.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(
        Theme::instance()->isDark() ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif

    app.setStyleSheet(Theme::instance()->styleSheet());

    MainWindow window;
    window.show();

    return app.exec();
}
