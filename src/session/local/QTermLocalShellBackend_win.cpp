// Windows implementation of QTermLocalShellBackend using ConPTY.
// This file is only compiled on Windows (see src/CMakeLists.txt).
// Requires Windows 10 SDK 1809 (10.0.17763) or later for CreatePseudoConsole.

#include <QTerm/QTermLocalShellBackend.h>

#include <QThread>
#include <QTimer>
#include <QVector>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>   // CreateToolhelp32Snapshot — child-process probe (coarse)

// MinGW distributions may not expose ConPTY declarations in Windows headers.
// Provide guarded declarations so this backend can compile with Qt MinGW kits.
#ifndef HPCON
typedef HANDLE HPCON;
#endif

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

extern "C" {
#ifndef QTERM_HAS_CREATE_PSEUDOCONSOLE_DECL
HRESULT WINAPI CreatePseudoConsole(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON *phPC);
#define QTERM_HAS_CREATE_PSEUDOCONSOLE_DECL 1
#endif

#ifndef QTERM_HAS_RESIZE_PSEUDOCONSOLE_DECL
HRESULT WINAPI ResizePseudoConsole(HPCON hPC, COORD size);
#define QTERM_HAS_RESIZE_PSEUDOCONSOLE_DECL 1
#endif

#ifndef QTERM_HAS_CLOSE_PSEUDOCONSOLE_DECL
void WINAPI ClosePseudoConsole(HPCON hPC);
#define QTERM_HAS_CLOSE_PSEUDOCONSOLE_DECL 1
#endif
}

