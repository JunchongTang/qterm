#include "TerminalTab.h"

#include "MenuItemWidget.h"
#include "SearchBar.h"
#include "Theme.h"

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QUrl>

#include <QTerm/QTermLocalShellBackend.h>
#include <QTerm/QTermSerialBackend.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTelnetBackend.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>
#include <QTerm/QTermWidget.h>

namespace {

// ANSI palettes matching the Qt Quick demo so both frontends look identical.
QTerm::QTermTheme terminalTheme(bool dark)
{
    QTerm::QTermTheme theme = dark ? QTerm::QTermTheme::dark() : QTerm::QTermTheme::light();
    if (dark) {
        theme.setForeground(QColor(QStringLiteral("#F2F2F2")));
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
    } else {
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
    }
    return theme;
}

QString defaultFontFamily()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Consolas");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Menlo");
#else
    return QStringLiteral("Monospace");
#endif
}

} // namespace

TerminalTab::TerminalTab(const SessionConfig &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(Theme::instance()->space2(), Theme::instance()->space2(),
                               0, Theme::instance()->space2());
    layout->setSpacing(0);

    m_terminal = new QTerm::QTermTerminal(this);
    m_session = new QTerm::QTermSession(this);

    m_view = new QTerm::QTermWidget(this);
    m_view->setTerminal(m_terminal);
    m_view->setFontFamily(defaultFontFamily());
    m_view->setFontPixelSize(16);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_scrollBar = new QScrollBar(Qt::Vertical, this);
    m_scrollBar->setRange(0, 1000);

    layout->addWidget(m_view);
    layout->addWidget(m_scrollBar);

    applyTheme();
    connect(Theme::instance(), &Theme::darkChanged, this, &TerminalTab::applyTheme);

    connect(m_view, &QTerm::QTermWidget::scrollChanged, this, &TerminalTab::updateScrollBar);
    connect(m_view, &QTerm::QTermWidget::wheelScrolled, this, &TerminalTab::updateScrollBar);
    connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        const int span = m_scrollBar->maximum() - m_scrollBar->minimum();
        if (span <= 0)
            return;
        const qreal position = qreal(value) / span * (1.0 - m_view->scrollSize());
        if (!qFuzzyCompare(m_view->scrollPosition(), position))
            m_view->setScrollPosition(position);
    });

    connect(m_view, &QTerm::QTermWidget::hyperlinkActivated, this, [this](const QString &url) {
        emit statusMessage(tr("Opening %1").arg(url));
        QDesktopServices::openUrl(QUrl(url));
    });
    connect(m_terminal, &QTerm::QTermTerminal::titleChanged,
            this, &TerminalTab::tabTitleChanged);

    // Right-click only; every other button belongs to the view, which drives
    // selection and the mouse protocol.
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested,
            this, &TerminalTab::showContextMenu);

    installShortcuts();

    m_backend = createBackend(config);
    m_session->setBackend(m_backend);
    m_terminal->setSession(m_session);
    m_session->open();

    updateScrollBar();
}

void TerminalTab::installShortcuts()
{
    buildContextMenu();

    // The context menu's actions carry the shortcuts, so they stay in step with
    // the labels the menu shows. Scoped to this tab, so a sequence only fires
    // for the terminal the user is actually looking at.
    for (QAction *action : m_contextMenu->actions())
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addActions(m_contextMenu->actions());
}

void TerminalTab::buildContextMenu()
{
    // Ctrl+C has to reach the child process as an interrupt, so only macOS can
    // use the plain Copy/Paste sequences (Cmd based there). Elsewhere terminals
    // conventionally add Shift.
#if defined(Q_OS_MACOS)
    const QKeySequence copySequence(QKeySequence::Copy);
    const QKeySequence pasteSequence(QKeySequence::Paste);
    const QKeySequence selectAllSequence(QKeySequence::SelectAll);
#else
    const QKeySequence copySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C);
    const QKeySequence pasteSequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V);
    const QKeySequence selectAllSequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A);
