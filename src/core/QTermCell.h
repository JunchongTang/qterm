#ifndef QTERM_QTERMCELL_H
#define QTERM_QTERMCELL_H

#include <QString>

namespace QTerm {

struct QTermCellAttributes
{
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool inverse = false;
    int foregroundIndex = -1;
    int backgroundIndex = -1;
    int foregroundRgb = -1;
    int backgroundRgb = -1;
    // OSC 8 hyperlink: 0 means no link; positive int is an index into the
    // core's hyperlink URL table.
    int hyperlinkId = 0;
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