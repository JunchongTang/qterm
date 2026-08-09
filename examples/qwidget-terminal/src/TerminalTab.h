#pragma once

#include <QWidget>

#include "SessionConfig.h"

class QAction;
class QMenu;
class QPoint;
class QScrollBar;
class SearchBar;

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
    // Forwarded so the context menu can reach the window, which owns the tabs.
    void newTabRequested();
    void closeTabRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyTheme();
    void updateScrollBar();
    void installShortcuts();
    void buildContextMenu();
    void copySelection();
    void pasteFromClipboard();
    void showContextMenu(const QPoint &position);
    void openFind();
    void closeFind();
    void positionSearchBar();
    void clearScreen();
    void clearScrollback();
    // Resolves the OSC 8 link id of the cell at a viewport position, or 0.
    int hyperlinkIdAt(const QPoint &position) const;
    QTerm::QTermSessionBackend *createBackend(const SessionConfig &config);

    SessionConfig m_config;
    QTerm::QTermTerminal *m_terminal = nullptr;
    QTerm::QTermSession *m_session = nullptr;
    QTerm::QTermSessionBackend *m_backend = nullptr;
    QTerm::QTermWidget *m_view = nullptr;
    QScrollBar *m_scrollBar = nullptr;
    // Created on first use; the bar is absent most of the time.
    SearchBar *m_searchBar = nullptr;

    // Built once so the actions can carry live shortcuts; the entries whose
    // state depends on the click are refreshed before each popup.
    QMenu *m_contextMenu = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_findSelectionAction = nullptr;
    QAction *m_linkSeparator = nullptr;
    QAction *m_openLinkAction = nullptr;
    QAction *m_copyLinkAction = nullptr;
    // The link under the cell that was right-clicked, so the link entries act
    // on that cell rather than on wherever the pointer has drifted to.
    QString m_menuLinkUrl;
};
