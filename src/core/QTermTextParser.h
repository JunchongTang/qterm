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
        CsiIgnore,
        Osc,
        OscEscape,
        IgnoreString,
        IgnoreStringEscape,
        IgnoreOsc,
        IgnoreOscEscape,
        EscapeIgnore,
    };

    static int parameterAt(const QVector<int> &parameters, int index, int defaultValue);
    // Returns a reference into a reused buffer, valid until the next call.
    // A coloured stream carries millions of CSI sequences, so returning by
    // value meant an allocation per sequence.
    const QVector<int> &parseCsiParameters(const QString &text);
    void handleGroundTextUnit(QStringView text, QTermInputExecutor &executor);
    void handleCsiFinal(bool privateMode, bool secondaryMode, QChar final, QTermInputExecutor &executor);
    void handleCsiIntermediateFinal(QChar intermediate, QChar final, QTermInputExecutor &executor);
    void handleEscapeFinal(QChar final, QTermInputExecutor &executor);
    void handleOscTerminator(QTermInputExecutor &executor);

    State m_state = State::Ground;
    QString m_csiParameters;
    QVector<int> m_csiParameterValues;
    QChar m_csiIntermediate;
    QString m_oscData;
    QChar m_pendingHighSurrogate;
    QChar m_escIntermediate;
};

} // namespace QTerm

#endif // QTERM_QTERMTEXTPARSER_H