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
    Q_PROPERTY(int cursorShape READ cursorShape NOTIFY cursorChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool selectionVisible READ selectionVisible NOTIFY selectionChanged)
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
    // Cursor shape: 0 = Block, 1 = Underline, 2 = Bar (matches CursorShape enum)
    int cursorShape() const noexcept;
    bool hasSelection() const noexcept;
    bool selectionVisible() const noexcept;
    int selectionStartRow() const noexcept;
    int selectionStartColumn() const noexcept;
    int selectionEndRow() const noexcept;
    int selectionEndColumn() const noexcept;
    QString selectedText() const;
    QStringList visibleLines() const;
    QVariantList visibleLineRuns() const;
    QString plainText() const;  // on-demand; delegates to terminal debugPlainText()

    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);

signals:
    void sizeChanged();
    void cursorChanged();
    void selectionChanged();
    void visibleLinesChanged();
    void visibleLineRunsChanged();
    void plainTextChanged();

private:
    friend class QTermTerminal;

    void setSize(int columns, int rows);
    void setCursor(int row, int column, bool visible, int shape = 0);
    void setSelectionController(QTermTerminal *terminal);
    void setSelectionSnapshot(bool hasSelection, int startRow, int startColumn, int endRow, int endColumn);
    void setSelectionSnapshot(bool hasSelection, int startRow, int startColumn, int endRow, int endColumn, const QString &selectedText);
    void setVisibleLines(const QStringList &visibleLines);
    void setVisibleLineRuns(const QVariantList &visibleLineRuns);

    int m_rows = 24;
    int m_columns = 80;
    int m_cursorRow = 0;
    int m_cursorColumn = 0;
    bool m_cursorVisible = true;
    int m_cursorShape = 0; // 0=Block, 1=Underline, 2=Bar
    bool m_hasSelection = false;
    int m_selectionStartRow = 0;
    int m_selectionStartColumn = 0;
    int m_selectionEndRow = 0;
    int m_selectionEndColumn = 0;
    QTermTerminal *m_selectionController = nullptr;
    QString m_selectedText;
    QStringList m_visibleLines;
    QVariantList m_visibleLineRuns;
};

} // namespace QTerm

#endif // QTERM_QTERMSURFACEMODEL_H