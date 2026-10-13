// addcomputerdialog.cpp - see addcomputerdialog.h
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute)

#include "addcomputerdialog.h"

#include "remote/hostprobe.h"
#include "sshconfig.h"

#include <QComboBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
bool isPattern(const QString& host)
{
    return host.contains(QLatin1Char('*')) || host.contains(QLatin1Char('?')) || host.contains(QLatin1Char('!'));
}
}

void AddComputerDialog::fillHostCombo(QComboBox* combo, bool withLocal, const QString& select)
{
    const QSignalBlocker block(combo);
    combo->clear();
    if (withLocal)
        combo->addItem(tr("This computer"), QString());
    QStringList seen;
    for (const SshConfigEntry& e : SshConfigParser::parseFile()) {
        if (isPattern(e.host) || seen.contains(e.host))
            continue;
        seen << e.host;
        combo->addItem(e.host, e.host);
        combo->setItemData(combo->count() - 1, tr("From ~/.ssh/config"), Qt::ToolTipRole);
    }
    for (const remote::HostProfile& p : remote::HostProfiles::load()) {
        if (seen.contains(p.name))
            continue;  // an ~/.ssh/config entry of the same name wins, as it did before
        seen << p.name;
        combo->addItem(p.name, p.name);
        combo->setItemData(combo->count() - 1, tr("Added in qurcuma: %1").arg(p.target), Qt::ToolTipRole);
    }
    const int idx = select.isEmpty() ? -1 : combo->findData(select);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

bool AddComputerDialog::isSavedComputer(const QString& name)
{
    if (name.isEmpty())
        return false;
    for (const SshConfigEntry& e : SshConfigParser::parseFile())
        if (e.host == name)
            return false;
    return remote::HostProfiles::find(name).has_value();
}

QString AddComputerDialog::defaultServerCommand(const QString& host)
{
    const QString stored = QSettings().value(QStringLiteral("remote/serverCommand/") + host).toString();
    if (!stored.isEmpty())
        return stored;
    if (const auto p = remote::HostProfiles::find(host))
        return p->effectiveServerCommand();
    return QStringLiteral("qurcuma-server");
}

AddComputerDialog::AddComputerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add computer"));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("A computer that runs the simulations for this window. qurcuma reaches it over ssh without a password "
                                "prompt (key or ssh agent), and qurcuma-server must be installed on it."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    m_address = new QLineEdit(this);
    m_address->setPlaceholderText(tr("user@host.example.org or 192.0.2.10"));
    m_address->setToolTip(tr("Host name or IP address, optionally with the user name in front."));
    form->addRow(tr("Address:"), m_address);
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("shown in the list; taken from the address if empty"));
    form->addRow(tr("Name:"), m_name);
    m_port = new QSpinBox(this);
    m_port->setRange(0, 65535);
    m_port->setSpecialValueText(tr("22 (default)"));
    form->addRow(tr("ssh port:"), m_port);
    m_key = new QLineEdit(this);
    m_key->setPlaceholderText(tr("empty: ssh agent and default keys"));
    auto* browse = new QPushButton(tr("Browse..."), this);
    auto* keyRow = new QHBoxLayout;
    keyRow->addWidget(m_key);
    keyRow->addWidget(browse);
    form->addRow(tr("Key file:"), keyRow);
    m_serverCommand = new QLineEdit(this);
    m_serverCommand->setPlaceholderText(QStringLiteral("qurcuma-server"));
    m_serverCommand->setToolTip(tr("Command that starts qurcuma-server on that computer (full path if it is not in the PATH). "
                                   "The test fills it in when it finds the server."));
    form->addRow(tr("Server command:"), m_serverCommand);
    layout->addLayout(form);

    m_test = new QPushButton(tr("Test connection"), this);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* testRow = new QHBoxLayout;
    testRow->addWidget(m_test);
    testRow->addWidget(m_status, 1);
    layout->addLayout(testRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    layout->addWidget(buttons);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, tr("ssh key file"), QDir::homePath() + QStringLiteral("/.ssh"));
        if (!f.isEmpty())
            m_key->setText(f);
    });
    connect(m_test, &QPushButton::clicked, this, &AddComputerDialog::runTest);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddComputerDialog::tryAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    for (QLineEdit* e : { m_address, m_name, m_key })
        connect(e, &QLineEdit::textChanged, this, &AddComputerDialog::updateButtons);
    connect(m_port, QOverload<int>::of(&QSpinBox::valueChanged), this, &AddComputerDialog::updateButtons);
    updateButtons();
}

remote::HostProfile AddComputerDialog::profile() const
{
    remote::HostProfile p;
    p.target = m_address->text().trimmed();
    p.name = m_name->text().trimmed();
    if (p.name.isEmpty()) {
        // the host part of the address (after '@', before ':' of an IPv6-less host:port)
        p.name = p.target.section(QLatin1Char('@'), -1);
    }
    p.port = m_port->value();
    p.identityFile = m_key->text().trimmed();
    p.serverCommand = m_serverCommand->text().trimmed();
    return p;
}

void AddComputerDialog::updateButtons()
{
    QString why;
    const bool valid = !m_address->text().trimmed().isEmpty() && profile().isValid(&why);
    m_test->setEnabled(valid && !m_probe);
    m_ok->setEnabled(valid);
    if (!valid && !m_address->text().trimmed().isEmpty())
        m_status->setText(why);
    else if (!m_probe)
        m_status->clear();
}

void AddComputerDialog::runTest()
{
    if (m_probe)
        return;
    m_test->setEnabled(false);
    m_status->setText(tr("Testing ..."));
    m_probe = new remote::HostProbe(profile(), this);
    connect(m_probe, &remote::HostProbe::progress, m_status, &QLabel::setText);
    connect(m_probe, &remote::HostProbe::finished, this, &AddComputerDialog::onProbeFinished);
    m_probe->start();
}

void AddComputerDialog::onProbeFinished(bool sshOk, const QString& serverPath, bool serverRuns, const QString& message)
{
    m_probe->deleteLater();
    m_probe = nullptr;
    if (!serverPath.isEmpty() && m_serverCommand->text().trimmed().isEmpty())
        m_serverCommand->setText(serverPath);
    const bool good = sshOk && serverRuns;
    m_status->setText(QStringLiteral("<span style='color:%1'>%2</span>").arg(good ? QStringLiteral("#2e7d32") : QStringLiteral("#c62828"), message.toHtmlEscaped()));
    updateButtons();
}

void AddComputerDialog::tryAccept()
{
    QString why;
    remote::HostProfile p = profile();
    for (const SshConfigEntry& e : SshConfigParser::parseFile())
        if (e.host == p.name) {
            m_status->setText(tr("'%1' is already an entry of ~/.ssh/config; choose another name (or use that entry).").arg(p.name));
            return;
        }
    if (!remote::HostProfiles::add(p, &why)) {
        m_status->setText(why);
        return;
    }
    accept();
}
