#include "QTermCore.h"

#include "QTermInputEncoder.h"
#include "QTermInputExecutor.h"

#include <QtGlobal>

namespace {

constexpr int kMinimumColumns = 2;
constexpr int kMinimumRows = 1;
constexpr int kTabWidth = 8;

} // namespace

namespace QTerm {

QTermCore::QTermCore(QObject *parent)
    : QObject(parent)
    , m_primaryScreen(80, 24)
    , m_alternateScreen(80, 24)
{
}

int QTermCore::rows() const noexcept
{
    return activeScreen().buffer.rows();
}

int QTermCore::columns() const noexcept
{
    return activeScreen().buffer.columns();
}

QString QTermCore::title() const
{
    return m_title;
}

QString QTermCore::currentDirectory() const
{
    return m_currentDirectory;
}

int QTermCore::shellZone() const noexcept
{
    return m_shellZone;
}

int QTermCore::lastExitCode() const noexcept
{
    return m_lastExitCode;
}

QString QTermCore::debugPlainText() const
{
    return activeScreen().buffer.debugPlainText();
}

QTermCursorState QTermCore::cursorState() const noexcept
{
    return activeScreen().cursorState;
}

const QTermBuffer &QTermCore::buffer() const noexcept
{
    return activeScreen().buffer;
}

QTermBuffer &QTermCore::buffer() noexcept
{
    return m_modeState.alternateScreenActive ? m_alternateScreen.buffer : m_primaryScreen.buffer;
}

const QTermModeState &QTermCore::modeState() const noexcept
{
    return m_modeState;
}

QByteArray QTermCore::encodeKey(int key, const QString &text) const
{
    return QTermInputEncoder::encodeKey(m_modeState, key, text);
}

QByteArray QTermCore::encodePaste(const QString &text) const
{
    return QTermInputEncoder::encodePaste(m_modeState, text);
}

QString QTermCore::hyperlinkUrl(int id) const
{
    return m_hyperlinkUrls.value(id, QString());
}

int QTermCore::registerHyperlinkUrl(const QString &url)
{
    if (url.isEmpty()) {
        return 0;
    }
    // Reuse existing id if the same URL was already registered
    for (auto it = m_hyperlinkUrls.constBegin(); it != m_hyperlinkUrls.constEnd(); ++it) {
        if (it.value() == url) {
            return it.key();
        }
    }
    const int id = m_nextHyperlinkId++;
    m_hyperlinkUrls.insert(id, url);
    return id;
}

void QTermCore::clear()
{
    m_primaryScreen.clear();
    m_alternateScreen.clear();
    m_modeState = QTermModeState();
    emit debugPlainTextChanged();
    emit cursorStateChanged();
}

void QTermCore::writePlainText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    QTermInputExecutor executor(m_primaryScreen, m_alternateScreen, m_modeState);
    executor.setBellHandler([this]() {
        emit bell();
    });
    executor.setWindowTitleHandler([this](const QString &title) {
        if (m_title == title) {
            return;
        }

        m_title = title;
        emit titleChanged(m_title);
    });
    executor.setCurrentDirectoryHandler([this](const QString &url) {
        if (m_currentDirectory == url) {
            return;
        }

        m_currentDirectory = url;
        emit currentDirectoryChanged(m_currentDirectory);
    });
    executor.setWriteClipboardHandler([this](const QString &text) {
        emit clipboardWriteRequested(text);
    });
    executor.setShellZoneHandler([this](const QString &mark) {
        int newZone = m_shellZone;
        int newExitCode = m_lastExitCode;

        if (mark.startsWith(u'A')) {
            newZone = 1; // Prompt
        } else if (mark.startsWith(u'B')) {
            newZone = 2; // CommandInput
        } else if (mark.startsWith(u'C')) {
            newZone = 3; // Output
        } else if (mark.startsWith(u'D')) {
            newZone = 0; // Unknown (command ended)
            const qsizetype sep = mark.indexOf(u';');
            if (sep >= 0) {
                bool ok = false;
                const int code = mark.sliced(sep + 1).toInt(&ok);
                if (ok) {
                    newExitCode = code;
                }
            } else {
                newExitCode = 0;
            }
        }

        if (newZone != m_shellZone || newExitCode != m_lastExitCode) {
            m_shellZone = newZone;
            m_lastExitCode = newExitCode;
            emit shellZoneChanged();
        }
    });
    executor.setOutboundHandler([this](const QByteArray &data) {
        emit outboundData(data);
    });
    executor.setRegisterHyperlinkHandler([this](const QString &url) {
        return registerHyperlinkUrl(url);
    });
    const MouseTracking prevMouseTracking = m_modeState.mouseTracking;
    const bool prevAlternate = m_modeState.alternateScreenActive;
    const CursorShape prevCursorShape = m_modeState.cursorShape;

    m_textParser.parse(text, executor);

    emit debugPlainTextChanged();
    emit cursorStateChanged();

    if (m_modeState.mouseTracking != prevMouseTracking ||
        m_modeState.alternateScreenActive != prevAlternate ||
        m_modeState.cursorShape != prevCursorShape) {
        emit modeStateChanged();
    }
}

void QTermCore::setTerminalSize(int columns, int rows)
{
    const int boundedColumns = qMax(columns, kMinimumColumns);
    const int boundedRows = qMax(rows, kMinimumRows);
    if (m_primaryScreen.buffer.columns() == boundedColumns && m_primaryScreen.buffer.rows() == boundedRows) {
        return;
    }

    m_primaryScreen.resize(boundedColumns, boundedRows);
    m_alternateScreen.resize(boundedColumns, boundedRows);
    emit sizeChanged();
    emit debugPlainTextChanged();
    emit cursorStateChanged();
}

void QTermCore::sendKey(int key, const QString &text)
{
    const QByteArray encoded = encodeKey(key, text);
    if (encoded.isEmpty()) {
        return;
    }

    emit outboundData(encoded);
}

void QTermCore::sendPaste(const QString &text)
{
    const QByteArray encoded = encodePaste(text);
    if (encoded.isEmpty()) {
        return;
    }

    emit outboundData(encoded);
}

const QTermScreenState &QTermCore::activeScreen() const noexcept
{
    return m_modeState.alternateScreenActive ? m_alternateScreen : m_primaryScreen;
}

} // namespace QTerm