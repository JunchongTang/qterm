#include <QTerm/qtermlibvtermparser.h>

#include <algorithm>

#include <QTerm/qtermsurfacemodel.h>

namespace {

quint32 toArgb(VTermScreen *screen, VTermColor color)
{
    vterm_screen_convert_color_to_rgb(screen, &color);
    return 0xff000000u |
           (static_cast<quint32>(color.rgb.red) << 16) |
           (static_cast<quint32>(color.rgb.green) << 8) |
           static_cast<quint32>(color.rgb.blue);
}

QTermCell toCell(VTermScreen *screen, const VTermScreenCell &source)
{
    QTermCell cell;
    cell.code = source.chars[0];
    cell.foreground = toArgb(screen, source.fg);
    cell.background = toArgb(screen, source.bg);
    cell.width = static_cast<quint8>(std::max(1, static_cast<int>(source.width)));

    cell.attributes = 0;
    if (source.attrs.bold) {
        cell.attributes |= 1 << 0;
    }
    if (source.attrs.underline) {
        cell.attributes |= 1 << 1;
    }
    if (source.attrs.italic) {
        cell.attributes |= 1 << 2;
    }
    if (source.attrs.reverse) {
        cell.attributes |= 1 << 3;
    }

    return cell;
}

}

QTermLibvtermParser::QTermLibvtermParser(QObject *parent)
    : QTermParser(parent)
{
    ensureVTerm();
}

QTermLibvtermParser::~QTermLibvtermParser()
{
    destroyVTerm();
}

void QTermLibvtermParser::setSurfaceModel(QTermSurfaceModel *surfaceModel)
{
    QTermParser::setSurfaceModel(surfaceModel);
    rebuildVisibleCells();
}

void QTermLibvtermParser::reset()
{
    destroyVTerm();
    m_scrollbackLines.clear();
    ensureVTerm();
    markDirty();
    rebuildVisibleCells();
}

void QTermLibvtermParser::setTerminalSize(const QSize &size)
{
    const QSize normalizedSize(std::max(1, size.width()), std::max(1, size.height()));
    if (m_terminalSize == normalizedSize && m_vterm) {
        return;
    }

    m_terminalSize = normalizedSize;
    ensureVTerm();
    vterm_set_size(m_vterm, m_terminalSize.height(), m_terminalSize.width());
    markDirty();
    rebuildVisibleCells();
}

void QTermLibvtermParser::processBytes(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    ensureVTerm();
    vterm_input_write(m_vterm, data.constData(), static_cast<size_t>(data.size()));
    vterm_screen_flush_damage(m_screen);
    rebuildVisibleCells();
}

int QTermLibvtermParser::handleScreenDamage(VTermRect, void *user)
{
    auto *parser = static_cast<QTermLibvtermParser *>(user);
    parser->markDirty();
    return 1;
}

int QTermLibvtermParser::handleScreenResize(int rows, int cols, void *user)
{
    auto *parser = static_cast<QTermLibvtermParser *>(user);
    parser->m_terminalSize = QSize(cols, rows);
    parser->markDirty();
    return 1;
}

int QTermLibvtermParser::handleScrollbackPushline(int cols, const VTermScreenCell *cells, void *user)
{
    auto *parser = static_cast<QTermLibvtermParser *>(user);
    return parser->pushScrollbackLine(cols, cells);
}

int QTermLibvtermParser::handleScrollbackClear(void *user)
{
    auto *parser = static_cast<QTermLibvtermParser *>(user);
    return parser->clearScrollback();
}

void QTermLibvtermParser::ensureVTerm()
{
    if (m_vterm) {
        return;
    }

    m_vterm = vterm_new(m_terminalSize.height(), m_terminalSize.width());
    vterm_set_utf8(m_vterm, 1);

    m_screen = vterm_obtain_screen(m_vterm);
    static const VTermScreenCallbacks callbacks = {
        &QTermLibvtermParser::handleScreenDamage,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &QTermLibvtermParser::handleScreenResize,
        &QTermLibvtermParser::handleScrollbackPushline,
        nullptr,
        &QTermLibvtermParser::handleScrollbackClear,
        nullptr,
    };
    vterm_screen_set_callbacks(m_screen, &callbacks, this);
    vterm_screen_set_damage_merge(m_screen, VTERM_DAMAGE_SCREEN);
    vterm_screen_reset(m_screen, 1);
}

void QTermLibvtermParser::destroyVTerm()
{
    if (m_vterm) {
        vterm_free(m_vterm);
        m_vterm = nullptr;
        m_screen = nullptr;
    }
}

void QTermLibvtermParser::rebuildVisibleCells()
{
    if (!m_surfaceModel || !m_screen) {
        return;
    }

    const int columns = std::max(1, m_terminalSize.width());
    const int rows = std::max(1, m_terminalSize.height());
    QVector<QTermCell> cells(columns * rows);

    const int scrollbackLineCount = std::max(0, static_cast<int>(m_scrollbackLines.size()));
    m_surfaceModel->setScrollbackLineCount(scrollbackLineCount);

    const int topLine = std::clamp(m_surfaceModel->viewportTopLine(), 0, scrollbackLineCount);
    for (int row = 0; row < rows; ++row) {
        const int lineIndex = topLine + row;
        if (lineIndex < m_scrollbackLines.size()) {
            const QVector<QTermCell> &scrollbackLine = m_scrollbackLines.at(lineIndex);
            const int copyCount = std::min(columns, static_cast<int>(scrollbackLine.size()));
            for (int column = 0; column < copyCount; ++column) {
                cells[row * columns + column] = scrollbackLine.at(column);
            }
            continue;
        }

        const int screenRow = lineIndex - m_scrollbackLines.size();
        if (screenRow < 0 || screenRow >= rows) {
            continue;
        }

        for (int column = 0; column < columns; ++column) {
            VTermScreenCell screenCell;
            if (!vterm_screen_get_cell(m_screen, VTermPos{screenRow, column}, &screenCell)) {
                continue;
            }

            cells[row * columns + column] = toCell(m_screen, screenCell);
        }
    }

    m_surfaceModel->setVisibleCells(std::move(cells));
    m_surfaceModel->setDirtyRegion(QRect(0, 0, columns, rows));
    m_dirty = false;
}

void QTermLibvtermParser::markDirty()
{
    m_dirty = true;
}

int QTermLibvtermParser::pushScrollbackLine(int cols, const VTermScreenCell *cells)
{
    QVector<QTermCell> line;
    line.reserve(cols);
    for (int column = 0; column < cols; ++column) {
        line.push_back(toCell(m_screen, cells[column]));
    }

    m_scrollbackLines.push_back(std::move(line));
    const int overflow = m_scrollbackLines.size() - m_maxScrollbackLines;
    if (overflow > 0) {
        m_scrollbackLines.erase(m_scrollbackLines.begin(), m_scrollbackLines.begin() + overflow);
    }

    markDirty();
    return 1;
}

int QTermLibvtermParser::clearScrollback()
{
    m_scrollbackLines.clear();
    markDirty();
    return 1;
}