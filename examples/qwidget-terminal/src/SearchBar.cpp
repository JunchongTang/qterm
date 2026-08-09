#include "SearchBar.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include <QTerm/QTermTerminal.h>

SearchBar::SearchBar(QTerm::QTermTerminal *terminal, QWidget *parent)
    : QWidget(parent)
    , m_terminal(terminal)
{
    setObjectName(QStringLiteral("searchBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(40);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(Theme::instance()->space3(), 0,
                               Theme::instance()->space3(), 0);
    layout->setSpacing(Theme::instance()->space2());

    m_field = new QLineEdit;
    m_field->setPlaceholderText(tr("Find"));
    m_field->setFixedWidth(200);
    m_field->installEventFilter(this);

    m_status = new QLabel;
    m_status->setFixedWidth(58);
    m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_status->setProperty("muted", true);

    const auto makeButton = [](const QString &tip) {
        auto *button = new QToolButton;
        button->setObjectName(QStringLiteral("barButton"));
        button->setFixedSize(24, 24);
        button->setIconSize(QSize(12, 12));
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };

    m_previous = makeButton(tr("Previous match"));
    m_next = makeButton(tr("Next match"));

    // Case sensitivity as a toggle rather than a checkbox, to keep the bar
    // compact; the checked state is what the style sheet keys off.
    m_caseToggle = new QToolButton;
    m_caseToggle->setObjectName(QStringLiteral("caseToggle"));
    m_caseToggle->setText(tr("Aa"));
    m_caseToggle->setCheckable(true);
    m_caseToggle->setFixedSize(26, 24);
    m_caseToggle->setToolTip(tr("Match case"));
    m_caseToggle->setCursor(Qt::PointingHandCursor);

    m_close = makeButton(tr("Close"));

    layout->addWidget(m_field);
    layout->addWidget(m_status);
    layout->addWidget(m_previous);
    layout->addWidget(m_next);
    layout->addWidget(m_caseToggle);
    layout->addWidget(m_close);

    // Searching rescans the whole buffer including scrollback, so it waits for
    // a pause in typing rather than running on every keystroke.
    m_debounce = new QTimer(this);
    m_debounce->setInterval(150);
    m_debounce->setSingleShot(true);
    connect(m_debounce, &QTimer::timeout, this, &SearchBar::runSearch);

    connect(m_field, &QLineEdit::textChanged, this, [this] { m_debounce->start(); });
    connect(m_previous, &QToolButton::clicked, this, [this] {
        m_terminal->findPrevious();
    });
    connect(m_next, &QToolButton::clicked, this, [this] {
        m_terminal->findNext();
    });
    connect(m_caseToggle, &QToolButton::toggled, this, &SearchBar::runSearch);
    connect(m_close, &QToolButton::clicked, this, &SearchBar::closeRequested);
    connect(m_terminal, &QTerm::QTermTerminal::searchChanged,
            this, &SearchBar::updateStatus);

    applyTheme();
    updateStatus();
}

void SearchBar::activate(const QString &initialQuery)
{
    if (!initialQuery.isEmpty())
        m_field->setText(initialQuery);
    m_field->setFocus();
    m_field->selectAll();
    runSearch();
}

namespace {

// Carries its own disabled appearance: with a style sheet in play the button
// keeps drawing the normal icon at full strength when disabled, which reads as
// "greyed-out control that still works".
QIcon dimmableIcon(const QString &name, const QColor &tint, int size)
{
    const Theme *theme = Theme::instance();
    QIcon icon = theme->icon(name, tint, size);
    const QIcon dim = theme->icon(name, theme->disabledForeground(), size);
    icon.addPixmap(dim.pixmap(QSize(size, size)), QIcon::Disabled);
    return icon;
}

} // namespace

void SearchBar::applyTheme()
{
    const Theme *theme = Theme::instance();
    m_previous->setIcon(dimmableIcon(QStringLiteral("chevron-up"), theme->foreground(), 12));
    m_next->setIcon(dimmableIcon(QStringLiteral("chevron-down"), theme->foreground(), 12));
    m_close->setIcon(theme->icon(QStringLiteral("x"), theme->mutedForeground(), 12));
}

void SearchBar::runSearch()
{
    if (m_field->text().isEmpty())
        m_terminal->clearSearch();
    else
        m_terminal->search(m_field->text(), m_caseToggle->isChecked());
    updateStatus();
}

void SearchBar::updateStatus()
{
    const int matches = m_terminal->searchMatchCount();
    const bool noResults = !m_field->text().isEmpty() && matches == 0;

    if (noResults)
        m_status->setText(tr("none"));
    else if (matches == 0)
        m_status->clear();
    else
        m_status->setText(QStringLiteral("%1/%2")
                          .arg(m_terminal->searchCurrentIndex()).arg(matches));

    // The muted/destructive split is a property the style sheet selects on, so
    // the widget has to be repolished for the change to take effect.
    m_status->setProperty("muted", !noResults);
    m_status->setProperty("invalid", noResults);
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);

    m_field->setProperty("invalid", noResults);
    m_field->style()->unpolish(m_field);
    m_field->style()->polish(m_field);

    m_previous->setEnabled(matches > 0);
    m_next->setEnabled(matches > 0);
}

bool SearchBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_field && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                m_terminal->findPrevious();
            else
                m_terminal->findNext();
            return true;
        case Qt::Key_Escape:
            emit closeRequested();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}
