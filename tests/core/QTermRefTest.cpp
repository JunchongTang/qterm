// Replay-based reference tests.
//
// Each case is a directory under tests/refdata/ holding the raw bytes a real
// session produced (input.bin) plus a snapshot of the grid they should produce
// (expect.json). The test feeds the bytes through the same decode-and-parse
// path the session layer uses and compares the result.
//
// The snapshot deliberately records only what the public API exposes -- the
// visible text, the style runs a renderer consumes, the cursor and the history
// depth -- so that changing how cells are stored internally does not
// invalidate the corpus. That is the whole point: these tests exist to guard
// representation changes.
//
// Regenerate every snapshot after an intentional behaviour change with:
//     QTERM_UPDATE_REFS=1 ./qterm_ref_tests
// and review the resulting diff before committing it.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringDecoder>

#include "core/QTermCore.h"

using namespace Qt::Literals::StringLiterals;

namespace QTerm {

namespace {

QString refDataRoot()
{
    return QStringLiteral(QTERM_REFDATA_DIR);
}

// Style runs come back as QVariantMap; keep only what differs from the default
// so a snapshot stays readable and reviewable in a diff.
QJsonObject runToJson(const QVariantMap &run)
{
    QJsonObject object;
    object.insert(u"text"_s, run.value(u"text"_s).toString());
    object.insert(u"columns"_s, run.value(u"columns"_s).toInt());

    const auto flag = [&](const QString &key) {
        if (run.value(key).toBool()) {
            object.insert(key, true);
        }
    };
    flag(u"bold"_s);
    flag(u"dim"_s);
    flag(u"italic"_s);
    flag(u"underline"_s);
    flag(u"strikethrough"_s);
    flag(u"inverse"_s);

    const auto number = [&](const QString &key, int neutral) {
        const int value = run.value(key, neutral).toInt();
        if (value != neutral) {
            object.insert(key, value);
        }
    };
    number(u"foregroundIndex"_s, -1);
    number(u"backgroundIndex"_s, -1);
    number(u"foregroundRgb"_s, -1);
    number(u"backgroundRgb"_s, -1);
    number(u"hyperlinkId"_s, 0);
    return object;
}

QJsonObject snapshot(const QTermCore &core)
{
    const QTermBuffer &buffer = core.buffer();

    QJsonArray lines;
    const QStringList texts = buffer.visibleLineTexts();
    const QVariantList runs = buffer.visibleLineRuns();
    for (int row = 0; row < texts.size(); ++row) {
        QJsonObject line;
        line.insert(u"text"_s, texts.at(row));

        QJsonArray runArray;
        if (row < runs.size()) {
            const QVariantList rowRuns = runs.at(row).toList();
            for (const QVariant &run : rowRuns) {
                runArray.append(runToJson(run.toMap()));
            }
        }
        line.insert(u"runs"_s, runArray);
        lines.append(line);
    }

    QJsonObject cursor;
    cursor.insert(u"row"_s, core.cursorState().row);
    cursor.insert(u"column"_s, core.cursorState().column);

    QJsonObject object;
    object.insert(u"columns"_s, core.columns());
    object.insert(u"rows"_s, core.rows());
    object.insert(u"historyLines"_s, buffer.historyLineCount());
    object.insert(u"cursor"_s, cursor);
    object.insert(u"lines"_s, lines);
    return object;
}

// Mirrors how QTermTerminal drives the core: a streaming UTF-8 decoder feeding
// chunks, so a multi-byte sequence split across reads is exercised too.
void replay(QTermCore &core, const QByteArray &input, int chunkSize)
{
    QStringDecoder decoder(QStringDecoder::Utf8);
    for (qsizetype offset = 0; offset < input.size(); offset += chunkSize) {
        core.writePlainText(decoder(input.mid(offset, chunkSize)));
    }
}

} // namespace

class QTermRefTest : public QObject
{
    Q_OBJECT

private slots:
    void replaysToExpectedGrid_data();
    void replaysToExpectedGrid();
    void splittingInputIntoChunksDoesNotChangeResult_data();
    void splittingInputIntoChunksDoesNotChangeResult();
};

void QTermRefTest::replaysToExpectedGrid_data()
{
    QTest::addColumn<QString>("caseDir");

    const QDir root(refDataRoot());
    const QStringList cases = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QVERIFY2(!cases.isEmpty(), qPrintable(u"no reference cases under "_s + root.absolutePath()));

    for (const QString &name : cases) {
        QTest::newRow(qPrintable(name)) << root.absoluteFilePath(name);
    }
}

void QTermRefTest::replaysToExpectedGrid()
{
    QFETCH(QString, caseDir);

    QFile meta(caseDir + u"/case.json"_s);
    QVERIFY2(meta.open(QIODevice::ReadOnly), qPrintable(meta.fileName()));
    const QJsonObject config = QJsonDocument::fromJson(meta.readAll()).object();

    QFile inputFile(caseDir + u"/input.bin"_s);
    QVERIFY2(inputFile.open(QIODevice::ReadOnly), qPrintable(inputFile.fileName()));
    const QByteArray input = inputFile.readAll();

    QTermCore core;
    core.setTerminalSize(config.value(u"columns"_s).toInt(80),
                         config.value(u"rows"_s).toInt(24));
    if (config.contains(u"scrollback"_s)) {
        core.setMaximumScrollbackLines(config.value(u"scrollback"_s).toInt());
    }
    replay(core, input, config.value(u"chunk"_s).toInt(4096));

    const QJsonObject actual = snapshot(core);
    const QString expectPath = caseDir + u"/expect.json"_s;

    if (qEnvironmentVariableIsSet("QTERM_UPDATE_REFS")) {
        QFile out(expectPath);
        QVERIFY2(out.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(expectPath));
        out.write(QJsonDocument(actual).toJson(QJsonDocument::Indented));
        QSKIP("snapshot regenerated");
    }

    QFile expectFile(expectPath);
    QVERIFY2(expectFile.open(QIODevice::ReadOnly),
             qPrintable(expectPath + u" missing; run with QTERM_UPDATE_REFS=1"_s));
    const QJsonObject expected = QJsonDocument::fromJson(expectFile.readAll()).object();

    if (actual != expected) {
        // Point at the first differing row rather than dumping both documents.
        const QJsonArray a = actual.value(u"lines"_s).toArray();
        const QJsonArray e = expected.value(u"lines"_s).toArray();
        for (int row = 0; row < qMax(a.size(), e.size()); ++row) {
            const QJsonValue av = row < a.size() ? a.at(row) : QJsonValue();
            const QJsonValue ev = row < e.size() ? e.at(row) : QJsonValue();
            if (av != ev) {
                qWarning().noquote()
                    << u"first differing row "_s + QString::number(row)
                    << u"\n  actual   : "_s + QString::fromUtf8(
                           QJsonDocument(av.toObject()).toJson(QJsonDocument::Compact))
                    << u"\n  expected : "_s + QString::fromUtf8(
                           QJsonDocument(ev.toObject()).toJson(QJsonDocument::Compact));
                break;
            }
        }
        if (actual.value(u"cursor"_s) != expected.value(u"cursor"_s)) {
            qWarning().noquote() << u"cursor differs"_s;
        }
    }
    QCOMPARE(actual, expected);
}

void QTermRefTest::splittingInputIntoChunksDoesNotChangeResult_data()
{
    replaysToExpectedGrid_data();
}

// The parser carries state across writes, so the same bytes delivered in
// different-sized reads must land on the same grid. This catches sequences that
// are mishandled when split mid-escape.
void QTermRefTest::splittingInputIntoChunksDoesNotChangeResult()
{
    QFETCH(QString, caseDir);

    QFile meta(caseDir + u"/case.json"_s);
    QVERIFY(meta.open(QIODevice::ReadOnly));
    const QJsonObject config = QJsonDocument::fromJson(meta.readAll()).object();

    QFile inputFile(caseDir + u"/input.bin"_s);
    QVERIFY(inputFile.open(QIODevice::ReadOnly));
    const QByteArray input = inputFile.readAll();

    const int columns = config.value(u"columns"_s).toInt(80);
    const int rows = config.value(u"rows"_s).toInt(24);

    QJsonObject reference;
    for (int chunk : {1, 3, 64, 4096}) {
        QTermCore core;
        core.setTerminalSize(columns, rows);
        if (config.contains(u"scrollback"_s)) {
            core.setMaximumScrollbackLines(config.value(u"scrollback"_s).toInt());
        }
        replay(core, input, chunk);

        const QJsonObject result = snapshot(core);
        if (chunk == 1) {
            reference = result;
            continue;
        }
        if (result != reference) {
            qWarning().noquote() << u"chunk size "_s + QString::number(chunk)
                                    + u" produced a different grid than byte-at-a-time"_s;
        }
        QCOMPARE(result, reference);
    }
}

} // namespace QTerm

QTEST_MAIN(QTerm::QTermRefTest)

#include "QTermRefTest.moc"
