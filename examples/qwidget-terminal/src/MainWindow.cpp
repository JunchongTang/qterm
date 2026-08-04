#include "MainWindow.h"

#include "NewSessionDialog.h"
#include "TerminalTab.h"
#include "Theme.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QSize>
#include <QTabBar>
#include <QVBoxLayout>

#include <QTerm/QTermTerminal.h>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    resize(1280, 820);
    setMinimumSize(760, 480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Tab bar row ──────────────────────────────────────────────────────────
    auto *barRow = new QWidget;
    barRow->setFixedHeight(40);
    auto *barLayout = new QHBoxLayout(barRow);
    barLayout->setContentsMargins(Theme::instance()->space2(), 0,
                                  Theme::instance()->space2(), 0);
    barLayout->setSpacing(Theme::instance()->space1());

    m_tabBar = new QTabBar;
    m_tabBar->setTabsClosable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setFocusPolicy(Qt::NoFocus);

    m_newTabButton = new QPushButton;
    m_newTabButton->setProperty("variant", "ghost");
    m_newTabButton->setFixedSize(28, 28);
    m_newTabButton->setIconSize(QSize(14, 14));
    m_newTabButton->setToolTip(tr("New session"));

    m_themeButton = new QPushButton;
    m_themeButton->setProperty("variant", "ghost");
    m_themeButton->setFixedSize(28, 28);
    m_themeButton->setIconSize(QSize(14, 14));
    m_themeButton->setToolTip(tr("Toggle theme"));

    barLayout->addWidget(m_tabBar);
    barLayout->addWidget(m_newTabButton);
    barLayout->addStretch();
    barLayout->addWidget(m_themeButton);
    root->addWidget(barRow);

    // 1px divider under the tab bar.
    auto *divider = new QWidget;
    divider->setObjectName(QStringLiteral("divider"));
    divider->setFixedHeight(1);
    root->addWidget(divider);

    // ── Session stack + empty state ──────────────────────────────────────────
    m_stack = new QStackedWidget;
    root->addWidget(m_stack, 1);

    m_emptyState = new QWidget;
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setSpacing(Theme::instance()->space2());
    emptyLayout->addStretch();
    m_emptyIcon = new QLabel;
    m_emptyIcon->setAlignment(Qt::AlignHCenter);
    auto *emptyTitle = new QLabel(tr("No open sessions"));
    emptyTitle->setAlignment(Qt::AlignHCenter);
    emptyTitle->setStyleSheet(QStringLiteral("font-size: %1px; font-weight: 500;")
                              .arg(Theme::instance()->textSm()));
    auto *emptyHint = new QLabel(tr("Start a local shell, serial or telnet session."));
    emptyHint->setAlignment(Qt::AlignHCenter);
    emptyHint->setProperty("muted", true);
    auto *emptyButton = new QPushButton(tr("New Session"));
    emptyButton->setIconSize(QSize(14, 14));
    m_newSessionButtons.append(emptyButton);
    emptyLayout->addWidget(m_emptyIcon);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyHint);
    auto *emptyButtonRow = new QHBoxLayout;
    emptyButtonRow->addStretch();
    emptyButtonRow->addWidget(emptyButton);
    emptyButtonRow->addStretch();
    emptyLayout->addLayout(emptyButtonRow);
    emptyLayout->addStretch();
    m_stack->addWidget(m_emptyState);

    m_statusLabel = new QLabel;
    m_statusLabel->setProperty("muted", true);
    m_statusLabel->setContentsMargins(Theme::instance()->space2(), 0,
                                      Theme::instance()->space2(), 0);
    m_statusLabel->setFixedHeight(20);
    root->addWidget(m_statusLabel);

    connect(m_newTabButton, &QPushButton::clicked, this, &MainWindow::openNewSessionDialog);
    connect(emptyButton, &QPushButton::clicked, this, &MainWindow::openNewSessionDialog);
    connect(m_themeButton, &QPushButton::clicked, this, [] {
        Theme::instance()->setDark(!Theme::instance()->isDark());
    });
    connect(Theme::instance(), &Theme::darkChanged, this, &MainWindow::applyTheme);

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0 && index < m_tabs.size())
            m_stack->setCurrentWidget(m_tabs.at(index));
        updateWindowTitle();
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);

    applyTheme();
    updateEmptyState();
    updateWindowTitle();
}

