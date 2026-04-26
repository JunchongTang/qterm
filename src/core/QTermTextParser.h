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
        Csi,
        Osc,
        OscEscape,
    };

    static int parameterAt(const QVector<int> &parameters, int index, int defaultValue);
    static QVector<int> parseCsiParameters(const QString &text);
    void handleGroundTextUnit(const QString &text, QTermInputExecutor &executor);
    void handleCsiFinal(bool privateMode, bool secondaryMode, QChar final, QTermInputExecutor &executor);
    void handleEscapeFinal(QChar final, QTermInputExecutor &executor);
    void handleOscTerminator(QTermInputExecutor &executor);

    State m_state = State::Ground;
    QString m_csiParameters;
    QString m_oscData;
    QChar m_pendingHighSurrogate;
};

} // namespace QTerm

#endif // QTERM_QTERMTEXTPARSER_H