#ifndef QTERM_QTERMSURFACEMODEL_H
#define QTERM_QTERMSURFACEMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace QTerm {

class QTermTerminal;

class QTermSurfaceModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY sizeChanged)
    Q_PROPERTY(int cursorRow READ cursorRow NOTIFY cursorChanged)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY cursorChanged)
    Q_PROPERTY(bool cursorVisible READ cursorVisible NOTIFY cursorChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(int selectionStartRow READ selectionStartRow NOTIFY selectionChanged)
    Q_PROPERTY(int selectionStartColumn READ selectionStartColumn NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEndRow READ selectionEndRow NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEndColumn READ selectionEndColumn NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(QStringList visibleLines READ visibleLines NOTIFY visibleLinesChanged)
    Q_PROPERTY(QVariantList visibleLineRuns READ visibleLineRuns NOTIFY visibleLineRunsChanged)
    Q_PROPERTY(QString plainText READ plainText NOTIFY plainTextChanged)

public:
    explicit QTermSurfaceModel(QObject *parent = nullptr);

    int rows() const noexcept;
    int columns() const noexcept;
    int cursorRow() const noexcept;
    int cursorColumn() const noexcept;
    bool cursorVisible() const noexcept;
    bool hasSelection() const noexcept;
    int selectionStartRow() const noexcept;
    int selectionStartColumn() const noexcept;
    int selectionEndRow() const noexcept;
    int selectionEndColumn() const noexcept;
    QString selectedText() const;
    QStringList visibleLines() const;
    QVariantList visibleLineRuns() const;
    QString plainText() const;

    void setSize(int columns, int rows);
    void setCursor(int row, int column, bool visible);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);
    void setSelectionController(QTermTerminal *terminal);
    void setSelectionSnapshot(bool hasSelection, int startRow, int startColumn, int endRow, int endColumn);
    void setVisibleLineWrapFlags(const QVariantList &visibleLineWrapFlags);
    void setVisibleLineColumnTexts(const QVariantList &visibleLineColumnTexts);
    void setVisibleLines(const QStringList &visibleLines);
    void setVisibleLineRuns(const QVariantList &visibleLineRuns);
    void setPlainText(const QString &plainText);

signals:
    void sizeChanged();
    void cursorChanged();
    void selectionChanged();
    void visibleLinesChanged();
    void visibleLineRunsChanged();
    void plainTextChanged();

private:
    void updateSelectedText();

    int m_rows = 24;
    int m_columns = 80;
    int m_cursorRow = 0;
    int m_cursorColumn = 0;
    bool m_cursorVisible = true;
    bool m_hasSelection = false;
    int m_selectionStartRow = 0;
    int m_selectionStartColumn = 0;
    int m_selectionEndRow = 0;
    int m_selectionEndColumn = 0;
    QTermTerminal *m_selectionController = nullptr;
    QString m_selectedText;
    QVariantList m_visibleLineWrapFlags;
    QVariantList m_visibleLineColumnTexts;
    QStringList m_visibleLines;
    QVariantList m_visibleLineRuns;
    QString m_plainText;
};

} // namespace QTerm

#endif // QTERM_QTERMSURFACEMODEL_H