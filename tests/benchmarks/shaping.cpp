#include <QGuiApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QTextLayout>
#include <QRawFont>
#include <QFontMetricsF>

// Compares the two things a text renderer does per line:
//   1. shaping   -- characters to glyph ids and positions (HarfBuzz)
//   2. glyph id lookup only -- what a monospaced grid actually needs
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QFont font(QStringLiteral("Menlo"), 16);
    font.setStyleStrategy(QFont::PreferAntialias);

    // A representative terminal row: 120 columns.
    QString ascii; for (int i = 0; i < 120; ++i) ascii += QChar(u'a' + (i % 26));
    QString cjk;   for (int i = 0; i < 60;  ++i) cjk += QStringLiteral("中文");

    QRawFont rawFont = QRawFont::fromFont(font);
    const int rows = 30, frames = 200;   // one screen, 200 repaints

    for (const auto &pair : {std::make_pair(QStringLiteral("ASCII"), ascii),
                             std::make_pair(QStringLiteral("CJK  "), cjk)}) {
        const QString &line = pair.second;

        QElapsedTimer t; t.start();
        for (int f = 0; f < frames; ++f)
            for (int r = 0; r < rows; ++r) {
                QTextLayout layout(line, font);
                layout.beginLayout();
                QTextLine tl = layout.createLine();
                tl.setLineWidth(100000);
                layout.endLayout();
                (void)tl.glyphRuns();
            }
        const double shaped = t.elapsed() / 1000.0;

        t.restart();
        for (int f = 0; f < frames; ++f)
            for (int r = 0; r < rows; ++r) {
                const QList<quint32> ids = rawFont.glyphIndexesForString(line);
                (void)ids;
            }
        const double lookup = t.elapsed() / 1000.0;

        qInfo().noquote() << QStringLiteral("  %1  full shaping (QTextLayout) %2 s   glyph lookup only %3 s   ratio %4x")
                                 .arg(pair.first)
                                 .arg(shaped, 0, 'f', 3).arg(lookup, 0, 'f', 3)
                                 .arg(shaped / qMax(0.001, lookup), 0, 'f', 1);
    }
    qInfo().noquote() << QStringLiteral("  (%1 frames x %2 rows = %3 text lines)")
                             .arg(frames).arg(rows).arg(frames * rows);
    return 0;
}
