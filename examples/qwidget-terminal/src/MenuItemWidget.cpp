#include "MenuItemWidget.h"

#include "Theme.h"

#include <QAction>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>
#include <QWidgetAction>

namespace {

constexpr int kRowHeight = 30;
// Minimum gap between the label and the shortcut hint.
constexpr int kHintGap = 24;

} // namespace

QAction *MenuItemWidget::addTo(QMenu *menu, const QString &text, const QKeySequence &shortcut)
{
    auto *action = new QWidgetAction(menu);
    action->setText(text);
    if (!shortcut.isEmpty())
        action->setShortcut(shortcut);
    auto *item = new MenuItemWidget(action);
    action->setDefaultWidget(item);
    menu->addAction(action);

    // QMenu repaints itself when the active row changes but not the widgets it
    // hosts, so each row repaints on any hover -- which is also what keyboard
    // navigation emits.
    connect(menu, &QMenu::hovered, item, [item] { item->update(); });
    return action;
}

MenuItemWidget::MenuItemWidget(QAction *action, QWidget *parent)
    : QWidget(parent)
    , m_action(action)
{
    setAttribute(Qt::WA_Hover, true);
    // Enabled/disabled is decided just before the menu pops up.
    connect(action, &QAction::changed, this, [this] { update(); });
}

// Leaving the menu without entering another row emits no hovered(), so the
// last highlight would otherwise linger.
void MenuItemWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    update();
}

QSize MenuItemWidget::sizeHint() const
{
    const QFontMetrics metrics(font());
    int width = metrics.horizontalAdvance(m_action->text()) + Theme::instance()->space2() * 2;
    const QString hint = m_action->shortcut().toString(QKeySequence::NativeText);
    if (!hint.isEmpty())
        width += kHintGap + metrics.horizontalAdvance(hint);
    return QSize(width, kRowHeight);
}

void MenuItemWidget::paintEvent(QPaintEvent *)
{
    const Theme *theme = Theme::instance();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // QMenu marks the row under the pointer (or the arrow-key selection) as the
    // active action; there is no pseudo-state to read, so ask the menu.
    bool highlighted = false;
    if (auto *menu = qobject_cast<QMenu *>(parentWidget()))
        highlighted = menu->activeAction() == m_action;

    const bool enabled = m_action->isEnabled();
    if (highlighted && enabled) {
        painter.setBrush(theme->muted());
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect(), theme->radiusSm(), theme->radiusSm());
    }

    painter.setOpacity(enabled ? 1.0 : 0.45);

    const QRect textRect = rect().adjusted(theme->space2(), 0, -theme->space2(), 0);
    painter.setPen(theme->foreground());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_action->text());

    const QString hint = m_action->shortcut().toString(QKeySequence::NativeText);
    if (!hint.isEmpty()) {
        painter.setPen(theme->mutedForeground());
        painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, hint);
    }
}
