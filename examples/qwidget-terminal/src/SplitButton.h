#pragma once

#include <QWidget>

class QMenu;
class QToolButton;

/*
    Two halves sharing one outline: the left runs the default action straight
    away, the right opens a menu of the alternatives.

    QToolButton's own MenuButtonPopup mode draws a split button too, but its
    hover state applies to the whole widget rather than to the half under the
    pointer, so the two halves would not read as separately clickable. Composing
    two buttons inside a bordered container keeps the Qt Quick demo's behaviour.
*/
class SplitButton : public QWidget
{
    Q_OBJECT

public:
    explicit SplitButton(QWidget *parent = nullptr);

    // Menu ownership stays with the caller; it is popped right-aligned under
    // the button, matching the Qt Quick demo's placement.
    void setMenu(QMenu *menu);
    void setPrimaryToolTip(const QString &text);
    void setMenuToolTip(const QString &text);

    // Re-tinted on every theme change, so the icons are set from outside.
    void setIcons(const QIcon &primary, const QIcon &menuIndicator);

signals:
    void primaryClicked();

private:
    void showMenu();

    QToolButton *m_primary = nullptr;
    QToolButton *m_menuButton = nullptr;
    QMenu *m_menu = nullptr;
};
