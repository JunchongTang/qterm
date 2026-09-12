#pragma once

#include <QLoggingCategory>
#include <QString>

namespace QTerm {

/*
    Why the cursor is or is not on screen.

    A cursor that stops being drawn while typing still works is one of four
    things: the view has no focus, a program hid it with DECTCEM, it scrolled
    out of the viewport, or its opacity is zero. From the outside all four look
    identical, and the reports that matter arrive after a day or two of use --
    too late to attach a debugger, too rare to reproduce on demand.

    Enable with:  QT_LOGGING_RULES="qterm.cursor.debug=true"
*/
Q_DECLARE_LOGGING_CATEGORY(lcQTermCursor)

/*
    Logs the reason only when it changes, so a long session produces a readable
    timeline rather than one line per frame. \a lastReason is owned by the view
    -- a plain QString, so the public headers do not have to expose this one.

    \a opacity is the host-driven blink level. The library owns no blink timer,
    so an animation stopped at zero leaves the cursor invisible while every
    other piece of state looks perfectly healthy.
*/
inline void qtermReportCursorDraw(QString &lastReason, bool hasFocus, bool modelVisible,
                                  qreal opacity, bool hasDelegate = false)
{
    if (!lcQTermCursor().isDebugEnabled())
        return;

    QString reason;
    if (hasDelegate)
        reason = QStringLiteral("drawn by the cursor delegate");
    else if (!hasFocus)
        reason = QStringLiteral("hidden: the view does not have focus");
    else if (!modelVisible)
        reason = QStringLiteral("hidden: DECTCEM off, or scrolled out of the viewport");
    else if (opacity <= 0.0)
        reason = QStringLiteral("hidden: cursorOpacity is 0 -- the host drives the blink, "
                                "so an animation stopped at zero looks like this");
    else
        reason = QStringLiteral("drawn");

    if (reason == lastReason)
        return;
    lastReason = reason;
    qCDebug(lcQTermCursor).noquote()
        << reason
        << QStringLiteral("(focus=%1 modelVisible=%2 opacity=%3)")
               .arg(hasFocus).arg(modelVisible).arg(opacity, 0, 'f', 2);
}

} // namespace QTerm
