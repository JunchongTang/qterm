#ifndef QTERM_QTERMSURFACEMODEL_H
#define QTERM_QTERMSURFACEMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace QTerm {

class QTermTerminal;

/*!
    \class QTermSurfaceModel
    \inmodule QTerm
    \brief Exposes the visible terminal buffer, cursor and selection state to views and QML.
*/
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
    // Search-match highlights for the current viewport: list of
    // { row, startColumn, endColumn, current(bool) } in viewport-row coords.
    Q_PROPERTY(QVariantList searchHighlights READ searchHighlights NOTIFY searchHighlightsChanged)

public:
    explicit QTermSurfaceModel(QObject *parent = nullptr);

    /*!
        \brief Returns the number of rows in the current viewport.
    */
    int rows() const noexcept;
    /*!
        \brief Returns the number of columns in the current viewport.
    */
    int columns() const noexcept;
    /*!
        \brief Returns the cursor row position.
    */
    int cursorRow() const noexcept;
    /*!
        \brief Returns the cursor column position.
    */
    int cursorColumn() const noexcept;
    /*!
        \brief Returns whether the cursor is currently visible.
    */
    bool cursorVisible() const noexcept;
    // Cursor shape: 0 = Block, 1 = Underline, 2 = Bar (matches CursorShape enum)
    /*!
        \brief Returns the cursor shape as an integer code.
    */
    int cursorShape() const noexcept;
    /*!
        \brief Returns whether a selection range is currently active.
    */
    bool hasSelection() const noexcept;
    /*!
        \brief Returns whether the selection is currently visible.
    */
    bool selectionVisible() const noexcept;
    /*!
        \brief Returns the selection start row.
    */
    int selectionStartRow() const noexcept;
    /*!
        \brief Returns the selection start column.
    */
    int selectionStartColumn() const noexcept;
    /*!
        \brief Returns the selection end row.
    */
    int selectionEndRow() const noexcept;
    /*!
        \brief Returns the selection end column.
    */
    int selectionEndColumn() const noexcept;
    /*!
        \brief Returns the currently selected text.
    */
    QString selectedText() const;
    /*!
        \brief Returns the currently visible terminal lines.
    */
    QStringList visibleLines() const;
    /*!
        \brief Returns the visible line runs used for rendering.
    */
    QVariantList visibleLineRuns() const;
    /*!
        \brief Returns the full terminal text as plain text.

        Computed on demand; delegates to the terminal's dumpPlainText().
    */
    QString plainText() const;
    /*!
        \brief Returns the highlight ranges for the current search matches.
    */
    QVariantList searchHighlights() const;

    /*!
        \brief Clears the current selection.
    */
    Q_INVOKABLE void clearSelection();
    /*!
        \brief Sets a new selection range.
    */
    Q_INVOKABLE void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);

signals:
    void sizeChanged();
    void cursorChanged();
    void selectionChanged();
    void visibleLinesChanged();
    void visibleLineRunsChanged();
    void visibleLineRunsChangedPartial(QVector<int> changedRows);
    void plainTextChanged();
    void searchHighlightsChanged();

private:
    friend class QTermTerminal;

    void setSize(int columns, int rows);
    void setCursor(int row, int column, bool visible, int shape = 0);
    void setSelectionController(QTermTerminal *terminal);
    void setSelectionSnapshot(bool hasSelection, int startRow, int startColumn, int endRow, int endColumn);
    void setSelectionSnapshot(bool hasSelection, int startRow, int startColumn, int endRow, int endColumn, const QString &selectedText);
    void setVisibleLines(const QStringList &visibleLines);
    void setVisibleLineRuns(const QVariantList &visibleLineRuns);
    void setVisibleLineRunsPartial(const QVector<int> &rows, const QVariantList &runs);
    void setSearchHighlights(const QVariantList &highlights);

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
    QVariantList m_searchHighlights;
};

} // namespace QTerm

#endif // QTERM_QTERMSURFACEMODEL_H