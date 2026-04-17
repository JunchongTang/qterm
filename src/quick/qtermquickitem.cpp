#include <QTerm/qtermquickitem.h>

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QWheelEvent>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include <QTerm/qtermsession.h>
#include <QTerm/qtermsurfacemodel.h>

QTermQuickItem::QTermQuickItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_backgroundColor(0x10, 0x10, 0x10)
    , m_cellSize(10.0, 20.0)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
}

QTermQuickItem::~QTermQuickItem()
{
    releaseTexture();
}

QColor QTermQuickItem::backgroundColor() const
{
    return m_backgroundColor;
}

void QTermQuickItem::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    update();
    emit backgroundColorChanged();
}

QSizeF QTermQuickItem::cellSize() const
{
    return m_cellSize;
}

void QTermQuickItem::setCellSize(const QSizeF &size)
{
    if (m_cellSize == size || size.width() <= 0.0 || size.height() <= 0.0) {
        return;
    }

    m_cellSize = size;
    synchronizeTerminalSize();
    update();
    emit cellSizeChanged();
}

QTermSurfaceModel *QTermQuickItem::model() const
{
    return m_model;
}

void QTermQuickItem::setModel(QTermSurfaceModel *model)
{
    if (m_model == model) {
        return;
    }

    if (m_model) {
        m_model->disconnect(this);
    }

    m_model = model;
    connectModelSignals();
    synchronizeTerminalSize();
    update();
    emit modelChanged();
}

QTermSession *QTermQuickItem::session() const
{
    return m_session;
}

void QTermQuickItem::setSession(QTermSession *session)
{
    if (m_session == session) {
        return;
    }

    m_session = session;
    synchronizeTerminalSize();
    emit sessionChanged();
}

QSGNode *QTermQuickItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
    }

    const QRectF rect = boundingRect();
    const qreal devicePixelRatio = std::max(1.0, window() ? window()->devicePixelRatio() : 1.0);
    const QSize textureSize(std::max(1, static_cast<int>(std::ceil(rect.width() * devicePixelRatio))),
                            std::max(1, static_cast<int>(std::ceil(rect.height() * devicePixelRatio))));
    QImage image(textureSize, QImage::Format_RGBA8888_Premultiplied);
    image.fill(m_backgroundColor);

    if (m_model && m_model->terminalSize().width() > 0 && m_model->terminalSize().height() > 0) {
        QPainter painter(&image);
        painter.scale(devicePixelRatio, devicePixelRatio);
        painter.setRenderHint(QPainter::TextAntialiasing, false);
        painter.setPen(QColor(0xd0, 0xd0, 0xd0));

        QFont font(QStringLiteral("Menlo"));
        font.setStyleHint(QFont::Monospace);
        font.setPixelSize(std::max(12, static_cast<int>(m_cellSize.height() * 0.75)));
        painter.setFont(font);

        const int columns = std::max(1, m_model->terminalSize().width());
        const int rows = std::max(1, m_model->terminalSize().height());
        const int cellCount = std::min(static_cast<int>(m_model->visibleCellCount()), columns * rows);
        const QTermCell *cells = m_model->visibleCells();
        const qreal baseline = m_cellSize.height() * 0.8 + m_model->contentOffset().y();

        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                const int index = row * columns + column;
                if (index >= cellCount || cells[index].code == 0) {
                    continue;
                }

                const QRectF cellRect(column * m_cellSize.width(),
                                      row * m_cellSize.height() + m_model->contentOffset().y(),
                                      m_cellSize.width(),
                                      m_cellSize.height());
                painter.fillRect(cellRect, QColor::fromRgba(cells[index].background));
                painter.setPen(QColor::fromRgba(cells[index].foreground));
                painter.drawText(QPointF(cellRect.x(), row * m_cellSize.height() + baseline),
                                 QString(QChar(static_cast<char16_t>(cells[index].code))));
            }
        }
    }

    if (m_texture) {
        delete m_texture;
        m_texture = nullptr;
    }

    if (window()) {
        m_texture = window()->createTextureFromImage(image);
    }

    node->setRect(rect);
    node->setFiltering(QSGTexture::Nearest);
    node->setOwnsTexture(false);
    node->setTexture(m_texture);

    return node;
}

void QTermQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() == oldGeometry.size()) {
        return;
    }

    synchronizeTerminalSize();
}

void QTermQuickItem::wheelEvent(QWheelEvent *event)
{
    if (!m_model || m_cellSize.height() <= 0.0) {
        QQuickItem::wheelEvent(event);
        return;
    }

    qreal pixelDeltaY = 0.0;
    if (!event->pixelDelta().isNull()) {
        pixelDeltaY = -event->pixelDelta().y();
    } else if (!event->angleDelta().isNull()) {
        pixelDeltaY = -(event->angleDelta().y() / 120.0) * m_cellSize.height();
    }

    if (pixelDeltaY == 0.0) {
        QQuickItem::wheelEvent(event);
        return;
    }

    m_accumulatedScrollPixels += pixelDeltaY;

    const int wholeLines = static_cast<int>(m_accumulatedScrollPixels / m_cellSize.height());
    if (wholeLines != 0) {
        m_model->scrollByLines(wholeLines);
        m_accumulatedScrollPixels -= wholeLines * m_cellSize.height();
    }

    m_model->setContentOffset(QPointF(0.0, m_accumulatedScrollPixels));
    event->accept();
}

void QTermQuickItem::connectModelSignals()
{
    if (!m_model) {
        return;
    }

    connect(m_model, &QTermSurfaceModel::contentChanged,
            this, qOverload<>(&QQuickItem::update));
    connect(m_model, &QTermSurfaceModel::cursorChanged,
            this, qOverload<>(&QQuickItem::update));
    connect(m_model, &QTermSurfaceModel::selectionChanged,
            this, qOverload<>(&QQuickItem::update));
    connect(m_model, &QTermSurfaceModel::viewportTopLineChanged,
            this, qOverload<>(&QQuickItem::update));
    connect(m_model, &QTermSurfaceModel::contentOffsetChanged,
            this, qOverload<>(&QQuickItem::update));
    connect(m_model, &QTermSurfaceModel::followOutputChanged,
            this, qOverload<>(&QQuickItem::update));
}

void QTermQuickItem::synchronizeTerminalSize()
{
    if (m_cellSize.width() <= 0.0 || m_cellSize.height() <= 0.0) {
        return;
    }

    const int columns = std::max(1, static_cast<int>(width() / m_cellSize.width()));
    const int rows = std::max(1, static_cast<int>(height() / m_cellSize.height()));
    const QSize newTerminalSize(columns, rows);

    if (m_model) {
        const QSize previousSize = m_model->terminalSize();
        m_model->setTerminalSize(newTerminalSize);
        if (previousSize != newTerminalSize) {
            emit terminalSizeChanged();
        }
    }

    if (m_session) {
        m_session->resize(columns, rows);
    }
}

void QTermQuickItem::releaseTexture()
{
    delete m_texture;
    m_texture = nullptr;
}
