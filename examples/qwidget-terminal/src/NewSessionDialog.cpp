#include "NewSessionDialog.h"

#include "Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSize>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <QTerm/QTermLocalShellScanner.h>
#include <QTerm/QTermSerialPortInfo.h>
#include <QTerm/QTermSerialPortScanner.h>
#include <QTerm/QTermShellInfo.h>

namespace {

QLabel *fieldLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("muted", false);
    return label;
}

// One labelled parameter row: label on top, control below.
QWidget *fieldRow(const QString &labelText, QWidget *control)
{
    auto *row = new QWidget;
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::instance()->space1() + 2);
    layout->addWidget(fieldLabel(labelText));
    layout->addWidget(control);
    return row;
}

QWidget *fieldRow(const QString &labelText, QLayout *controlLayout)
{
    auto *row = new QWidget;
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::instance()->space1() + 2);
    layout->addWidget(fieldLabel(labelText));
    layout->addLayout(controlLayout);
    return row;
}

// A page holding parameter rows, top-aligned with a stretch below so shorter
// pages leave the remaining rows blank instead of centring.
QWidget *page(const QList<QWidget *> &rows)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::instance()->space4());
    for (QWidget *row : rows)
        layout->addWidget(row);
    layout->addStretch();
    return widget;
}

} // namespace

NewSessionDialog::NewSessionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Session"));
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::instance()->space4(), Theme::instance()->space4(),
                             Theme::instance()->space4(), Theme::instance()->space4());
    root->setSpacing(Theme::instance()->space4());

    auto *title = new QLabel(tr("New Session"));
    title->setStyleSheet(QStringLiteral("font-size: %1px; font-weight: 500;")
                         .arg(Theme::instance()->textSm()));
    auto *subtitle = new QLabel(tr("Configure and start a new terminal session."));
    subtitle->setProperty("muted", true);

    auto *header = new QVBoxLayout;
    header->setSpacing(Theme::instance()->space1());
    header->addWidget(title);
    header->addWidget(subtitle);
    root->addLayout(header);

    // Segmented session-type selector.
    auto *typeRow = new QHBoxLayout;
    typeRow->setSpacing(Theme::instance()->space1());
    const QStringList typeNames = {tr("Shell"), tr("Serial"), tr("Telnet")};
    for (int i = 0; i < 3; ++i) {
        m_typeButtons[i] = new QPushButton(typeNames.at(i));
        m_typeButtons[i]->setCheckable(true);
        m_typeButtons[i]->setProperty("variant", "ghost");
        typeRow->addWidget(m_typeButtons[i]);
        connect(m_typeButtons[i], &QPushButton::clicked, this, [this, i] { selectPage(i); });
    }
    root->addLayout(typeRow);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildPtyPage());
    m_pages->addWidget(buildSerialPage());
    m_pages->addWidget(buildTelnetPage());
    // Pin to the Shell page's three-row height so switching pages does not
    // resize the dialog.
    m_pages->setFixedHeight(m_pages->widget(0)->sizeHint().height());
    root->addWidget(m_pages);

    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet(QStringLiteral("color: %1;")
                                .arg(Theme::instance()->destructive().name()));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QStringLiteral("color: %1;")
                             .arg(Theme::instance()->border().name(QColor::HexArgb)));
    root->addWidget(separator);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    auto *cancelButton = new QPushButton(tr("Cancel"));
    cancelButton->setProperty("variant", "outline");
    auto *connectButton = new QPushButton(tr("Connect"));
    connectButton->setDefault(true);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(connectButton);
    root->addLayout(buttonRow);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(connectButton, &QPushButton::clicked, this, [this] {
        if (validate())
            accept();
    });

    selectPage(0);
    setMinimumWidth(480);
}