#endif

    m_contextMenu = new QMenu(this);

    const auto item = [this](const QString &text, const QKeySequence &shortcut = QKeySequence()) {
        return MenuItemWidget::addTo(m_contextMenu, text, shortcut);
    };

    m_copyAction = item(tr("Copy"), copySequence);
    connect(m_copyAction, &QAction::triggered, this, &TerminalTab::copySelection);
    m_pasteAction = item(tr("Paste"), pasteSequence);
    connect(m_pasteAction, &QAction::triggered, this, &TerminalTab::pasteFromClipboard);
    connect(item(tr("Select All"), selectAllSequence), &QAction::triggered,
            this, [this] { m_terminal->selectAll(); });

    m_contextMenu->addSeparator();

    connect(item(tr("Find…"), QKeySequence::Find), &QAction::triggered,
            this, &TerminalTab::openFind);
    m_findSelectionAction = item(tr("Find Selection"), QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_findSelectionAction, &QAction::triggered, this, &TerminalTab::openFind);

    m_linkSeparator = m_contextMenu->addSeparator();
    m_openLinkAction = item(tr("Open Link"));
    connect(m_openLinkAction, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(m_menuLinkUrl));
    });
    m_copyLinkAction = item(tr("Copy Link Address"));
    connect(m_copyLinkAction, &QAction::triggered, this, [this] {
        QGuiApplication::clipboard()->setText(m_menuLinkUrl);
    });

    m_contextMenu->addSeparator();

    connect(item(tr("Clear Screen"), QKeySequence(Qt::CTRL | Qt::Key_K)), &QAction::triggered,
            this, &TerminalTab::clearScreen);
    connect(item(tr("Clear Scrollback")), &QAction::triggered,
            this, &TerminalTab::clearScrollback);
    connect(item(tr("Reset Terminal")), &QAction::triggered,
            this, [this] { m_terminal->clear(); });

    m_contextMenu->addSeparator();

    connect(item(tr("New Tab"), QKeySequence(Qt::CTRL | Qt::Key_T)), &QAction::triggered,
            this, &TerminalTab::newTabRequested);
    connect(item(tr("Close Tab"), QKeySequence(Qt::CTRL | Qt::Key_W)), &QAction::triggered,
            this, &TerminalTab::closeTabRequested);
}

void TerminalTab::showContextMenu(const QPoint &position)
{
    const int linkId = hyperlinkIdAt(position);
    m_menuLinkUrl = linkId > 0 ? m_terminal->hyperlinkUrl(linkId) : QString();

    const bool hasSelection = m_terminal->surfaceModel()->hasSelection();
    m_copyAction->setEnabled(hasSelection);
    m_findSelectionAction->setEnabled(hasSelection);
    m_pasteAction->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());

    const bool hasLink = !m_menuLinkUrl.isEmpty();
    m_linkSeparator->setVisible(hasLink);
    m_openLinkAction->setVisible(hasLink);
    m_copyLinkAction->setVisible(hasLink);

    m_contextMenu->popup(m_view->mapToGlobal(position));
}

int TerminalTab::hyperlinkIdAt(const QPoint &position) const
{
    const int row = m_view->rowAtPosition(position.y());
    const int column = m_view->columnAtPosition(position.x());
    if (row < 0 || column < 0)
        return 0;

    // Style runs carry the OSC 8 link id, so a cell's link is found by walking
    // the row's runs until the one covering that column.
    const QVariantList rows = m_terminal->surfaceModel()->visibleLineRuns();
    if (row >= rows.size())
        return 0;

    int start = 0;
    const QVariantList runs = rows.at(row).toList();
    for (const QVariant &value : runs) {
        const QVariantMap run = value.toMap();
        const int span = run.value(QStringLiteral("columns")).toInt();
        if (column >= start && column < start + span)
            return run.value(QStringLiteral("hyperlinkId")).toInt();
        start += span;
    }
    return 0;
}

