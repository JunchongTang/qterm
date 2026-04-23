#ifndef QTERM_QTERMSELECTIONMODEL_H
#define QTERM_QTERMSELECTIONMODEL_H

#include <QString>
#include <QVariantList>

namespace QTerm {

class QTermBuffer;

struct QTermSelectionSnapshot
{
    bool hasSelection = false;
    int startRow = 0;
    int startColumn = 0;
    int endRow = 0;
    int endColumn = 0;
    QString selectedText;
};

class QTermSelectionModel
{
public:
    void setTerminalSize(int columns, int rows);
    void setVisibleProjection(const QVariantList &visibleLineWrapFlags,
                              const QVariantList &visibleLineColumnTexts);

    void clearSelection();
    void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);
    void selectWordAt(const QTermBuffer &buffer, int row, int column);
    void selectLogicalLineAt(const QTermBuffer &buffer, int row);

    const QTermSelectionSnapshot &snapshot() const noexcept;

private:
    void updateSelectedText();

    int m_rows = 24;
    int m_columns = 80;
    QVariantList m_visibleLineWrapFlags;
    QVariantList m_visibleLineColumnTexts;
    QTermSelectionSnapshot m_snapshot;
};

} // namespace QTerm

#endif // QTERM_QTERMSELECTIONMODEL_H