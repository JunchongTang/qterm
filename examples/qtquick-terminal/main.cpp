#include <QGuiApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickStyle>
#include <QStyleHints>
#include <QTimer>

#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermQuickItem.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>

namespace {

class DemoClipboardBridge final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void copyText(const QString &text)
    {
        if (QGuiApplication::clipboard())
            QGuiApplication::clipboard()->setText(text);
    }

    Q_INVOKABLE QString clipboardText() const
    {
        if (QGuiApplication::clipboard())
            return QGuiApplication::clipboard()->text();
        return {};
    }
};

class DemoThemeHelper final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // ANSI 16 colors tuned to a Windows-console-like palette.
    static QTerm::QTermTheme windowsLikeDarkTheme()
    {
        QTerm::QTermTheme theme = QTerm::QTermTheme::dark();
        theme.setName(QStringLiteral("Qt Quick Terminal Dark"));
        theme.setForeground(QColor(QStringLiteral("#F2F2F2")));
        // Matches Theme.background in the QML demo so the terminal blends
        // into the surrounding chrome.
        theme.setBackground(QColor(QStringLiteral("#18181B")));
        theme.setSelection(QColor(QStringLiteral("#2A66D9")));
        theme.setCursor(QColor(QStringLiteral("#FFFFFF")));
        theme.setHyperlinkTint(QColor(QStringLiteral("#4AA8FF")));

        static const char *palette[16] = {
            "#0C0C0C", "#C50F1F", "#13A10E", "#C19C00",
            "#0037DA", "#881798", "#3A96DD", "#CCCCCC",
            "#767676", "#E74856", "#16C60C", "#F9F1A5",
            "#3B78FF", "#B4009E", "#61D6D6", "#F2F2F2"
        };
        for (int i = 0; i < 16; ++i)
            theme.setPaletteColor(i, QColor(QString::fromLatin1(palette[i])));
        return theme;
    }

    static QTerm::QTermTheme windowsLikeLightTheme()
    {
        QTerm::QTermTheme theme = QTerm::QTermTheme::light();
        theme.setName(QStringLiteral("Qt Quick Terminal Light"));
        theme.setForeground(QColor(QStringLiteral("#1A1A1A")));
        theme.setBackground(QColor(QStringLiteral("#FFFFFF")));
        theme.setSelection(QColor(QStringLiteral("#B3D4FC")));
        theme.setCursor(QColor(QStringLiteral("#1A1A1A")));
        theme.setHyperlinkTint(QColor(QStringLiteral("#1A66C2")));

        static const char *palette[16] = {
            "#1A1A1A", "#C50F1F", "#0E7A0B", "#8A6D00",
            "#0037DA", "#7A1585", "#0E7490", "#4D4D4D",
            "#767676", "#B02532", "#118A0E", "#9C7B00",
            "#2860C4", "#8E2196", "#0F7C90", "#1A1A1A"
        };
        for (int i = 0; i < 16; ++i)
            theme.setPaletteColor(i, QColor(QString::fromLatin1(palette[i])));
        return theme;
    }

    Q_INVOKABLE void applyTheme(QObject *item, bool dark)
    {
        const auto theme = dark ? windowsLikeDarkTheme() : windowsLikeLightTheme();
        if (auto *t = qobject_cast<QTerm::QTermQuickPaintedItem *>(item))
            t->setTheme(theme);
        else if (auto *t = qobject_cast<QTerm::QTermQuickItem *>(item))
            t->loadTheme(theme);
    }

    // Requests the platform color scheme, which also repaints the native
    // window decoration (title bar) on macOS and Windows. QStyleHints only
    // gained a colorScheme setter in Qt 6.8; on older Qt the title bar stays
    // at whatever the desktop reports.
    Q_INVOKABLE void setColorScheme(bool dark)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark
                                                           : Qt::ColorScheme::Light);
#else
        Q_UNUSED(dark);
#endif
    }
};

