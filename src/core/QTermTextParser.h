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
    };

    static int parameterAt(const QVector<int> &parameters, int index, int defaultValue);
    static QVector<int> parseCsiParameters(const QString &text);
    void handleGroundCharacter(QChar character, QTermInputExecutor &executor);
    void handleCsiFinal(QChar final, QTermInputExecutor &executor);
    void handleEscapeFinal(QChar final, QTermInputExecutor &executor);

    State m_state = State::Ground;
    QString m_csiParameters;
};

} // namespace QTerm

#endif // QTERM_QTERMTEXTPARSER_H