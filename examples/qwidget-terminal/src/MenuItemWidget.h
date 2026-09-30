#pragma once

#include <QWidget>

class QAction;
class QMenu;

/*
    One row of a menu: label on the left, shortcut hint right-aligned in muted
    colour, accent fill on hover -- the same anatomy as the Qt Quick demo's
    MenuItem.qml, which also replaces the control's content item.

    A plain QAction cannot produce this: as soon as a style sheet touches QMenu,
    Qt switches the menu to QStyleSheetStyle, which draws the label but not the
    shortcut column, so the hints disappear. Drawing the row ourselves is what
    keeps the hints and the shadcn look in the same menu.
*/
class MenuItemWidget : public QWidget
{
    Q_OBJECT

public:
    // Adds an entry to \a menu and returns its action, so callers keep the
    // usual QAction handle for connecting, enabling and hiding.
    static QAction *addTo(QMenu *menu, const QString &text,
                          const QKeySequence &shortcut = QKeySequence());

    MenuItemWidget(QAction *action, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QAction *m_action = nullptr;
};
