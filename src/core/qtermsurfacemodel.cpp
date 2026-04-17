#include <QTerm/qtermsurfacemodel.h>

#include <algorithm>

QTermSurfaceModel::QTermSurfaceModel(QObject *parent)
    : QObject(parent)
{
}

QTermSurfaceModel::~QTermSurfaceModel() = default;

QSize QTermSurfaceModel::terminalSize() const
{
    return m_terminalSize;
}

QRect QTermSurfaceModel::dirtyRegion() const
{
    return m_dirtyRegion;
}

int QTermSurfaceModel::viewportTopLine() const
{
    return m_viewportTopLine;
}

int QTermSurfaceModel::scrollbackLineCount() const
{
    return m_scrollbackLineCount;
}

bool QTermSurfaceModel::followOutput() const
{
    return m_followOutput;
}

QPointF QTermSurfaceModel::contentOffset() const
{
    return m_contentOffset;
}

const QTermCell *QTermSurfaceModel::visibleCells() const
{
    return m_visibleCells.constData();
}

qsizetype QTermSurfaceModel::visibleCellCount() const
{
    return m_visibleCells.size();
}

void QTermSurfaceModel::setTerminalSize(const QSize &size)
{
    if (m_terminalSize == size) {
        return;
    }

    m_terminalSize = size;
    if (m_followOutput) {
        const int bottomLine = maximumViewportTopLine();
        if (m_viewportTopLine != bottomLine) {
            m_viewportTopLine = bottomLine;
            emit viewportTopLineChanged();
        }
    } else {
        const int clampedLine = std::clamp(m_viewportTopLine, 0, maximumViewportTopLine());
        if (clampedLine != m_viewportTopLine) {
            m_viewportTopLine = clampedLine;
            emit viewportTopLineChanged();
        }
    }

    emit terminalSizeChanged();
}

void QTermSurfaceModel::setDirtyRegion(const QRect &region)
{
    if (m_dirtyRegion == region) {
        return;
    }

    m_dirtyRegion = region;
    emit contentChanged();
}

void QTermSurfaceModel::setViewportTopLine(int line)
{
    const int clampedLine = std::clamp(line, 0, maximumViewportTopLine());
    if (m_viewportTopLine == clampedLine) {
        return;
    }

    m_viewportTopLine = clampedLine;
    const bool shouldFollowOutput = (m_viewportTopLine == maximumViewportTopLine());
    if (m_followOutput != shouldFollowOutput) {
        m_followOutput = shouldFollowOutput;
        emit followOutputChanged();
    }

    emit viewportTopLineChanged();
    emit contentChanged();
}

void QTermSurfaceModel::setScrollbackLineCount(int lineCount)
{
    const int normalizedLineCount = std::max(0, lineCount);
    if (m_scrollbackLineCount == normalizedLineCount) {
        return;
    }

    m_scrollbackLineCount = normalizedLineCount;
    emit scrollbackLineCountChanged();

    if (m_followOutput) {
        const int bottomLine = maximumViewportTopLine();
        if (m_viewportTopLine != bottomLine) {
            m_viewportTopLine = bottomLine;
            emit viewportTopLineChanged();
        }
    } else {
        const int clampedLine = std::clamp(m_viewportTopLine, 0, maximumViewportTopLine());
        if (clampedLine != m_viewportTopLine) {
            m_viewportTopLine = clampedLine;
            emit viewportTopLineChanged();
        }
    }

    emit contentChanged();
}

void QTermSurfaceModel::setFollowOutput(bool followOutput)
{
    if (m_followOutput == followOutput) {
        return;
    }

    m_followOutput = followOutput;
    if (m_followOutput) {
        const int bottomLine = maximumViewportTopLine();
        if (m_viewportTopLine != bottomLine) {
            m_viewportTopLine = bottomLine;
            emit viewportTopLineChanged();
        }
    }

    emit followOutputChanged();
    emit contentChanged();
}

void QTermSurfaceModel::setContentOffset(const QPointF &offset)
{
    if (m_contentOffset == offset) {
        return;
    }

    m_contentOffset = offset;
    emit contentOffsetChanged();
    emit contentChanged();
}

void QTermSurfaceModel::setVisibleCells(QVector<QTermCell> cells)
{
    m_visibleCells = std::move(cells);
    emit contentChanged();
}

void QTermSurfaceModel::clearDirtyRegion()
{
    if (m_dirtyRegion.isNull()) {
        return;
    }

    m_dirtyRegion = QRect();
    emit contentChanged();
}

void QTermSurfaceModel::scrollByLines(int deltaLines)
{
    if (deltaLines == 0) {
        return;
    }

    setViewportTopLine(m_viewportTopLine + deltaLines);
}

void QTermSurfaceModel::scrollToBottom()
{
    setFollowOutput(true);
    setContentOffset(QPointF());
}

int QTermSurfaceModel::maximumViewportTopLine() const
{
    return std::max(0, m_scrollbackLineCount);
}