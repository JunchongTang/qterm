#pragma once

#include <QPointF>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QObject>

#include <QTerm/qtermcell.h>

class QTermSurfaceModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QSize terminalSize READ terminalSize NOTIFY terminalSizeChanged)
    Q_PROPERTY(QRect dirtyRegion READ dirtyRegion NOTIFY contentChanged)
    Q_PROPERTY(int viewportTopLine READ viewportTopLine WRITE setViewportTopLine NOTIFY viewportTopLineChanged)
    Q_PROPERTY(int scrollbackLineCount READ scrollbackLineCount WRITE setScrollbackLineCount NOTIFY scrollbackLineCountChanged)
    Q_PROPERTY(bool followOutput READ followOutput WRITE setFollowOutput NOTIFY followOutputChanged)
    Q_PROPERTY(QPointF contentOffset READ contentOffset WRITE setContentOffset NOTIFY contentOffsetChanged)

public:
    explicit QTermSurfaceModel(QObject *parent = nullptr);
    ~QTermSurfaceModel() override;

    QSize terminalSize() const;
    QRect dirtyRegion() const;
    int viewportTopLine() const;
    int scrollbackLineCount() const;
    bool followOutput() const;
    QPointF contentOffset() const;
    const QTermCell *visibleCells() const;
    qsizetype visibleCellCount() const;

    void setTerminalSize(const QSize &size);
    void setDirtyRegion(const QRect &region);
    void setViewportTopLine(int line);
    void setScrollbackLineCount(int lineCount);
    void setFollowOutput(bool followOutput);
    void setContentOffset(const QPointF &offset);
    void setVisibleCells(QVector<QTermCell> cells);
    void clearDirtyRegion();

    Q_INVOKABLE void scrollByLines(int deltaLines);
    Q_INVOKABLE void scrollToBottom();

signals:
    void terminalSizeChanged();
    void contentChanged();
    void cursorChanged();
    void selectionChanged();
    void viewportTopLineChanged();
    void scrollbackLineCountChanged();
    void followOutputChanged();
    void contentOffsetChanged();

private:
    int maximumViewportTopLine() const;

    QSize m_terminalSize;
    QRect m_dirtyRegion;
    QVector<QTermCell> m_visibleCells;
    int m_viewportTopLine = 0;
    int m_scrollbackLineCount = 0;
    bool m_followOutput = true;
    QPointF m_contentOffset;
};