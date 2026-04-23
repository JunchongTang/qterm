#include <QTerm/QTermTerminal.h>

#include "QTermCore.h"

#include <QString>
#include <QtGlobal>

namespace QTerm {

namespace {

struct NormalizedSelectionRange
{
    bool hasSelection = false;
    int startRow = 0;
    int startColumn = 0;
    int endRow = 0;
    int endColumn = 0;
};

enum class TokenClass {
    Empty,
    Space,
    Word,
    Punctuation,
};

struct LogicalColumnRef
{
    int row = 0;
    int column = 0;
    QString text;
};

NormalizedSelectionRange normalizeSelectionRange(int rows,
                                                 int columns,
                                                 int startRow,
                                                 int startColumn,
                                                 int endRow,
                                                 int endColumn)
{
    NormalizedSelectionRange range;

    const int boundedStartRow = qBound(0, startRow, qMax(0, rows - 1));
    const int boundedEndRow = qBound(0, endRow, qMax(0, rows - 1));
    const int boundedStartColumn = qBound(0, startColumn, columns);
    const int boundedEndColumn = qBound(0, endColumn, columns);

    const bool startBeforeEnd = boundedStartRow < boundedEndRow ||
        (boundedStartRow == boundedEndRow && boundedStartColumn <= boundedEndColumn);

    range.startRow = startBeforeEnd ? boundedStartRow : boundedEndRow;
    range.startColumn = startBeforeEnd ? boundedStartColumn : boundedEndColumn;
    range.endRow = startBeforeEnd ? boundedEndRow : boundedStartRow;
    range.endColumn = startBeforeEnd ? boundedEndColumn : boundedStartColumn;
    range.hasSelection = range.startRow != range.endRow || range.startColumn != range.endColumn;

    return range;
}

int graphemeEndColumn(const QStringList &lineColumns, int startColumn)
{
    int column = startColumn + 1;
    while (column < lineColumns.size() && lineColumns.at(column).isEmpty()) {
        ++column;
    }

    return column;
}

int previousLeadingColumn(const QStringList &lineColumns, int startColumn)
{
    int column = startColumn - 1;
    while (column >= 0 && lineColumns.at(column).isEmpty()) {
        --column;
    }

    return column;
}

TokenClass classifyColumnText(const QString &columnText)
{
    if (columnText.isEmpty()) {
        return TokenClass::Empty;
    }

    if (columnText == QStringLiteral(" ")) {
        return TokenClass::Space;
    }

    const QChar firstCharacter = columnText.at(0);
    if (firstCharacter.isLetterOrNumber() || firstCharacter == u'_') {
        return TokenClass::Word;
    }

    return TokenClass::Punctuation;
}

QVector<LogicalColumnRef> collectLogicalLineColumns(const QTermBuffer &buffer, int row)
{
    QVector<LogicalColumnRef> logicalColumns;

    int logicalStartRow = row;
    while (logicalStartRow > 0 && buffer.lineAt(logicalStartRow - 1).wrappedToNextLine()) {
        --logicalStartRow;
    }

    int currentRow = logicalStartRow;
    while (currentRow < buffer.rows()) {
        const QTermLine &line = buffer.lineAt(currentRow);
        const QStringList lineColumns = line.columnTexts();
        for (int column = 0; column < lineColumns.size(); ++column) {
            logicalColumns.append(LogicalColumnRef{currentRow, column, lineColumns.at(column)});
        }

        if (!line.wrappedToNextLine()) {
            break;
        }

        ++currentRow;
    }

    return logicalColumns;
}

void syncSurfaceCursor(QTermSurfaceModel &surfaceModel, QTermCore *core)
{
    const QTermCursorState cursorState = core->cursorState();
    surfaceModel.setCursor(cursorState.row, cursorState.column, core->modeState().cursorVisible);
}

} // namespace

QTermTerminal::QTermTerminal(QObject *parent)
    : QObject(parent)
    , m_core(new QTermCore(this))
    , m_surfaceModel(this)
{
    m_surfaceModel.setSelectionController(this);

    connect(m_core, &QTermCore::sizeChanged, this, [this]() {
        m_surfaceModel.setSize(m_core->columns(), m_core->rows());
        syncSurfaceSelection();
        emit sizeChanged();
    });

    connect(m_core, &QTermCore::debugPlainTextChanged, this, [this]() {
        m_surfaceModel.setVisibleLineWrapFlags(m_core->buffer().visibleLineWrapFlags());
        m_surfaceModel.setVisibleLineColumnTexts(m_core->buffer().visibleLineColumnTexts());
        m_surfaceModel.setVisibleLines(m_core->buffer().visibleLineTexts());
        m_surfaceModel.setVisibleLineRuns(m_core->buffer().visibleLineRuns());
        m_surfaceModel.setPlainText(m_core->debugPlainText());
    });

    connect(m_core, &QTermCore::cursorStateChanged, this, [this]() {
        syncSurfaceCursor(m_surfaceModel, m_core);
    });

    connect(m_core, &QTermCore::bell, this, &QTermTerminal::bell);
    connect(m_core, &QTermCore::titleChanged, this, &QTermTerminal::setTitle);

    connect(m_core, &QTermCore::outboundData, this, &QTermTerminal::outboundData);

    m_surfaceModel.setSize(m_core->columns(), m_core->rows());
    syncSurfaceSelection();
    m_surfaceModel.setVisibleLineWrapFlags(m_core->buffer().visibleLineWrapFlags());
    m_surfaceModel.setVisibleLineColumnTexts(m_core->buffer().visibleLineColumnTexts());
    m_surfaceModel.setVisibleLines(m_core->buffer().visibleLineTexts());
    m_surfaceModel.setVisibleLineRuns(m_core->buffer().visibleLineRuns());
    m_surfaceModel.setPlainText(m_core->debugPlainText());
    syncSurfaceCursor(m_surfaceModel, m_core);
}

int QTermTerminal::rows() const noexcept
{
    return m_core->rows();
}

int QTermTerminal::columns() const noexcept
{
    return m_core->columns();
}

QTermSession *QTermTerminal::session() const noexcept
{
    return m_session;
}

QString QTermTerminal::title() const
{
    return m_title;
}

QTermSurfaceModel *QTermTerminal::surfaceModel() noexcept
{
    return &m_surfaceModel;
}

void QTermTerminal::clear()
{
    m_core->clear();
    clearSelection();
}

void QTermTerminal::feedText(const QString &text)
{
    m_core->writePlainText(text);
}

void QTermTerminal::setTerminalSize(int columns, int rows)
{
    m_core->setTerminalSize(columns, rows);
}

void QTermTerminal::clearSelection()
{
    if (!m_hasSelection) {
        return;
    }

    m_hasSelection = false;
    m_selectionStartRow = 0;
    m_selectionStartColumn = 0;
    m_selectionEndRow = 0;
    m_selectionEndColumn = 0;
    syncSurfaceSelection();
}

void QTermTerminal::setSelectionRange(int startRow, int startColumn, int endRow, int endColumn)
{
    const NormalizedSelectionRange range = normalizeSelectionRange(rows(), columns(), startRow, startColumn, endRow, endColumn);
    if (m_hasSelection == range.hasSelection &&
        m_selectionStartRow == range.startRow &&
        m_selectionStartColumn == range.startColumn &&
        m_selectionEndRow == range.endRow &&
        m_selectionEndColumn == range.endColumn) {
        return;
    }

    m_hasSelection = range.hasSelection;
    m_selectionStartRow = range.startRow;
    m_selectionStartColumn = range.startColumn;
    m_selectionEndRow = range.endRow;
    m_selectionEndColumn = range.endColumn;
    syncSurfaceSelection();
}

void QTermTerminal::selectWordAt(int row, int column)
{
    const QTermBuffer &buffer = m_core->buffer();
    if (buffer.rows() <= 0) {
        clearSelection();
        return;
    }

    const int boundedRow = qBound(0, row, buffer.rows() - 1);
    const QVector<LogicalColumnRef> logicalColumns = collectLogicalLineColumns(buffer, boundedRow);
    if (logicalColumns.isEmpty()) {
        clearSelection();
        return;
    }

    int logicalIndex = -1;
    for (int index = 0; index < logicalColumns.size(); ++index) {
        const LogicalColumnRef &logicalColumn = logicalColumns.at(index);
        if (logicalColumn.row == boundedRow && logicalColumn.column == qMax(0, column)) {
            logicalIndex = index;
            break;
        }
    }

    if (logicalIndex < 0) {
        int fallbackColumn = qMax(0, column);
        for (int index = logicalColumns.size() - 1; index >= 0; --index) {
            const LogicalColumnRef &logicalColumn = logicalColumns.at(index);
            if (logicalColumn.row == boundedRow && logicalColumn.column <= fallbackColumn) {
                logicalIndex = index;
                break;
            }
        }
    }

    if (logicalIndex < 0) {
        clearSelection();
        return;
    }

    while (logicalIndex > 0 && logicalColumns.at(logicalIndex).text.isEmpty()) {
        --logicalIndex;
    }

    const TokenClass tokenClass = classifyColumnText(logicalColumns.at(logicalIndex).text);
    if (tokenClass == TokenClass::Empty) {
        clearSelection();
        return;
    }

    int selectionStartIndex = logicalIndex;
    while (selectionStartIndex > 0) {
        int previousIndex = selectionStartIndex - 1;
        while (previousIndex >= 0 && logicalColumns.at(previousIndex).text.isEmpty()) {
            --previousIndex;
        }

        if (previousIndex < 0 || classifyColumnText(logicalColumns.at(previousIndex).text) != tokenClass) {
            break;
        }

        selectionStartIndex = previousIndex;
    }

    int selectionEndIndex = logicalIndex;
    while (selectionEndIndex + 1 < logicalColumns.size()) {
        int nextIndex = selectionEndIndex + 1;
        while (nextIndex < logicalColumns.size() && logicalColumns.at(nextIndex).text.isEmpty()) {
            ++nextIndex;
        }

        if (nextIndex >= logicalColumns.size() || classifyColumnText(logicalColumns.at(nextIndex).text) != tokenClass) {
            break;
        }

        selectionEndIndex = nextIndex;
    }

    const LogicalColumnRef &selectionStart = logicalColumns.at(selectionStartIndex);
    const LogicalColumnRef &selectionEnd = logicalColumns.at(selectionEndIndex);
    const int exclusiveEndColumn = graphemeEndColumn(buffer.lineAt(selectionEnd.row).columnTexts(), selectionEnd.column);
    setSelectionRange(selectionStart.row, selectionStart.column, selectionEnd.row, exclusiveEndColumn);
}

void QTermTerminal::sendKey(int key, const QString &text)
{
    m_core->sendKey(key, text);
}

void QTermTerminal::sendPaste(const QString &text)
{
    m_core->sendPaste(text);
}

void QTermTerminal::setSession(QTermSession *session)
{
    if (m_session == session) {
        return;
    }

    QObject::disconnect(m_sessionDataConnection);
    QObject::disconnect(m_sessionDestroyedConnection);
    QObject::disconnect(m_coreOutboundConnection);
    QObject::disconnect(m_sizeToSessionResizeConnection);

    m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);

