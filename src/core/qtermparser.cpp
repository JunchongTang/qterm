#include <QTerm/qtermparser.h>

QTermParser::QTermParser(QObject *parent)
    : QObject(parent)
{
}

QTermParser::~QTermParser() = default;

QTermSurfaceModel *QTermParser::surfaceModel() const
{
    return m_surfaceModel;
}

void QTermParser::setSurfaceModel(QTermSurfaceModel *surfaceModel)
{
    m_surfaceModel = surfaceModel;
}