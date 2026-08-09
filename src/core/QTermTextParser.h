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
    // Parallel to the values from the last parseCsiParameters(): 1 where the
    // value was introduced by a colon rather than a semicolon, i.e. where it is
    // a sub-parameter of the value before it. SGR needs the distinction because
    // ESC[38:2::r:g:bm carries an empty colour-space slot that the semicolon
    // form does not have, and folding colons into semicolons shifts the
    // components by one.
    const QVector<quint8> &csiSubParameterFlags() const { return m_csiParameterIsSub; }
    void handleGroundTextUnit(QStringView text, QTermInputExecutor &executor);
    void handleCsiFinal(bool privateMode, bool secondaryMode, QChar final, QTermInputExecutor &executor);
    void handleCsiIntermediateFinal(QChar intermediate, QChar final, QTermInputExecutor &executor);
    void handleEscapeFinal(QChar final, QTermInputExecutor &executor);
    void handleOscTerminator(QTermInputExecutor &executor);

    State m_state = State::Ground;
    QString m_csiParameters;
    QVector<int> m_csiParameterValues;
    QVector<quint8> m_csiParameterIsSub;
    QChar m_csiIntermediate;
    QString m_oscData;
    QChar m_pendingHighSurrogate;
    QChar m_escIntermediate;
};

} // namespace QTerm

#endif // QTERM_QTERMTEXTPARSER_H