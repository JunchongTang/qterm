#pragma once

#include <QColor>
#include <QObject>
#include <QString>

// Design tokens shared with the Qt Quick demo (shadcn/ui base-mira, blue
// primary). Widgets get their look from the generated style sheet rather than
// from per-widget painting, so switching mode is a single re-apply.
class Theme : public QObject
{
    Q_OBJECT

public:
    static Theme *instance();

    bool isDark() const noexcept { return m_dark; }
    void setDark(bool dark);

    // Color tokens
    QColor background() const { return m_dark ? QColor("#18181b") : QColor("#ffffff"); }
    QColor foreground() const { return m_dark ? QColor("#fafafa") : QColor("#0a0a0a"); }
    QColor popover() const { return m_dark ? QColor("#1f1f23") : QColor("#ffffff"); }
    QColor primary() const { return m_dark ? QColor("#3b82f6") : QColor("#2563eb"); }
    QColor primaryForeground() const { return QColor("#ffffff"); }
    QColor muted() const { return m_dark ? QColor("#2e2e33") : QColor("#f5f5f5"); }
    QColor mutedForeground() const { return m_dark ? QColor("#a1a1aa") : QColor("#737373"); }
    QColor border() const { return m_dark ? QColor(255, 255, 255, 26) : QColor("#e5e5e5"); }
    QColor input() const { return m_dark ? QColor(255, 255, 255, 38) : QColor("#e5e5e5"); }
    QColor ring() const { return m_dark ? QColor("#737373") : QColor("#a1a1a1"); }
    QColor destructive() const { return m_dark ? QColor("#ff6467") : QColor("#e7000b"); }

    // Radii and spacing, mirroring the QML Theme singleton.
    int radiusSm() const { return 6; }
    int radiusMd() const { return 8; }
    int radiusLg() const { return 10; }
    int radiusXl() const { return 14; }

    int space1() const { return 4; }
    int space2() const { return 8; }
    int space3() const { return 12; }
    int space4() const { return 16; }

    int textXs() const { return 12; }
    int textSm() const { return 14; }

    // The application-wide style sheet for the current mode.
    QString styleSheet() const;

signals:
    void darkChanged();

private:
    explicit Theme(QObject *parent = nullptr) : QObject(parent) {}

    bool m_dark = true;
};
