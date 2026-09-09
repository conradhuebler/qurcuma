// chatdock.cpp - The assistant, as a dock.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "chatdock.h"

#include "core/tool.h"
#include "llm/llmsession.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>

ChatDock::ChatDock(QWidget* parent)
    : QDockWidget(DockConfig::ChatDockTitle, parent)
{
    setObjectName(DockConfig::ChatDockObjectName);
    setupUI();
}

void ChatDock::setupUI()
{
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Assistant"), content);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    header->addWidget(title);
    header->addStretch();
    m_profileBox = new QComboBox(content);
    m_profileBox->setToolTip(tr("Endpoint profile (~/.config/qurcuma/llm.json)"));
    connect(m_profileBox, &QComboBox::currentTextChanged, this, &ChatDock::profileChanged);
    header->addWidget(m_profileBox);
    layout->addLayout(header);

    m_conversation = new QTextEdit(content);
    m_conversation->setReadOnly(true);
    m_conversation->setPlaceholderText(
        tr("Ask about the loaded structure. The assistant can read it, measure it and change how "
           "it is shown; anything that calculates or writes asks first."));
    layout->addWidget(m_conversation, 1);

    m_status = new QLabel(content);
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit(content);
    m_input->setPlaceholderText(tr("Ask something…"));
    connect(m_input, &QLineEdit::returnPressed, this, &ChatDock::submit);
    inputRow->addWidget(m_input, 1);

    m_sendButton = new QPushButton(tr("Send"), content);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatDock::submit);
    inputRow->addWidget(m_sendButton);

    m_stopButton = new QPushButton(tr("Stop"), content);
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, [this] {
        if (m_session)
            m_session->cancel();
    });
    inputRow->addWidget(m_stopButton);
    layout->addLayout(inputRow);

    setWidget(content);
}

void ChatDock::attachSession(LlmSession* session)
{
    m_session = session;
    if (!session)
        return;

    connect(session, &LlmSession::assistantMessage, this, [this](const QString& text) {
        if (!text.isEmpty())
            appendBlock(tr("Assistant"), text, QStringLiteral("#2e7d32"));
    });
    connect(session, &LlmSession::toolStarted, this,
        [this](const QString& name, const QJsonObject& args) {
            const QString shown = args.isEmpty()
                ? QString()
                : QStringLiteral(" %1").arg(QString::fromUtf8(
                      QJsonDocument(args).toJson(QJsonDocument::Compact)));
            appendBlock(tr("Tool"), name + shown, QStringLiteral("#616161"));
        });
    connect(session, &LlmSession::toolFinished, this,
        [this](const QString& name, const ToolResult& result) {
            if (result.ok)
                return;  // the failure text is what matters, and it comes back to the model
            appendBlock(tr("Tool failed"), QStringLiteral("%1: %2").arg(name, result.error),
                QStringLiteral("#c62828"));
        });
    connect(session, &LlmSession::toolRefused, this,
        [this](const QString& name, const QString& reason) {
            appendBlock(tr("Refused"), QStringLiteral("%1: %2").arg(name, reason),
                QStringLiteral("#ef6c00"));
        });
    connect(session, &LlmSession::failed, this, [this](const QString& error) {
        appendBlock(tr("Error"), error, QStringLiteral("#c62828"));
    });
    connect(session, &LlmSession::busyChanged, this, &ChatDock::setBusy);
}

void ChatDock::submit()
{
    if (!m_session || m_input->text().trimmed().isEmpty())
        return;
    const QString question = m_input->text().trimmed();
    m_input->clear();
    appendBlock(tr("You"), question, QStringLiteral("#1565c0"));
    m_session->ask(question);
}

void ChatDock::appendBlock(const QString& who, const QString& text, const QString& colour)
{
    if (!m_conversation)
        return;
    m_conversation->append(QStringLiteral("<b style=\"color:%1\">%2:</b> %3")
                               .arg(colour, who.toHtmlEscaped(), text.toHtmlEscaped()));
    if (auto* bar = m_conversation->verticalScrollBar())
        bar->setValue(bar->maximum());
}

void ChatDock::setBusy(bool busy)
{
    if (m_sendButton)
        m_sendButton->setEnabled(!busy);
    if (m_input)
        m_input->setEnabled(!busy);
    if (m_stopButton)
        m_stopButton->setEnabled(busy);
}

void ChatDock::setProfiles(const QStringList& names, const QString& active)
{
    if (!m_profileBox)
        return;
    QSignalBlocker block(m_profileBox);
    m_profileBox->clear();
    m_profileBox->addItems(names);
    if (!active.isEmpty())
        m_profileBox->setCurrentText(active);
}

QString ChatDock::currentProfile() const
{
    return m_profileBox ? m_profileBox->currentText() : QString();
}

void ChatDock::setStatus(const QString& text, bool isError)
{
    if (!m_status)
        return;
    m_status->setText(text);
    m_status->setStyleSheet(isError ? QStringLiteral("color: #c62828;") : QString());
    m_status->setVisible(!text.isEmpty());
}
