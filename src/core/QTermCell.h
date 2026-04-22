#ifndef QTERM_QTERMCELL_H
#define QTERM_QTERMCELL_H

#include <QString>

namespace QTerm {

struct QTermCellAttributes
{
    bool bold = false;
    bool italic = false;
    bool underline = false;
    int foregroundIndex = -1;
    int backgroundIndex = -1;
};

struct QTermCell
{
    QString text;
    int width = 1;
    bool continuation = false;
    QTermCellAttributes attributes;
};

} // namespace QTerm

#endif // QTERM_QTERMCELL_H