namespace QTerm {

// ── ReadThread ────────────────────────────────────────────────────────────────
// Performs blocking ReadFile on the ConPTY output pipe in a dedicated thread.
// The owning backend stops this thread by closing the pipe handle, which causes
// ReadFile to return with ERROR_BROKEN_PIPE or ERROR_INVALID_HANDLE.

class QTermLocalShellBackend::ReadThread : public QThread
{
    Q_OBJECT
public:
    explicit ReadThread(HANDLE hPipeOut, QObject *parent = nullptr)
        : QThread(parent)
        , m_hPipeOut(hPipeOut)
    {}

signals:
    void dataChunkReceived(const QByteArray &data);
    void pipeEnded();

protected:
    void run() override
    {
        constexpr DWORD kBufSize = 65536;
        QByteArray buf(static_cast<qsizetype>(kBufSize), Qt::Uninitialized);
        DWORD bytesRead = 0;

        for (;;) {
            bytesRead = 0;
            if (!ReadFile(m_hPipeOut, buf.data(), kBufSize, &bytesRead, nullptr))
                break;
            if (bytesRead > 0)
                emit dataChunkReceived(QByteArray(buf.constData(), static_cast<qsizetype>(bytesRead)));
        }
        emit pipeEnded();
    }

private:
    HANDLE m_hPipeOut = nullptr;
};

// ── QTermLocalShellBackend ────────────────────────────────────────────────────

QTermLocalShellBackend::QTermLocalShellBackend(QObject *parent)
    : QTermSessionBackend(parent)
    , m_processExitTimer(new QTimer(this))
{
    m_processExitTimer->setInterval(50);
    connect(m_processExitTimer, &QTimer::timeout, this, [this]() {
        pollProcessExit();
    });
}

QTermLocalShellBackend::~QTermLocalShellBackend()
{
    close();
}

QString QTermLocalShellBackend::program() const { return m_program; }
QStringList QTermLocalShellBackend::arguments() const { return m_arguments; }
QString QTermLocalShellBackend::workingDirectory() const { return m_workingDirectory; }
QProcessEnvironment QTermLocalShellBackend::processEnvironment() const { return m_environment; }

void QTermLocalShellBackend::setProgram(const QString &program)
{
    m_program = program;
    emit programChanged();
}

void QTermLocalShellBackend::setArguments(const QStringList &arguments)
{
    m_arguments = arguments;
    emit argumentsChanged();
}

void QTermLocalShellBackend::setWorkingDirectory(const QString &workingDirectory)
{
    m_workingDirectory = workingDirectory;
    emit workingDirectoryChanged();
}

void QTermLocalShellBackend::setProcessEnvironment(const QProcessEnvironment &environment)
{
    m_environment = environment;
}

void QTermLocalShellBackend::open()
{
    if (state() == Open || state() == Opening)
        return;
    close();

    const QString prog = resolvedProgram();
    if (prog.isEmpty()) {
        emitErrorOccurred(SpawnFailed, QStringLiteral("No program configured for ConPTY session."));
        return;
    }

    setState(Opening);

    // Create pipe pairs.
    //   stdin  pipe: we write to hPipeInWrite, process reads from hPipeInRead.
    //   stdout pipe: process writes to hPipeOutWrite, we read from hPipeOutRead.
    HANDLE hPipeInRead = nullptr, hPipeInWrite = nullptr;
    HANDLE hPipeOutRead = nullptr, hPipeOutWrite = nullptr;

    if (!CreatePipe(&hPipeInRead, &hPipeInWrite, nullptr, 0)) {
        emitErrorOccurred(SpawnFailed, QStringLiteral("CreatePipe (stdin) failed: error %1")
            .arg(static_cast<unsigned long>(GetLastError())));
        setState(Closed);
        return;
    }

    if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, nullptr, 0)) {
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        emitErrorOccurred(SpawnFailed, QStringLiteral("CreatePipe (stdout) failed: error %1")
            .arg(static_cast<unsigned long>(GetLastError())));
        setState(Closed);
        return;
    }

    // Create the pseudo-console. ConPTY takes ownership of hPipeInRead and
    // hPipeOutWrite; close our copies immediately after.
    COORD size = { static_cast<SHORT>(m_columns), static_cast<SHORT>(m_rows) };
    HPCON hPC = nullptr;
    const HRESULT hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hPC);
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeOutWrite);

    if (FAILED(hr)) {
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        emitErrorOccurred(SpawnFailed, QStringLiteral("CreatePseudoConsole failed: HRESULT 0x%1")
            .arg(static_cast<unsigned long>(hr), 8, 16, QChar(u'0')));
        setState(Closed);
        return;
    }

    // Build STARTUPINFOEX with PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE.
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    auto *attrList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        ::HeapAlloc(GetProcessHeap(), 0, attrListSize));

    auto cleanupOnError = [&]() {
        if (attrList) {
            DeleteProcThreadAttributeList(attrList);
            ::HeapFree(GetProcessHeap(), 0, attrList);
        }
        ClosePseudoConsole(hPC);
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        setState(Closed);
    };

    if (!attrList || !InitializeProcThreadAttributeList(attrList, 1, 0, &attrListSize)) {
        cleanupOnError();
        emitErrorOccurred(SpawnFailed, QStringLiteral("Failed to initialize process attribute list."));
        return;
    }

    if (!UpdateProcThreadAttribute(attrList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            hPC, sizeof(HPCON), nullptr, nullptr)) {
        const DWORD err = GetLastError();
        cleanupOnError();
        emitErrorOccurred(SpawnFailed, QStringLiteral("UpdateProcThreadAttribute failed: error %1")
            .arg(static_cast<unsigned long>(err)));
        return;
    }

    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.lpAttributeList = attrList;

    // Command line — writable buffer required by CreateProcessW.
    std::wstring cmdLine = buildCommandLine().toStdWString();

    // Working directory — nullptr means inherit from parent process.
    const std::wstring workDirW = m_workingDirectory.isEmpty()
        ? std::wstring()
        : m_workingDirectory.toStdWString();
    const wchar_t *workDirPtr = m_workingDirectory.isEmpty() ? nullptr : workDirW.c_str();

    // Environment block: UTF-16, null-separated key=value pairs, double-null end.
    QProcessEnvironment env = m_environment.isEmpty()
        ? QProcessEnvironment::systemEnvironment()
        : m_environment;
    if (!env.contains(QStringLiteral("TERM")))
        env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));

    QVector<wchar_t> envBlock;
    for (const QString &key : env.keys()) {
        const std::wstring entry = (key + QLatin1Char('=') + env.value(key)).toStdWString();
        for (wchar_t ch : entry)
            envBlock.append(ch);
        envBlock.append(L'\0');
    }
    envBlock.append(L'\0');

    PROCESS_INFORMATION pi = {};
    const BOOL created = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr, nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envBlock.data(),
        workDirPtr,
        &si.StartupInfo,
        &pi);

    DeleteProcThreadAttributeList(attrList);
    ::HeapFree(GetProcessHeap(), 0, attrList);

    if (!created) {
        const DWORD err = GetLastError();
        ClosePseudoConsole(hPC);
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        emitErrorOccurred(SpawnFailed, QStringLiteral("CreateProcess failed: \"%1\" (error %2)")
            .arg(prog)
            .arg(static_cast<unsigned long>(err)));
        setState(Closed);
        return;
    }

    m_hPC = hPC;
    m_hProcess = pi.hProcess;
    m_hThread = pi.hThread;
    m_hPipeIn = hPipeInWrite;
    m_hPipeOut = hPipeOutRead;

    m_readThread = new ReadThread(hPipeOutRead);
    connect(m_readThread, &ReadThread::dataChunkReceived, this,
        [this](const QByteArray &data) {
            onReadThreadDataReceived(data);
        }, Qt::QueuedConnection);
    connect(m_readThread, &ReadThread::pipeEnded, this,
        [this]() {
            onReadThreadPipeEnded();
        }, Qt::QueuedConnection);
    m_readThread->start();

    m_processExitTimer->start();
    setState(Open);
}