QWidget *NewSessionDialog::buildPtyPage()
{
    m_shellCombo = new QComboBox;
    m_programEdit = new QLineEdit;
    m_programEdit->setPlaceholderText(
#if defined(Q_OS_WIN)
        tr("e.g. C:/Windows/System32/cmd.exe")
#else
        tr("e.g. /bin/zsh")
#endif
    );
    m_programEdit->hide();

    const auto shells = QTerm::QTermLocalShellScanner().availableShells();
    for (const auto &shell : shells) {
        m_shells.append({shell.name(), shell.program()});
        m_shellCombo->addItem(shell.name());
    }
    m_shellCombo->addItem(tr("Custom executable..."));

    // The custom-executable field shares the shell row rather than adding a
    // fourth one, keeping the page three rows tall in every state.
    auto *shellLayout = new QHBoxLayout;
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(Theme::instance()->space2());
    shellLayout->addWidget(m_shellCombo, 1);
    shellLayout->addWidget(m_programEdit, 1);

    auto *shellRow = fieldRow(tr("Shell"), shellLayout);
    m_shellLabel = shellRow->findChild<QLabel *>();

    connect(m_shellCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool custom = index == m_shells.size();
        m_programEdit->setVisible(custom);
        if (m_shellLabel)
            m_shellLabel->setText(custom ? tr("Shell / executable") : tr("Shell"));
    });

    m_argumentsEdit = new QLineEdit;
    m_argumentsEdit->setPlaceholderText(tr("Optional, e.g. -NoLogo"));
    m_workdirEdit = new QLineEdit;
    m_workdirEdit->setPlaceholderText(tr("Default"));

    return page({shellRow,
                 fieldRow(tr("Arguments"), m_argumentsEdit),
                 fieldRow(tr("Working directory"), m_workdirEdit)});
}

QWidget *NewSessionDialog::buildSerialPage()
{
    m_portCombo = new QComboBox;
    m_portCombo->setEditable(true);
    auto *refreshButton = new QPushButton;
    refreshButton->setProperty("variant", "outline");
    refreshButton->setIcon(Theme::instance()->icon(QStringLiteral("refresh-cw")));
    refreshButton->setIconSize(QSize(14, 14));
    refreshButton->setFixedWidth(32);
    refreshButton->setToolTip(tr("Refresh ports"));
    connect(refreshButton, &QPushButton::clicked, this, &NewSessionDialog::refreshSerialPorts);

    auto *portLayout = new QHBoxLayout;
    portLayout->setContentsMargins(0, 0, 0, 0);
    portLayout->setSpacing(Theme::instance()->space2());
    portLayout->addWidget(m_portCombo, 1);
    portLayout->addWidget(refreshButton);

    m_baudCombo = new QComboBox;
    m_baudCombo->addItems({"9600", "19200", "38400", "57600",
                           "115200", "230400", "460800", "921600"});
    m_baudCombo->setCurrentIndex(4);

    m_flowCombo = new QComboBox;
    m_flowCombo->addItem(tr("None"), QStringLiteral("none"));
    m_flowCombo->addItem(tr("Hardware"), QStringLiteral("hardware"));
    m_flowCombo->addItem(tr("Software"), QStringLiteral("software"));

    // Second row pairs baud rate with flow control so the page stays three rows.
    auto *linkLayout = new QHBoxLayout;
    linkLayout->setContentsMargins(0, 0, 0, 0);
    linkLayout->setSpacing(Theme::instance()->space3());
    linkLayout->addWidget(fieldRow(tr("Baud rate"), m_baudCombo), 1);
    linkLayout->addWidget(fieldRow(tr("Flow control"), m_flowCombo), 1);
    auto *linkRow = new QWidget;
    linkRow->setLayout(linkLayout);

    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItems({"5", "6", "7", "8"});
    m_dataBitsCombo->setCurrentIndex(3);

    m_parityCombo = new QComboBox;
    m_parityCombo->addItem(tr("None"), QStringLiteral("N"));
    m_parityCombo->addItem(tr("Even"), QStringLiteral("E"));
    m_parityCombo->addItem(tr("Odd"), QStringLiteral("O"));
    m_parityCombo->addItem(tr("Mark"), QStringLiteral("M"));
    m_parityCombo->addItem(tr("Space"), QStringLiteral("S"));

    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItems({"1", "2"});

    auto *frameLayout = new QHBoxLayout;
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(Theme::instance()->space3());
    frameLayout->addWidget(fieldRow(tr("Data bits"), m_dataBitsCombo), 1);
    frameLayout->addWidget(fieldRow(tr("Parity"), m_parityCombo), 1);
    frameLayout->addWidget(fieldRow(tr("Stop bits"), m_stopBitsCombo), 1);
    auto *frameRow = new QWidget;
    frameRow->setLayout(frameLayout);

    refreshSerialPorts();

    return page({fieldRow(tr("Port"), portLayout), linkRow, frameRow});
}

