#include <QTerm/qtermtextparser.h>

#include <algorithm>

#include <QTerm/qtermcell.h>
#include <QTerm/qtermsurfacemodel.h>

QTermTextParser::QTermTextParser(QObject *parent)
    : QTermParser(parent)
{
    reset();
}

QTermTextParser::~QTermTextParser() = default;

void QTermTextParser::setSurfaceModel(QTermSurfaceModel *surfaceModel)
{
    QTermParser::setSurfaceModel(surfaceModel);
    rebuildVisibleCells();
}

void QTermTextParser::reset()
{
    m_lines.clear();
    m_lines.push_back(QString());
    m_cursorColumn = 0;
    m_parserState = ParserState::Normal;
    m_csiBuffer.clear();
    rebuildVisibleCells();
}

void QTermTextParser::setTerminalSize(const QSize &size)
{
    const QSize normalizedSize(std::max(1, size.width()), std::max(1, size.height()));
    if (m_terminalSize == normalizedSize) {
        return;
    }

    m_terminalSize = normalizedSize;
    rebuildVisibleCells();
}

void QTermTextParser::processBytes(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    for (char byte : data) {
        appendByte(byte);
    }

    trimScrollback();
    rebuildVisibleCells();
}

void QTermTextParser::rebuildVisibleCells()
{
    if (!m_surfaceModel) {
        return;
    }

    const int columns = std::max(1, m_terminalSize.width());
    const int rows = std::max(1, m_terminalSize.height());
    QVector<QTermCell> cells(columns * rows);

    const int totalLines = std::max(1, static_cast<int>(m_lines.size()));
    const int scrollbackLineCount = std::max(0, totalLines - rows);
    m_surfaceModel->setScrollbackLineCount(scrollbackLineCount);

    const int topLine = std::clamp(m_surfaceModel->viewportTopLine(), 0, scrollbackLineCount);
    for (int row = 0; row < rows; ++row) {
        const int lineIndex = topLine + row;
        if (lineIndex < 0 || lineIndex >= m_lines.size()) {
            continue;
        }

        const QString &line = m_lines.at(lineIndex);
        const int visibleCharacterCount = std::min(columns, static_cast<int>(line.size()));
        for (int column = 0; column < visibleCharacterCount; ++column) {
            QTermCell &cell = cells[row * columns + column];
            cell.code = line.at(column).unicode();
        }
    }

    m_surfaceModel->setVisibleCells(std::move(cells));
    m_surfaceModel->setDirtyRegion(QRect(0, 0, columns, rows));
}

void QTermTextParser::appendByte(char byte)
{
    switch (m_parserState) {
    case ParserState::Normal:
        if (byte == '\x1b') {
            m_parserState = ParserState::Escape;
            return;
        }

        switch (byte) {
        case '\r':
            carriageReturn();
            return;
        case '\n':
            newLine();
            return;
        case '\b':
            backspace();
            return;
        case '\t': {
            const int nextTabStop = ((m_cursorColumn / 8) + 1) * 8;
            while (m_cursorColumn < nextTabStop) {
                writeCharacter(QChar::fromLatin1(' '));
            }
            return;
        }
        default:
            if (static_cast<unsigned char>(byte) >= 0x20) {
                writeCharacter(QChar::fromLatin1(byte));
            }
            return;
        }

    case ParserState::Escape:
        if (byte == '[') {
            m_parserState = ParserState::Csi;
            m_csiBuffer.clear();
            return;
        }

        if (byte == ']') {
            m_parserState = ParserState::Osc;
            return;
        }

        m_parserState = ParserState::Normal;
        return;

    case ParserState::Csi:
        if (byte >= '@' && byte <= '~') {
            handleCsiCommand(byte);
            m_csiBuffer.clear();
            m_parserState = ParserState::Normal;
            return;
        }

        m_csiBuffer.append(byte);
        return;

    case ParserState::Osc:
        if (byte == '\a') {
            m_parserState = ParserState::Normal;
            return;
        }

        if (byte == '\x1b') {
            m_parserState = ParserState::OscEscape;
        }
        return;

    case ParserState::OscEscape:
        m_parserState = (byte == '\\') ? ParserState::Normal : ParserState::Osc;
        return;
    }
}

void QTermTextParser::writeCharacter(QChar character)
{
    ensureCurrentLine();
    QString &line = m_lines.last();

    if (m_cursorColumn > line.size()) {
        line.append(QString(m_cursorColumn - line.size(), QChar::fromLatin1(' ')));
    }

    if (m_cursorColumn == line.size()) {
        line.append(character);
    } else {
        line[m_cursorColumn] = character;
    }

    ++m_cursorColumn;
    if (m_cursorColumn >= std::max(1, m_terminalSize.width())) {
        newLine();
    }
}

void QTermTextParser::newLine()
{
    m_lines.push_back(QString());
    m_cursorColumn = 0;
}

void QTermTextParser::carriageReturn()
{
    m_cursorColumn = 0;
}

void QTermTextParser::backspace()
{
    if (m_cursorColumn > 0) {
        --m_cursorColumn;
    }
}

void QTermTextParser::clearLineFromCursor()
{
    ensureCurrentLine();
    QString &line = m_lines.last();
    if (m_cursorColumn < line.size()) {
        line.truncate(m_cursorColumn);
    }
}

void QTermTextParser::ensureCurrentLine()
{
    if (m_lines.isEmpty()) {
        m_lines.push_back(QString());
    }
}

void QTermTextParser::trimScrollback()
{
    const int overflow = static_cast<int>(m_lines.size()) - m_maxScrollbackLines;
    if (overflow <= 0) {
        return;
    }

    m_lines.erase(m_lines.begin(), m_lines.begin() + overflow);
}

void QTermTextParser::handleCsiCommand(char command)
{
    switch (command) {
    case 'K':
        clearLineFromCursor();
        break;
    case 'C':
        m_cursorColumn += csiParameterOrDefault(1);
        break;
    case 'D':
        m_cursorColumn = std::max(0, m_cursorColumn - csiParameterOrDefault(1));
        break;
    case 'm':
    default:
        break;
    }
}

int QTermTextParser::csiParameterOrDefault(int defaultValue) const
{
    if (m_csiBuffer.isEmpty()) {
        return defaultValue;
    }

    bool ok = false;
    const QList<QByteArray> parts = m_csiBuffer.split(';');
    const int value = parts.constFirst().toInt(&ok);
    return ok ? value : defaultValue;
}