#include "QTermCore.h"

#include "QTermInputExecutor.h"

#include <QtGlobal>

namespace {

constexpr int kMinimumColumns = 2;
constexpr int kMinimumRows = 1;
constexpr int kTabWidth = 8;

} // namespace

namespace QTerm {

QTermCore::QTermCore(QObject *parent)
    : QObject(parent)
    , m_primaryScreen(80, 24)
    , m_alternateScreen(80, 24)
{
}

int QTermCore::rows() const noexcept
{
    return activeScreen().buffer.rows();
}

int QTermCore::columns() const noexcept
{
    return activeScreen().buffer.columns();
}

QString QTermCore::debugPlainText() const
{
    return activeScreen().buffer.debugPlainText();
}

QTermCursorState QTermCore::cursorState() const noexcept
{
    return activeScreen().cursorState;
}

const QTermBuffer &QTermCore::buffer() const noexcept
{
    return activeScreen().buffer;
}

const QTermModeState &QTermCore::modeState() const noexcept
{
    return m_modeState;
}

void QTermCore::clear()
{
    m_primaryScreen.clear();
    m_alternateScreen.clear();
    m_modeState = QTermModeState();
    emit debugPlainTextChanged();
    emit cursorStateChanged();
}

void QTermCore::writePlainText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    QTermInputExecutor executor(m_primaryScreen, m_alternateScreen, m_modeState);
    m_textParser.parse(text, executor);

    emit debugPlainTextChanged();
    emit cursorStateChanged();
}

void QTermCore::setTerminalSize(int columns, int rows)
{
    const int boundedColumns = qMax(columns, kMinimumColumns);
    const int boundedRows = qMax(rows, kMinimumRows);
    if (m_primaryScreen.buffer.columns() == boundedColumns && m_primaryScreen.buffer.rows() == boundedRows) {
        return;
    }

    m_primaryScreen.resize(boundedColumns, boundedRows);
    m_alternateScreen.resize(boundedColumns, boundedRows);
    emit sizeChanged();
    emit debugPlainTextChanged();
    emit cursorStateChanged();
}

const QTermScreenState &QTermCore::activeScreen() const noexcept
{
    return m_modeState.alternateScreenActive ? m_alternateScreen : m_primaryScreen;
}

} // namespace QTerm