void QTermLocalShellBackend::close()
{
    if (state() == Closed || state() == Closing)
        return;
    setState(Closing);
    doClose();
    setState(Closed);
}

void QTermLocalShellBackend::writeData(const QByteArray &data)
{
    if (!m_hPipeIn || data.isEmpty())
        return;

    const char *cursor = data.constData();
    DWORD remaining = static_cast<DWORD>(data.size());
    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(static_cast<HANDLE>(m_hPipeIn), cursor, remaining, &written, nullptr)) {
            emitErrorOccurred(ConnectionLost, QStringLiteral("ConPTY write failed: error %1")
                .arg(static_cast<unsigned long>(GetLastError())));
            close();
            return;
        }
        cursor += written;
        remaining -= written;
    }
}

void QTermLocalShellBackend::resize(int columns, int rows)
{
    m_columns = qMax(1, columns);
    m_rows = qMax(1, rows);
    if (!m_hPC)
        return;
    const COORD size = { static_cast<SHORT>(m_columns), static_cast<SHORT>(m_rows) };
    ResizePseudoConsole(static_cast<HPCON>(m_hPC), size);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void QTermLocalShellBackend::doClose()
{
    m_processExitTimer->stop();

    // CancelIoEx unblocks any ReadFile blocked in the read thread.
    // CloseHandle alone does not reliably wake up a blocking ReadFile on Windows.
    if (m_hPipeOut) {
        CancelIoEx(static_cast<HANDLE>(m_hPipeOut), nullptr);
        CloseHandle(static_cast<HANDLE>(m_hPipeOut));
        m_hPipeOut = nullptr;
    }

    // Disconnect and wait for the read thread (exits promptly after CancelIoEx).
    if (m_readThread) {
        disconnect(m_readThread, nullptr, this, nullptr);
        if (!m_readThread->wait(3000))
            m_readThread->terminate();
        m_readThread->wait();
        delete m_readThread;
        m_readThread = nullptr;
    }

    if (m_hProcess) {
        TerminateProcess(static_cast<HANDLE>(m_hProcess), 0);
        WaitForSingleObject(static_cast<HANDLE>(m_hProcess), 2000);
        CloseHandle(static_cast<HANDLE>(m_hProcess));
        m_hProcess = nullptr;
    }

    if (m_hThread) {
        CloseHandle(static_cast<HANDLE>(m_hThread));
        m_hThread = nullptr;
    }

    if (m_hPipeIn) {
        CloseHandle(static_cast<HANDLE>(m_hPipeIn));
        m_hPipeIn = nullptr;
    }

    // ClosePseudoConsole after the process and pipes are released to avoid a
    // potential deadlock in ConPTY's internal plumbing.
    if (m_hPC) {
        ClosePseudoConsole(static_cast<HPCON>(m_hPC));
        m_hPC = nullptr;
    }
}

void QTermLocalShellBackend::onReadThreadDataReceived(const QByteArray &data)
{
    if (state() != Open)
        return;
    emitDataReceived(data);
}

void QTermLocalShellBackend::onReadThreadPipeEnded()
{
    if (state() == Open)
        close();
}

void QTermLocalShellBackend::pollProcessExit()
{
    if (!m_hProcess || state() != Open)
        return;
    if (WaitForSingleObject(static_cast<HANDLE>(m_hProcess), 0) != WAIT_OBJECT_0)
        return;

    DWORD exitCode = 0;
    GetExitCodeProcess(static_cast<HANDLE>(m_hProcess), &exitCode);

    m_processExitTimer->stop();
    setState(Closing);
    doClose();

    if (exitCode != 0)
        emitErrorOccurred(ConnectionLost, QStringLiteral("Process exited with code %1.").arg(exitCode));
    else
        setState(Closed);
}

QString QTermLocalShellBackend::resolvedProgram() const
{
    if (!m_program.isEmpty())
        return m_program;
    // %ComSpec% is the Windows equivalent of $SHELL (usually cmd.exe).
    const QString comspec = qEnvironmentVariable("ComSpec");
    return comspec.isEmpty() ? QStringLiteral("cmd.exe") : comspec;
}

QString QTermLocalShellBackend::buildCommandLine() const
{
    // Windows CreateProcess requires a single command-line string.
    // Quote any argument that contains spaces or double quotes.
    auto quoteArg = [](const QString &arg) -> QString {
        if (!arg.contains(QLatin1Char(' '))
                && !arg.contains(QLatin1Char('\t'))
                && !arg.contains(QLatin1Char('"'))) {
            return arg;
        }
        QString q = arg;
        q.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    };

    QStringList parts;
    parts.append(quoteArg(resolvedProgram()));
    for (const QString &arg : m_arguments)
        parts.append(quoteArg(arg));
    return parts.join(QLatin1Char(' '));
}

// ── Foreground process detection (Windows, coarse) ──────────────────────────
// The Windows console layer has no Unix-style "foreground process group", so a
// foreground/background command cannot be distinguished precisely. We use an
// agreed approximation: walk the process snapshot and check whether the shell
// (m_hProcess) has any child process — if so, treat it as a command running.
// Queried purely on demand; no Job Object is attached on the spawn path.

QTermSessionBackend::WorkState QTermLocalShellBackend::workState() const
{
    if (!m_hProcess)
        return WorkUnknown;
    const DWORD shellPid = ::GetProcessId(static_cast<HANDLE>(m_hProcess));
    if (shellPid == 0)
        return WorkUnknown;
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return WorkUnknown;
    WorkState result = WorkIdle;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ParentProcessID == shellPid) { result = WorkBusy; break; }
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return result;
}

QString QTermLocalShellBackend::foregroundProcessName() const
{
    if (!m_hProcess)
        return {};
    const DWORD shellPid = ::GetProcessId(static_cast<HANDLE>(m_hProcess));
    if (shellPid == 0)
        return {};
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return {};
    QString name;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ParentProcessID == shellPid) {
                name = QString::fromWCharArray(pe.szExeFile);   // first child's name (coarse)
                break;
            }
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return name;
}

} // namespace QTerm

// Required so AUTOMOC processes Q_OBJECT classes defined in this .cpp file.
#include "QTermLocalShellBackend_win.moc"
