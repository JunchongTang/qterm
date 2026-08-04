#pragma once

#include <QWidget>

#include "SessionConfig.h"

class QScrollBar;

namespace QTerm {
class QTermSession;
class QTermSessionBackend;
class QTermTerminal;
class QTermWidget;
}

// One terminal session: core + session + backend + view, with the scrollbar
// wired to the widget's scroll span.
class TerminalTab : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalTab(const SessionConfig &config, QWidget *parent = nullptr);
    ~TerminalTab() override;

    QString tabTitle() const;
    QTerm::QTermTerminal *terminal() const { return m_terminal; }

signals:
    void tabTitleChanged();
    void statusMessage(const QString &message);

private:
    void applyTheme();
    void updateScrollBar();
    QTerm::QTermSessionBackend *createBackend(const SessionConfig &config);

    SessionConfig m_config;
    QTerm::QTermTerminal *m_terminal = nullptr;
    QTerm::QTermSession *m_session = nullptr;
    QTerm::QTermSessionBackend *m_backend = nullptr;
    QTerm::QTermWidget *m_view = nullptr;
    QScrollBar *m_scrollBar = nullptr;
};