void TerminalTab::openFind()
{
    if (!m_searchBar) {
        m_searchBar = new SearchBar(m_terminal, this);
        connect(m_searchBar, &SearchBar::closeRequested, this, &TerminalTab::closeFind);
        connect(Theme::instance(), &Theme::darkChanged, m_searchBar, &SearchBar::applyTheme);
    }

    positionSearchBar();
    m_searchBar->show();
    m_searchBar->raise();
    // Opening with a selection seeds the field, which is what a user who
    // selected something and hit find almost always wants.
    m_searchBar->activate(m_terminal->surfaceModel()->selectedText());
}

void TerminalTab::closeFind()
{
    if (!m_searchBar)
        return;
    m_searchBar->hide();
    m_terminal->clearSearch();
    m_view->setFocus();
}

void TerminalTab::positionSearchBar()
{
    if (!m_searchBar)
        return;
    const QSize hint = m_searchBar->sizeHint();
    // Pinned to the terminal's top-right, clear of the scroll bar.
    m_searchBar->setGeometry(m_view->geometry().right() - hint.width(),
                             m_view->geometry().top() + Theme::instance()->space2(),
                             hint.width(), hint.height());
}

void TerminalTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_searchBar && m_searchBar->isVisible())
        positionSearchBar();
}

// ESC[2J leaves the scrollback intact, matching what the shell's own clear
// command does. Resetting the whole terminal is a separate, heavier action.
void TerminalTab::clearScreen()
{
    m_terminal->feedText(QStringLiteral("\033[H\033[2J"));
}

void TerminalTab::clearScrollback()
{
    m_terminal->feedText(QStringLiteral("\033[3J"));
}

void TerminalTab::copySelection()
{
    const QString text = m_terminal->surfaceModel()->selectedText();
    if (text.isEmpty())
        return;

    QGuiApplication::clipboard()->setText(text);
    emit statusMessage(tr("Copied %n character(s)", nullptr, int(text.size())));
}

void TerminalTab::pasteFromClipboard()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;

    m_terminal->sendPaste(text);
}

TerminalTab::~TerminalTab()
{
    if (m_session)
        m_session->close();
}

QString TerminalTab::tabTitle() const
{
    const QString title = m_terminal ? m_terminal->title() : QString();
    if (!title.isEmpty())
        return title;
    return m_config.label.isEmpty() ? tr("Terminal") : m_config.label;
}

void TerminalTab::applyTheme()
{
    m_view->setTheme(terminalTheme(Theme::instance()->isDark()));
}

void TerminalTab::updateScrollBar()
{
    const qreal size = m_view->scrollSize();
    const qreal position = m_view->scrollPosition();
    const int span = 1000;
    const int pageStep = qMax(1, int(size * span));

    m_scrollBar->blockSignals(true);
    m_scrollBar->setPageStep(pageStep);
    m_scrollBar->setMaximum(span - pageStep);
    m_scrollBar->setValue(int(position / qMax(1e-9, 1.0 - size) * m_scrollBar->maximum()));
    m_scrollBar->blockSignals(false);
}

QTerm::QTermSessionBackend *TerminalTab::createBackend(const SessionConfig &config)
{
    switch (config.type) {
    case SessionConfig::Serial: {
        auto *backend = new QTerm::QTermSerialBackend(this);
        backend->setPortName(config.portName);
        backend->setBaudRate(config.baudRate);
        backend->setDataBits(config.dataBits);
        backend->setParity(config.parity);
        backend->setStopBits(config.stopBits);
        backend->setFlowControl(config.flowControl);
        return backend;
    }
    case SessionConfig::Telnet: {
        auto *backend = new QTerm::QTermTelnetBackend(this);
        backend->setHost(config.host);
        backend->setPort(quint16(config.port));
        return backend;
    }
    case SessionConfig::Pty:
        break;
    }

    auto *backend = new QTerm::QTermLocalShellBackend(this);
    if (!config.program.isEmpty())
        backend->setProgram(config.program);
    if (!config.arguments.isEmpty())
        backend->setArguments(config.arguments);
    if (!config.workingDirectory.isEmpty())
        backend->setWorkingDirectory(config.workingDirectory);
    return backend;
}
