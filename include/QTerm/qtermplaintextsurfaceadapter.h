#pragma once

#include <QObject>

class QTermParser;
class QTermSession;
class QTermSurfaceModel;

class QTermPlainTextSurfaceAdapter : public QObject
{
    Q_OBJECT

public:
    explicit QTermPlainTextSurfaceAdapter(QObject *parent = nullptr);
    ~QTermPlainTextSurfaceAdapter() override;

    QTermSession *session() const;
    void setSession(QTermSession *session);

    QTermSurfaceModel *surfaceModel() const;
    void setSurfaceModel(QTermSurfaceModel *surfaceModel);

    QTermParser *parser() const;
    void setParser(QTermParser *parser);

private slots:
    void consumeSessionData();
    void synchronizeParserTerminalSize();
    void resetParser();

private:
    QTermSession *m_session = nullptr;
    QTermSurfaceModel *m_surfaceModel = nullptr;
    QTermParser *m_parser = nullptr;
};