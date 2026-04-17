#pragma once

#include <QStringList>

#include <QTerm/qtermparser.h>

class QTermTextParser : public QTermParser
{
    Q_OBJECT

public:
    explicit QTermTextParser(QObject *parent = nullptr);
    ~QTermTextParser() override;

    void setSurfaceModel(QTermSurfaceModel *surfaceModel) override;
    void reset() override;
    void setTerminalSize(const QSize &size) override;
    void processBytes(const QByteArray &data) override;

private:
    enum class ParserState {
        Normal,
        Escape,
        Csi,
        Osc,
        OscEscape,
    };

    void rebuildVisibleCells();
    void appendByte(char byte);
    void writeCharacter(QChar character);
    void newLine();
    void carriageReturn();
    void backspace();
    void clearLineFromCursor();
    void ensureCurrentLine();
    void trimScrollback();
    void handleCsiCommand(char command);
    int csiParameterOrDefault(int defaultValue) const;

    QStringList m_lines;
    QSize m_terminalSize = QSize(80, 24);
    int m_cursorColumn = 0;
    int m_maxScrollbackLines = 5000;
    ParserState m_parserState = ParserState::Normal;
    QByteArray m_csiBuffer;
};