#include <QTerm/QTermLocalShellScanner.h>

#include <QDir>
#include <QFileInfo>
#include <QSet>

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <QFile>
#include <QTextStream>
#endif

namespace QTerm {

QTermLocalShellScanner::QTermLocalShellScanner(QObject *parent)
    : QObject(parent)
{
}

QList<QTermShellInfo> QTermLocalShellScanner::availableShells() const
{
    QList<QTermShellInfo> result;

#if defined(Q_OS_WIN)
    // On Windows: prefer the system default shell first (%ComSpec%), then
    // add known shells in preference order for explicit selection in UI.
    struct Candidate {
        const wchar_t *exe;
        const char *name;
    };
    constexpr Candidate candidates[] = {
        { L"pwsh.exe",       "PowerShell 7"       },
        { L"powershell.exe", "Windows PowerShell" },
        { L"cmd.exe",        "Command Prompt"     },
    };

    QSet<QString> seenPrograms;
    const auto appendIfNew = [&](const QString &name, const QString &program) {
        if (program.isEmpty())
            return;
        const QString key = QDir::fromNativeSeparators(program).toLower();
        if (seenPrograms.contains(key))
            return;
        seenPrograms.insert(key);
        result.append(QTermShellInfo(name, program));
    };

    QString comspec = qEnvironmentVariable("ComSpec").trimmed();
    if (comspec.startsWith(QLatin1Char('"')) && comspec.endsWith(QLatin1Char('"')) && comspec.size() > 1)
        comspec = comspec.mid(1, comspec.size() - 2);
    if (QFileInfo(comspec).exists())
        appendIfNew(QStringLiteral("System Default (ComSpec)"), comspec);

    for (const auto &c : candidates) {
        wchar_t fullPath[MAX_PATH];
        if (SearchPathW(nullptr, c.exe, nullptr, MAX_PATH, fullPath, nullptr) != 0) {
            appendIfNew(QString::fromLatin1(c.name), QString::fromWCharArray(fullPath));
        }
    }
#else
    // On Unix: read /etc/shells for valid login shells, prepend $SHELL so the
    // user's configured default is always first in the list.
    const QString defaultShell = qEnvironmentVariable("SHELL");

    QStringList paths;
    QFile shellsFile(QStringLiteral("/etc/shells"));
    if (shellsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&shellsFile);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith(QLatin1Char('#')))
                paths.append(line);
        }
    }

    // Ensure $SHELL is at the front, deduplicated.
    if (!defaultShell.isEmpty()) {
        paths.removeAll(defaultShell);
        paths.prepend(defaultShell);
    }

    for (const QString &path : std::as_const(paths)) {
        if (QFileInfo(path).isExecutable())
            result.append(QTermShellInfo(QFileInfo(path).fileName(), path));
    }
#endif

    return result;
}

} // namespace QTerm