// Runs a command in the live demo and reports when its last line is actually
// on screen.
//
// The shell's `time` reports how long the child lived, which is not what the
// user waits for. And a stripped-down harness is not a substitute for the real
// app: measuring one gave numbers three times off, because the QML layer and
// the window size are part of the cost. This drives the real window through
// the real QML.
//
//   QTERM_BENCH=<payload>  QTERM_BENCH_MARKER=<text on its last line>
//   QTERM_BENCH_WARMUP=<n> runs the payload n times first, so the scrollback is
//                          as full as it is in a terminal already in use
class DemoBenchmark final : public QObject
{
    Q_OBJECT

public:
    void start(QQmlApplicationEngine *engine)
    {
        m_payload = qEnvironmentVariable("QTERM_BENCH");
        m_marker = qEnvironmentVariable("QTERM_BENCH_MARKER");
        if (m_payload.isEmpty())
            return;

        m_runsLeft = qEnvironmentVariableIntValue("QTERM_BENCH_WARMUP") + 1;
        QTimer::singleShot(1500, this, [this, engine] { openSession(engine); });
    }

private:
    void openSession(QQmlApplicationEngine *engine)
    {
        if (engine->rootObjects().isEmpty())
            ::exit(1);

        // The demo opens with no session, so create one the way the New Session
        // dialog does, then give the shell time to come up.
        QObject *root = engine->rootObjects().constFirst();
        // A window macOS considers occluded has its compositing skipped, which
        // silently makes a benchmark look faster than the app really is.
        QQmlExpression front(qmlContext(root), root,
                             QStringLiteral("root.raise(); root.requestActivate()"));
        front.evaluate();

        if (qEnvironmentVariableIsSet("QTERM_BENCH_MAXIMIZE")) {
            QQmlExpression maximize(qmlContext(root), root,
                                    QStringLiteral("root.showMaximized()"));
            maximize.evaluate();
            if (maximize.hasError())
                qInfo().noquote() << QStringLiteral("  bench: %1").arg(maximize.error().toString());
        }

        if (qEnvironmentVariableIsSet("QTERM_BENCH_PAINTER")) {
            QQmlExpression renderer(qmlContext(root), root,
                                    QStringLiteral("AppState.useSceneGraphRenderer = false"));
            renderer.evaluate();
            if (renderer.hasError())
                qInfo().noquote() << QStringLiteral("  bench: %1").arg(renderer.error().toString());
        }

        QQmlExpression open(qmlContext(root), root,
                            QStringLiteral("workspace.addTab({type: 'pty', label: 'bench',"
                                           " program: '', arguments: [], workingDirectory: ''})"));
        open.evaluate();
        if (open.hasError())
            qInfo().noquote() << QStringLiteral("  bench: %1").arg(open.error().toString());

        QTimer::singleShot(1500, this, [this, engine] { begin(engine); });
    }

    void begin(QQmlApplicationEngine *engine)
    {
        for (QObject *root : engine->rootObjects()) {
            if (auto *item = root->findChild<QTerm::QTermQuickItem *>()) {
                m_terminal = item->terminal();
                break;
            }
            if (auto *item = root->findChild<QTerm::QTermQuickPaintedItem *>()) {
                m_terminal = item->terminal();
                break;
            }
        }
        if (!m_terminal || !m_terminal->session()) {
            qInfo().noquote() << "  bench: no terminal found";
            ::exit(1);
        }

        qInfo().noquote() << QStringLiteral("  grid %1x%2  渲染器 %3")
                                 .arg(m_terminal->columns()).arg(m_terminal->rows())
                                 .arg(qEnvironmentVariableIsSet("QTERM_BENCH_PAINTER")
                                          ? "QPainter" : "SceneGraph");

        auto *poll = new QTimer(this);
        poll->setInterval(5);
        connect(poll, &QTimer::timeout, this, [this] { checkForMarker(); });
        poll->start();

        sendRun();
    }

    void sendRun()
    {
        // clear first so the previous run's marker is off screen; without it the
        // poll below would match immediately and time nothing.
        m_awaitingMarker = false;
        m_terminal->session()->writeData(QByteArrayLiteral("clear\n"));
        QTimer::singleShot(300, this, [this] {
            if (m_runsLeft == 1)
                m_timer.start();
            m_awaitingMarker = true;
            // Run it under the shell's own `time` so its number can be compared
            // directly against what the user reports.
            m_terminal->session()->writeData(
                QStringLiteral("time cat %1\n").arg(m_payload).toUtf8());
        });
    }

    void checkForMarker()
    {
        if (!m_awaitingMarker)
            return;
        bool found = false;
        for (const QString &line : m_terminal->surfaceModel()->visibleLines()) {
            if (line.contains(m_marker)) {
                found = true;
                break;
            }
        }
        if (!found)
            return;

        m_awaitingMarker = false;
        if (--m_runsLeft <= 0) {
            m_lastLineAt = m_timer.elapsed() / 1000.0;
            // The shell prints its timing after the payload, so wait for it.
            auto *wait = new QTimer(this);
            wait->setInterval(20);
            connect(wait, &QTimer::timeout, this, [this] {
                for (const QString &line : m_terminal->surfaceModel()->visibleLines()) {
                    if (!line.contains(u"total"))
                        continue;
                    qInfo().noquote()
                        << QStringLiteral("  末行显示 %1 s   shell 报告: %2")
                               .arg(m_lastLineAt, 0, 'f', 3).arg(line.trimmed());
                    ::exit(0);
                }
            });
            wait->start();
            return;
        }
        QTimer::singleShot(300, this, [this] { sendRun(); });
    }

    QString m_payload;
    QString m_marker;
    QElapsedTimer m_timer;
    QTerm::QTermTerminal *m_terminal = nullptr;
    int m_runsLeft = 1;
    bool m_awaitingMarker = false;
    double m_lastLineAt = 0;
};

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qtquick-terminal"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    DemoClipboardBridge clipboardBridge;
    DemoThemeHelper themeHelper;

    engine.rootContext()->setContextProperty(QStringLiteral("clipboardBridge"), &clipboardBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("themeHelper"), &themeHelper);

    engine.loadFromModule("QtQuickTerminal", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    DemoBenchmark benchmark;
    benchmark.start(&engine);

    return app.exec();
}

#include "main.moc"

