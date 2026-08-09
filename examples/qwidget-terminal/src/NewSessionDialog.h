#pragma once

#include <QDialog>

#include "SessionConfig.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QWidget;

// Shell / Serial / Telnet forms behind a segmented selector. Every page is laid
// out to three parameter rows so the dialog keeps a constant height as pages
// are switched.
class NewSessionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewSessionDialog(QWidget *parent = nullptr);

    SessionConfig sessionConfig() const;

    // Opens straight onto one of the three forms, so the dropdown's entries
    // land on the page they name.
    void selectType(int index) { selectPage(index); }

private:
    QWidget *buildPtyPage();
    QWidget *buildSerialPage();
    QWidget *buildTelnetPage();
    void selectPage(int index);
    void refreshSerialPorts();
    bool validate();

    QPushButton *m_typeButtons[3] = {};
    QStackedWidget *m_pages = nullptr;
    QLabel *m_errorLabel = nullptr;

    // Pty page
    QComboBox *m_shellCombo = nullptr;
    QLineEdit *m_programEdit = nullptr;
    QLineEdit *m_argumentsEdit = nullptr;
    QLineEdit *m_workdirEdit = nullptr;
    QLabel *m_shellLabel = nullptr;
    QList<QPair<QString, QString>> m_shells; // name, program

    // Serial page
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_flowCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;

    // Telnet page
    QLineEdit *m_hostEdit = nullptr;
    QLineEdit *m_portEdit = nullptr;
};
