#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QShortcut>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <QTerm/QTermLocalPtyBackend.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermThemePack.h>
#include <QTerm/QTermWidget.h>

// ── Main window ───────────────────────────────────────────────────────────────

class DemoWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DemoWindow(QTerm::QTermTerminal *terminal, QWidget *parent = nullptr)
        : QMainWindow(parent)
        , m_themePack(QTerm::QTermThemePack::qtermDefault())
    {
        setWindowTitle(QStringLiteral("QTerm Widget Demo"));
        resize(900, 600);

        // ── Central area: terminal + scrollbar ──────────────────────────────
        QWidget *central = new QWidget(this);
        QHBoxLayout *hbox = new QHBoxLayout(central);
        hbox->setContentsMargins(0, 0, 0, 0);
        hbox->setSpacing(0);

        m_term = new QTerm::QTermWidget(central);
        m_term->setTerminal(terminal);
        m_term->setFontFamily(QStringLiteral("Menlo"));
        m_term->setFontPixelSize(16);
        m_term->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        m_scrollBar = new QScrollBar(Qt::Vertical, central);
        m_scrollBar->setRange(0, 1000);   // logical; updated below
        m_scrollBar->setSingleStep(1);
        m_scrollBar->setPageStep(100);

        hbox->addWidget(m_term);
        hbox->addWidget(m_scrollBar);
        setCentralWidget(central);

        // ── Status bar ──────────────────────────────────────────────────────
        m_statusLabel = new QLabel(tr("Ready"), this);
        statusBar()->addPermanentWidget(m_statusLabel);

        // ── Theme menu ──────────────────────────────────────────────────────
        QMenu *themeMenu = menuBar()->addMenu(tr("Theme"));
        auto *darkAction  = themeMenu->addAction(tr("Dark"));
        auto *lightAction = themeMenu->addAction(tr("Light"));
        darkAction->setCheckable(true);
        lightAction->setCheckable(true);
        darkAction->setChecked(true);

        connect(darkAction, &QAction::triggered, this, [=]() {
            m_term->setTheme(m_themePack.variant(QStringLiteral("dark")));
            darkAction->setChecked(true);
            lightAction->setChecked(false);
        });
        connect(lightAction, &QAction::triggered, this, [=]() {
            m_term->setTheme(m_themePack.variant(QStringLiteral("light")));
            darkAction->setChecked(false);
            lightAction->setChecked(true);
        });

        // Apply default theme (dark).
        m_term->setTheme(m_themePack.variant(QStringLiteral("dark")));

        // ── ScrollBar ↔ terminal sync ────────────────────────────────────────
        // Terminal → ScrollBar
        connect(m_term, &QTerm::QTermWidget::scrollChanged, this, [this]() {
            updateScrollBar();
        });
        connect(m_term, &QTerm::QTermWidget::wheelScrolled, this, [this]() {
            updateScrollBar();
        });

        // ScrollBar → Terminal
        connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int value) {
            // scrollBar value: 0 = top (oldest), max = bottom (newest)
            // QTermWidget scrollPosition: 0 = top, trackSpan = bottom
            const int rangeSpan = m_scrollBar->maximum() - m_scrollBar->minimum();
            if (rangeSpan <= 0) return;
            const qreal pos = static_cast<qreal>(value) / rangeSpan
                              * (1.0 - m_term->scrollSize());
            // Avoid feedback loop: only set if meaningfully different
            if (!qFuzzyCompare(m_term->scrollPosition(), pos))
                m_term->setScrollPosition(pos);
        });

        // ── Clipboard shortcuts ──────────────────────────────────────────────
        auto *copyShortcut = new QShortcut(QKeySequence::Copy, m_term);
        connect(copyShortcut, &QShortcut::activated, this, [this]() {
            if (!m_term->terminal()) return;
            const QString text = m_term->terminal()->surfaceModel()->selectedText();
            if (!text.isEmpty()) {
                QApplication::clipboard()->setText(text);
                m_statusLabel->setText(tr("Copied"));
            }
        });

        auto *pasteShortcut = new QShortcut(QKeySequence::Paste, m_term);
        connect(pasteShortcut, &QShortcut::activated, this, [this]() {
            if (!m_term->terminal()) return;
            const QString text = QApplication::clipboard()->text();
            if (!text.isEmpty())
                m_term->terminal()->sendPaste(text);
        });

        // ── Hyperlink ────────────────────────────────────────────────────────
        connect(m_term, &QTerm::QTermWidget::hyperlinkActivated,
                this, [this](const QString &url) {
            m_statusLabel->setText(tr("Opening: ") + url);
            QDesktopServices_openUrl(url); // via Qt::openUrlExternally workaround below
        });

        // ── Title from terminal ──────────────────────────────────────────────
        connect(terminal, &QTerm::QTermTerminal::titleChanged,
                this, [this, terminal]() {
            const QString t = terminal->title();
            setWindowTitle(t.isEmpty() ? QStringLiteral("QTerm Widget Demo") : t);
        });

        // ── copyRequested (unused here, but forward for completeness) ────────
        connect(m_term, &QTerm::QTermWidget::copyRequested,
                this, [this](const QString &text) {
            QApplication::clipboard()->setText(text);
        });

        updateScrollBar();
    }

private:
    void updateScrollBar()
    {
        const qreal size = m_term->scrollSize();
        const qreal pos  = m_term->scrollPosition();
        const int   span = 1000;

        // page step proportional to visible fraction
        const int pageStep = qMax(1, static_cast<int>(size * span));
        m_scrollBar->blockSignals(true);
        m_scrollBar->setPageStep(pageStep);
        m_scrollBar->setMaximum(span - pageStep);
        m_scrollBar->setValue(static_cast<int>(pos / qMax(1e-9, 1.0 - size) * m_scrollBar->maximum()));
        m_scrollBar->blockSignals(false);
    }

    // Thin wrapper so we don't need to include QDesktopServices in the header
    static void QDesktopServices_openUrl(const QString &url);

    QTerm::QTermWidget *m_term = nullptr;
    QScrollBar *m_scrollBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTerm::QTermThemePack m_themePack;
};

// ── QDesktopServices shim ─────────────────────────────────────────────────────
#include <QDesktopServices>
#include <QUrl>

void DemoWindow::QDesktopServices_openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qterm-widget-demo"));

    QTerm::QTermTerminal terminal;
    QTerm::QTermSession session;
    QTerm::QTermLocalPtyBackend backend;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    const QString shellProgram = env.value(QStringLiteral("SHELL"), QStringLiteral("/bin/zsh"));
    const QString shellName    = QFileInfo(shellProgram).fileName();

    backend.setProgram(shellProgram);
    if (shellName == QStringLiteral("bash") || shellName == QStringLiteral("zsh"))
        backend.setArguments({QStringLiteral("-il")});
    else
        backend.setArguments({QStringLiteral("-i")});
    backend.setProcessEnvironment(env);

    session.setBackend(&backend);
    terminal.setSession(&session);
    terminal.setTitle(QStringLiteral("QTerm Widget Demo"));
    session.open();

    DemoWindow window(&terminal);
    window.show();

    return app.exec();
}

#include "main.moc"
