#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QTimer;
class QToolButton;

namespace QTerm {
class QTermTerminal;
}

/*
    Find bar pinned to the top-right of the terminal, the way editors and
    browsers do it -- a modal dialog would block typing into the session, and
    the matches are only meaningful while the output stays visible.

    An overlay child of the terminal tab rather than a row in its layout: adding
    a row would resize the terminal, which reflows the buffer and moves the very
    text the user is looking for.
*/
class SearchBar : public QWidget
{
    Q_OBJECT

public:
    SearchBar(QTerm::QTermTerminal *terminal, QWidget *parent = nullptr);

    // Seeds the field and takes focus. Called on every open, not just the
    // first, so reopening with a fresh selection replaces the old query.
    void activate(const QString &initialQuery);

    void applyTheme();

signals:
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void runSearch();
    void updateStatus();

    QTerm::QTermTerminal *m_terminal = nullptr;
    QLineEdit *m_field = nullptr;
    QLabel *m_status = nullptr;
    QToolButton *m_previous = nullptr;
    QToolButton *m_next = nullptr;
    QToolButton *m_caseToggle = nullptr;
    QToolButton *m_close = nullptr;
    QTimer *m_debounce = nullptr;
};
