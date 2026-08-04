#pragma once

#include <QWidget>

#include "SessionConfig.h"

class QLabel;
class QPushButton;
class QStackedWidget;
class QTabBar;

class TerminalTab;

// Tab strip plus a stack of terminal sessions, matching the Qt Quick demo's
// layout: tabs and a new-session button on the left, theme toggle on the right.
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void openNewSessionDialog();
    void addTab(const SessionConfig &config);
    void closeTab(int index);
    void applyTheme();
    void updateEmptyState();
    void updateWindowTitle();

    QTabBar *m_tabBar = nullptr;
    QPushButton *m_newTabButton = nullptr;
    QPushButton *m_themeButton = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_emptyState = nullptr;
    QLabel *m_emptyIcon = nullptr;
    QList<QPushButton *> m_newSessionButtons;
    QLabel *m_statusLabel = nullptr;
    QList<TerminalTab *> m_tabs;
};
