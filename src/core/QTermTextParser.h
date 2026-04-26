#ifndef QTERM_QTERMTEXTPARSER_H
#define QTERM_QTERMTEXTPARSER_H

#include <QString>
#include <QVector>

namespace QTerm {

class QTermInputExecutor;

class QTermTextParser
{
public:
    void parse(const QString &text, QTermInputExecutor &executor);

private:
    enum class State {
        Ground,
        Escape,
        EscapeIntermediate,
        Csi,
        CsiIntermediate, // after an intermediate byte (0x20–0x2F) inside CSI
        Osc,
        OscEscape,
    };

    static int parameterAt(const QVector<int> &parameters, int index, int defaultValue);
    static QVector<int> parseCsiParameters(const QString &text);
    void handleGroundTextUnit(const QString &text, QTermInputExecutor &executor);
    void handleCsiFinal(bool privateMode, bool secondaryMode, QChar final, QTermInputExecutor &executor);
    void handleCsiIntermediateFinal(QChar intermediate, QChar final, QTermInputExecutor &executor);
    void handleEscapeFinal(QChar final, QTermInputExecutor &executor);
    void handleOscTerminator(QTermInputExecutor &executor);

    State m_state = State::Ground;
    QString m_csiParameters;
    QChar m_csiIntermediate;
    QString m_oscData;
    QChar m_pendingHighSurrogate;
    QChar m_escIntermediate;
};

} // namespace QTerm

#endif // QTERM_QTERMTEXTPARSER_H