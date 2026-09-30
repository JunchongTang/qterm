#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>
#include "core/QTermCore.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QFile f(QString::fromLocal8Bit(argv[1]));
    if (!f.open(QIODevice::ReadOnly)) qFatal("cannot open");
    const QByteArray data = f.readAll();
    const qsizetype chunk = 65536;

    QList<double> runs;
    for (int r = 0; r < 5; ++r) {
        QTerm::QTermCore core;
        core.setTerminalSize(120, 30);
        core.setMaximumScrollbackLines(10000);
        QStringDecoder decoder(QStringDecoder::Utf8);
        QElapsedTimer t; t.start();
        for (qsizetype off = 0; off < data.size(); off += chunk)
            core.writePlainText(decoder(QByteArrayView(data).sliced(
                off, qMin(chunk, data.size() - off))));
        runs.append(t.elapsed() / 1000.0);
    }
    std::sort(runs.begin(), runs.end());
    QStringList s; for (double v : runs) s << QString::number(v, 'f', 3);
    qInfo().noquote() << QStringLiteral("runs : %1\nmedian: %2 s")
        .arg(s.join(QStringLiteral("  "))).arg(runs[2], 0, 'f', 3);
    qInfo().noquote() << QStringLiteral("RESULT\ttool=coremem\tpayload=%1\tseconds=%2")
        .arg(QFileInfo(QString::fromLocal8Bit(argv[1])).fileName())
        .arg(runs[2], 0, 'f', 3);
    return 0;
}