    m_session = session;

    if (m_session) {
        m_sessionDataConnection = connect(m_session, &QTermSession::dataReceived, this, [this](const QByteArray &data) {
            m_core->writePlainText(m_sessionUtf8Decoder(data));
        });
        m_sessionDestroyedConnection = connect(m_session, &QObject::destroyed, this, [this]() {
            QObject::disconnect(m_sessionDataConnection);
            QObject::disconnect(m_sessionDestroyedConnection);
            QObject::disconnect(m_coreOutboundConnection);
            QObject::disconnect(m_sizeToSessionResizeConnection);
            m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);
            m_session = nullptr;
            emit sessionChanged();
        });
        m_coreOutboundConnection = connect(m_core, &QTermCore::outboundData, m_session, &QTermSession::writeData);
        m_sizeToSessionResizeConnection = connect(this, &QTermTerminal::sizeChanged, this, [this]() {
            if (m_session) {
                m_session->resize(columns(), rows());
            }
        });
        m_session->resize(columns(), rows());
    }

    emit sessionChanged();
}

void QTermTerminal::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }

    m_title = title;
    emit titleChanged();
}

void QTermTerminal::syncSurfaceSelection()
{
    if (!m_hasSelection) {
        m_surfaceModel.setSelectionSnapshot(false, 0, 0, 0, 0);
        return;
    }

    const NormalizedSelectionRange range = normalizeSelectionRange(rows(),
                                                                   columns(),
                                                                   m_selectionStartRow,
                                                                   m_selectionStartColumn,
                                                                   m_selectionEndRow,
                                                                   m_selectionEndColumn);
    m_hasSelection = range.hasSelection;
    m_selectionStartRow = range.startRow;
    m_selectionStartColumn = range.startColumn;
    m_selectionEndRow = range.endRow;
    m_selectionEndColumn = range.endColumn;
    m_surfaceModel.setSelectionSnapshot(range.hasSelection,
                                        range.startRow,
                                        range.startColumn,
                                        range.endRow,
                                        range.endColumn);
}

} // namespace QTerm