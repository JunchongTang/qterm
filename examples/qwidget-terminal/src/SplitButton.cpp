#include "SplitButton.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

SplitButton::SplitButton(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("splitButton"));
    // A plain QWidget ignores a style sheet background; the shared outline is
    // drawn by the sheet, so it has to opt in.
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(28);

    auto *layout = new QHBoxLayout(this);
    // 1px all round keeps the halves' hover fill inside the shared border.
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    m_primary = new QToolButton;
    m_primary->setObjectName(QStringLiteral("splitPrimary"));
    m_primary->setFixedSize(31, 26);
    m_primary->setIconSize(QSize(14, 14));
    m_primary->setCursor(Qt::PointingHandCursor);

    auto *separator = new QWidget;
    separator->setObjectName(QStringLiteral("splitSeparator"));
    separator->setFixedWidth(1);

    m_menuButton = new QToolButton;
    m_menuButton->setObjectName(QStringLiteral("splitMenu"));
    m_menuButton->setFixedSize(21, 26);
    m_menuButton->setIconSize(QSize(10, 10));
    m_menuButton->setCursor(Qt::PointingHandCursor);

    layout->addWidget(m_primary);
    layout->addWidget(separator);
    layout->addWidget(m_menuButton);

    connect(m_primary, &QToolButton::clicked, this, &SplitButton::primaryClicked);
    connect(m_menuButton, &QToolButton::clicked, this, &SplitButton::showMenu);
}

void SplitButton::setMenu(QMenu *menu)
{
    m_menu = menu;
}

void SplitButton::setPrimaryToolTip(const QString &text)
{
    m_primary->setToolTip(text);
}

void SplitButton::setMenuToolTip(const QString &text)
{
    m_menuButton->setToolTip(text);
}

void SplitButton::setIcons(const QIcon &primary, const QIcon &menuIndicator)
{
    m_primary->setIcon(primary);
    m_menuButton->setIcon(menuIndicator);
}

void SplitButton::showMenu()
{
    if (!m_menu)
        return;

    // Right-aligned under the button: the menu is wider than the control, and
    // growing to the left keeps it inside the window near the right edge.
    const QPoint below = mapToGlobal(QPoint(width(), height() + Theme::instance()->space1()));
    m_menu->popup(QPoint(below.x() - m_menu->sizeHint().width(), below.y()));
}
