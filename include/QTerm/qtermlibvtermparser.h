#pragma once

#include <QVector>

#include <vterm.h>

#include <QTerm/qtermcell.h>
#include <QTerm/qtermparser.h>

class QTermLibvtermParser : public QTermParser
{
    Q_OBJECT

public:
    explicit QTermLibvtermParser(QObject *parent = nullptr);
    ~QTermLibvtermParser() override;

    void setSurfaceModel(QTermSurfaceModel *surfaceModel) override;
    void reset() override;
    void setTerminalSize(const QSize &size) override;
    void processBytes(const QByteArray &data) override;

private:
    static int handleScreenDamage(VTermRect rect, void *user);
    static int handleScreenResize(int rows, int cols, void *user);
    static int handleScrollbackPushline(int cols, const VTermScreenCell *cells, void *user);
    static int handleScrollbackClear(void *user);

    void ensureVTerm();
    void destroyVTerm();
    void rebuildVisibleCells();
    void markDirty();
    int pushScrollbackLine(int cols, const VTermScreenCell *cells);
    int clearScrollback();

    VTerm *m_vterm = nullptr;
    VTermScreen *m_screen = nullptr;
    QSize m_terminalSize = QSize(80, 24);
    QVector<QVector<QTermCell>> m_scrollbackLines;
    int m_maxScrollbackLines = 5000;
    bool m_dirty = true;
};