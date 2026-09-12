#include "QTermCursorDiagnostics.h"

namespace QTerm {

// Off unless asked for: the point is to answer a field report, not to cost
// anything in normal use.
Q_LOGGING_CATEGORY(lcQTermCursor, "qterm.cursor", QtWarningMsg)

} // namespace QTerm