QWidget *NewSessionDialog::buildTelnetPage()
{
    m_hostEdit = new QLineEdit;
    // Public telnet playground, handy for exercising the renderer.
    m_hostEdit->setText(QStringLiteral("telehack.com"));
    m_hostEdit->setPlaceholderText(tr("hostname or IP"));

    m_portEdit = new QLineEdit(QStringLiteral("23"));
    m_portEdit->setValidator(new QIntValidator(1, 65535, this));
    m_portEdit->setFixedWidth(120);

    return page({fieldRow(tr("Host"), m_hostEdit),
                 fieldRow(tr("Port"), m_portEdit)});
}

void NewSessionDialog::selectPage(int index)
{
    m_pages->setCurrentIndex(index);
    for (int i = 0; i < 3; ++i) {
        m_typeButtons[i]->setChecked(i == index);
        m_typeButtons[i]->setProperty("variant", i == index ? "outline" : "ghost");
        m_typeButtons[i]->style()->unpolish(m_typeButtons[i]);
        m_typeButtons[i]->style()->polish(m_typeButtons[i]);
    }
    m_errorLabel->hide();
}

void NewSessionDialog::refreshSerialPorts()
{
    const QString current = m_portCombo->currentText();
    m_portCombo->clear();
    const auto ports = QTerm::QTermSerialPortScanner().availablePorts();
    for (const auto &port : ports)
        m_portCombo->addItem(port.portName());
    if (!current.isEmpty())
        m_portCombo->setCurrentText(current);
}

bool NewSessionDialog::validate()
{
    QString error;
    switch (m_pages->currentIndex()) {
    case 0:
        if (m_shellCombo->currentIndex() == m_shells.size()
            && m_programEdit->text().trimmed().isEmpty()) {
            error = tr("Executable path is required.");
            m_programEdit->setProperty("invalid", true);
        }
        break;
    case 1:
        if (m_portCombo->currentText().trimmed().isEmpty())
            error = tr("Select a serial port.");
        break;
    case 2:
        if (m_hostEdit->text().trimmed().isEmpty()) {
            error = tr("Host is required.");
            m_hostEdit->setProperty("invalid", true);
        }
        break;
    default:
        break;
    }

    if (error.isEmpty()) {
        m_errorLabel->hide();
        return true;
    }
    m_errorLabel->setText(error);
    m_errorLabel->show();
    return false;
}

SessionConfig NewSessionDialog::sessionConfig() const
{
    SessionConfig config;
    switch (m_pages->currentIndex()) {
    case 1:
        config.type = SessionConfig::Serial;
        config.portName = m_portCombo->currentText().trimmed();
        config.label = tr("Serial - %1").arg(config.portName);
        config.baudRate = m_baudCombo->currentText().toInt();
        config.dataBits = m_dataBitsCombo->currentText().toInt();
        config.parity = m_parityCombo->currentData().toString();
        config.stopBits = m_stopBitsCombo->currentText().toInt();
        config.flowControl = m_flowCombo->currentData().toString();
        break;
    case 2:
        config.type = SessionConfig::Telnet;
        config.host = m_hostEdit->text().trimmed();
        config.port = m_portEdit->text().toInt();
        config.label = tr("Telnet - %1").arg(config.host);
        break;
    default: {
        config.type = SessionConfig::Pty;
        const int index = m_shellCombo->currentIndex();
        const bool custom = index == m_shells.size();
        config.program = custom ? m_programEdit->text().trimmed()
                                : (index >= 0 && index < m_shells.size()
                                   ? m_shells.at(index).second : QString());
        config.label = m_shellCombo->currentText();
        const QString arguments = m_argumentsEdit->text().trimmed();
        if (!arguments.isEmpty())
            config.arguments = arguments.split(QRegularExpression(QStringLiteral("\\s+")));
        config.workingDirectory = m_workdirEdit->text().trimmed();
        break;
    }
    }
    return config;
}
