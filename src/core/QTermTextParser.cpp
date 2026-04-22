#include "QTermTextParser.h"

#include "QTermInputExecutor.h"

namespace QTerm {

void QTermTextParser::parse(const QString &text, QTermInputExecutor &executor)
{
    for (const QChar character : text) {
        switch (m_state) {
        case State::Ground:
            if (character == u'\x1b') {
                m_state = State::Escape;
            } else {
                handleGroundCharacter(character, executor);
            }
            break;
        case State::Escape:
            if (character == u'[') {
                m_state = State::Csi;
                m_csiParameters.clear();
            } else if (character == u'\x1b') {
                m_state = State::Escape;
            } else {
                handleEscapeFinal(character, executor);
                m_state = State::Ground;
            }
            break;
        case State::Csi:
            if (character.isDigit() || character == u';' || character == u'?') {
                m_csiParameters.append(character);
            } else {
                handleCsiFinal(character, executor);
                m_csiParameters.clear();
                m_state = State::Ground;
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
    if (parametersText.startsWith(u'?')) {
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

void QTermTextParser::handleGroundCharacter(QChar character, QTermInputExecutor &executor)
{
    switch (character.unicode()) {
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
    default:
        executor.print(QString(character));
        break;
    }
}

void QTermTextParser::handleCsiFinal(QChar final, QTermInputExecutor &executor)
{
    const QVector<int> parameters = parseCsiParameters(m_csiParameters);

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
    case 'm':
        executor.characterAttributes(parameters);
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
    default:
        handleGroundCharacter(final, executor);
        break;
    }
}

} // namespace QTerm