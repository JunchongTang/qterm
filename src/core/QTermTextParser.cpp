#include "QTermTextParser.h"

#include "QTermInputExecutor.h"

namespace QTerm {

void QTermTextParser::parse(const QString &text, QTermInputExecutor &executor)
{
    for (int index = 0; index < text.size(); ++index) {
        QChar character = text.at(index);

        if (!m_pendingHighSurrogate.isNull() && m_state == State::Ground) {
            if (character.isLowSurrogate()) {
                handleGroundTextUnit(QString(m_pendingHighSurrogate) + character, executor);
                m_pendingHighSurrogate = QChar();
                continue;
            }

            handleGroundTextUnit(QString(m_pendingHighSurrogate), executor);
            m_pendingHighSurrogate = QChar();
        }

        switch (m_state) {
        case State::Ground:
            if (character == u'\x1b') {
                m_state = State::Escape;
            } else if (character.isHighSurrogate()) {
                m_pendingHighSurrogate = character;
            } else {
                handleGroundTextUnit(QString(character), executor);
            }
            break;
        case State::Escape:
            if (character == u'[') {
                m_state = State::Csi;
                m_csiParameters.clear();
            } else if (character == u']') {
                m_state = State::Osc;
                m_oscData.clear();
            } else if (character == u'\x1b') {
                m_state = State::Escape;
            } else if (character == u'(' || character == u')' || character == u'%') {
                // Intermediate character for designate character set sequences
                // e.g. ESC ( 0, ESC ( B, ESC ) 0
                m_escIntermediate = character;
                m_state = State::EscapeIntermediate;
            } else {
                handleEscapeFinal(character, executor);
                m_state = State::Ground;
            }
            break;
        case State::EscapeIntermediate:
            executor.designateCharacterSet(m_escIntermediate, character);
            m_escIntermediate = QChar();
            m_state = State::Ground;
            break;
        case State::Csi:
            if (character.isDigit() || character == u';' || character == u'?' || character == u'>') {
                m_csiParameters.append(character);
            } else {
                handleCsiFinal(m_csiParameters.startsWith(u'?'),
                               m_csiParameters.startsWith(u'>'),
                               character, executor);
                m_csiParameters.clear();
                m_state = State::Ground;
            }
            break;
        case State::Osc:
            if (character == u'\x07') {
                handleOscTerminator(executor);
            } else if (character == u'\x1b') {
                m_state = State::OscEscape;
            } else {
                m_oscData.append(character);
            }
            break;
        case State::OscEscape:
            if (character == u'\\') {
                handleOscTerminator(executor);
            } else {
                m_oscData.append(u'\x1b');
                m_oscData.append(character);
                m_state = State::Osc;
            }
            break;
        }
    }
}

int QTermTextParser::parameterAt(const QVector<int> &parameters, int index, int defaultValue)
{
    if (index >= parameters.size() || parameters.at(index) <= 0) {
        return defaultValue;
    }

    return parameters.at(index);
}

QVector<int> QTermTextParser::parseCsiParameters(const QString &text)
{
    QString parametersText = text;
    if (parametersText.startsWith(u'?') || parametersText.startsWith(u'>')) {
        parametersText.remove(0, 1);
    }

    if (parametersText.isEmpty()) {
        return {};
    }

    const QStringList parts = parametersText.split(u';');
    QVector<int> parameters;
    parameters.reserve(parts.size());
    for (const QString &part : parts) {
        if (part.isEmpty()) {
            parameters.append(0);
        } else {
            parameters.append(part.toInt());
        }
    }

    return parameters;
}

void QTermTextParser::handleGroundTextUnit(const QString &text, QTermInputExecutor &executor)
{
    if (text.size() == 1) {
        switch (text.front().unicode()) {
        case '\0': // NUL — ignored
            break;
        case '\a':
            executor.bell();
            break;
        case '\n':
            executor.lineFeed();
            break;
        case '\r':
            executor.carriageReturn();
            break;
        case '\b':
            executor.backspace();
            break;
        case '\t':
            executor.horizontalTab();
            break;
        case '\x0e': // SO — Locking-Shift G1 (not yet fully implemented, ignore to prevent garbage)
        case '\x0f': // SI — Locking-Shift G0 (not yet fully implemented, ignore to prevent garbage)
        case '\x7f': // DEL — ignored
            break;
        default:
            executor.print(text);
            break;
        }
        return;
    }

    executor.print(text);
}

void QTermTextParser::handleCsiFinal(bool privateMode, bool secondaryMode, QChar final, QTermInputExecutor &executor)
{
    const QVector<int> parameters = parseCsiParameters(m_csiParameters);

    if (privateMode) {
        switch (final.unicode()) {
        case 'h':
            executor.setPrivateModes(parameters, true);
            break;
        case 'l':
            executor.setPrivateModes(parameters, false);
            break;
        default:
            break;
        }
        return;
    }

    if (secondaryMode) {
        switch (final.unicode()) {
        case 'c':
            executor.secondaryDeviceAttributes();
            break;
        default:
            break;
        }
        return;
    }

    switch (final.unicode()) {
    case 'A':
        executor.cursorUp(parameterAt(parameters, 0, 1));
        break;
    case 'B':
        executor.cursorDown(parameterAt(parameters, 0, 1));
        break;
    case 'C':
        executor.cursorForward(parameterAt(parameters, 0, 1));
        break;
    case 'D':
        executor.cursorBackward(parameterAt(parameters, 0, 1));
        break;
    case 'E':
        executor.cursorNextLine(parameterAt(parameters, 0, 1));
        break;
    case 'F':
        executor.cursorPreviousLine(parameterAt(parameters, 0, 1));
        break;
    case 'G':
        executor.cursorHorizontalAbsolute(parameterAt(parameters, 0, 1) - 1);
        break;
    case 'H':
    case 'f':
        executor.cursorPosition(parameterAt(parameters, 0, 1) - 1, parameterAt(parameters, 1, 1) - 1);
        break;
    case 'J':
        executor.eraseInDisplay(parameterAt(parameters, 0, 0));
        break;
    case 'K':
        executor.eraseInLine(parameterAt(parameters, 0, 0));
        break;
    case '@':
        executor.insertCharacters(parameterAt(parameters, 0, 1));
        break;
    case 'L':
        executor.insertLines(parameterAt(parameters, 0, 1));
        break;
    case 'M':
        executor.deleteLines(parameterAt(parameters, 0, 1));
        break;
    case 'P':
        executor.deleteCharacters(parameterAt(parameters, 0, 1));
        break;
    case 'S':
        executor.scrollUpRegion(parameterAt(parameters, 0, 1));
        break;
    case 'T':
        executor.scrollDownRegion(parameterAt(parameters, 0, 1));
        break;
    case 'X':
        executor.eraseCharacters(parameterAt(parameters, 0, 1));
        break;
    case 'c':
        executor.deviceAttributes();
        break;
    case 'd':
        executor.linePositionAbsolute(parameterAt(parameters, 0, 1) - 1);
        break;
    case 'm':
        executor.characterAttributes(parameters);
        break;
    case 'n':
        if (parameterAt(parameters, 0, 0) == 6) {
            executor.deviceStatusReport();
        }
        break;
    case 'r':
        executor.setScrollRegion(parameterAt(parameters, 0, 1) - 1,
                                 parameterAt(parameters, 1, executor.rows()) - 1);
        break;
    case 's':
        executor.saveCursor();
        break;
    case 'u':
        executor.restoreCursor();
        break;
    default:
        break;
    }
}

void QTermTextParser::handleEscapeFinal(QChar final, QTermInputExecutor &executor)
{
    switch (final.unicode()) {
    case '7':
        executor.saveCursor();
        break;
    case '8':
        executor.restoreCursor();
        break;
    case 'M':
        executor.reverseIndex();
        break;
    case '=':
        executor.setKeypadMode(true);
        break;
    case '>':
        executor.setKeypadMode(false);
        break;
    case 'c':
        // RIS — Reset to Initial State: full terminal hard reset.
        executor.reset();
        break;
    default:
        break;
    }
}

void QTermTextParser::handleOscTerminator(QTermInputExecutor &executor)
{
    const qsizetype separatorIndex = m_oscData.indexOf(u';');
    if (separatorIndex > 0) {
        const QString command = m_oscData.left(separatorIndex);
        if (command == u"0" || command == u"2") {
            executor.setWindowTitle(m_oscData.sliced(separatorIndex + 1));
        } else if (command == u"8") {
            // OSC 8 ; params ; uri BEL/ST
            // The content after the command separator is "params;uri"
            const QString rest = m_oscData.sliced(separatorIndex + 1);
            const qsizetype uriSep = rest.indexOf(u';');
            const QString uri = (uriSep >= 0) ? rest.sliced(uriSep + 1) : QString();
            executor.setHyperlink(uri);
        }
    }

    m_oscData.clear();
    m_state = State::Ground;
}

} // namespace QTerm