void MainWindow::openNewSessionDialog()
{
    NewSessionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        addTab(dialog.sessionConfig());
}

void MainWindow::addTab(const SessionConfig &config)
{
    auto *tab = new TerminalTab(config, this);
    m_tabs.append(tab);
    m_stack->addWidget(tab);

    const int index = m_tabBar->addTab(tab->tabTitle());
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentWidget(tab);

    connect(tab, &TerminalTab::tabTitleChanged, this, [this, tab] {
        const int i = m_tabs.indexOf(tab);
        if (i >= 0)
            m_tabBar->setTabText(i, tab->tabTitle());
        updateWindowTitle();
    });
    connect(tab, &TerminalTab::statusMessage, this, [this](const QString &message) {
        m_statusLabel->setText(message);
    });

    updateEmptyState();
    updateWindowTitle();
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    TerminalTab *tab = m_tabs.takeAt(index);
    m_tabBar->removeTab(index);
    m_stack->removeWidget(tab);
    tab->deleteLater();

    updateEmptyState();
    updateWindowTitle();
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(Theme::instance()->styleSheet());

    // Mirrors the QML demo: the glyph shows what a click switches to.
    const Theme *t = Theme::instance();
    m_themeButton->setIcon(t->icon(t->isDark() ? QStringLiteral("sun")
                                               : QStringLiteral("moon")));
    m_newTabButton->setIcon(t->icon(QStringLiteral("plus")));
    m_emptyIcon->setPixmap(t->icon(QStringLiteral("terminal"),
                                   t->mutedForeground(), 32).pixmap(32, 32));
    // New-session buttons carry the same leading icon as the QML demo.
    for (QPushButton *b : m_newSessionButtons)
        b->setIcon(t->icon(QStringLiteral("plus"), t->primaryForeground(), 14));

    // Tab strip: active tab is a filled pill with a 1px border, matching
    // shadcn's tabs-trigger.
    const Theme *theme = Theme::instance();
    const QString tabQss = QStringLiteral(R"(
QTabBar::tab {
    background: transparent;
    color: %1;
    border: none;
    border-radius: %2px;
    min-width: 100px;
    max-width: 200px;
    height: 28px;
    padding: 0 %3px;
    margin-right: %4px;
    font-size: %5px;
    font-weight: 500;
}
QTabBar::tab:hover {
    background: %6;
}
QTabBar::tab:selected {
    background: %7;
    color: %8;
    border: 1px solid %9;
}
QTabBar::close-button {
    image: url(:/assets/icons/close-%10.svg);
    width: 12px;
    height: 12px;
    subcontrol-position: right;
}
)")
            .arg(theme->mutedForeground().name(QColor::HexArgb))
            .arg(theme->radiusMd())
            .arg(theme->space2() + 2)
            .arg(theme->space1())
            .arg(theme->textXs())
            .arg(theme->muted().name(QColor::HexArgb))
            .arg(theme->muted().name(QColor::HexArgb))
            .arg(theme->foreground().name(QColor::HexArgb))
            .arg(theme->border().name(QColor::HexArgb))
            .arg(theme->isDark() ? QStringLiteral("dark") : QStringLiteral("light"));
    m_tabBar->setStyleSheet(tabQss);
}

void MainWindow::updateEmptyState()
{
    if (m_tabs.isEmpty())
        m_stack->setCurrentWidget(m_emptyState);
    m_tabBar->setVisible(!m_tabs.isEmpty());
}

void MainWindow::updateWindowTitle()
{
    const int index = m_tabBar->currentIndex();
    if (index >= 0 && index < m_tabs.size()) {
        setWindowTitle(tr("%1 - Qt Widgets Terminal").arg(m_tabs.at(index)->tabTitle()));
        return;
    }
    setWindowTitle(tr("Qt Widgets Terminal"));
}
