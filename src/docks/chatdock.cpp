// chatdock.cpp - The assistant, as a dock.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "chatdock.h"

#include "core/tool.h"
#include "llm/llmsession.h"
#include "widgets/collapsiblesection.h"

#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QIcon>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
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
    m_copyButton = new QPushButton(content);
    m_copyButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    m_copyButton->setToolTip(tr("Copy the whole conversation, including the reasoning, "
                                "to the clipboard"));
    m_copyButton->setMaximumWidth(30);
    connect(m_copyButton, &QPushButton::clicked, this, &ChatDock::copyTranscript);
    header->addWidget(m_copyButton);

    m_profileBox = new QComboBox(content);
    m_profileBox->setToolTip(tr("Endpoint profile (~/.config/qurcuma/llm.json)"));
    connect(m_profileBox, &QComboBox::currentTextChanged, this, &ChatDock::profileChanged);
    header->addWidget(m_profileBox);
    layout->addLayout(header);

    // Which models exist is a property of the endpoint, so it is asked, not
    // guessed. Editable, because a listing can be incomplete or absent.
    auto* modelRow = new QHBoxLayout;
    modelRow->addWidget(new QLabel(tr("Model:"), content));
    m_modelBox = new QComboBox(content);
    m_modelBox->setEditable(true);
    m_modelBox->setInsertPolicy(QComboBox::NoInsert);
    m_modelBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_modelBox->setMinimumContentsLength(18);
    connect(m_modelBox, &QComboBox::currentTextChanged, this, &ChatDock::modelChanged);
    modelRow->addWidget(m_modelBox, 1);
    layout->addLayout(modelRow);

    m_modelInfo = new QLabel(content);
    m_modelInfo->setWordWrap(true);
    m_modelInfo->hide();
    layout->addWidget(m_modelInfo);

    // How much may run unattended. A single control over the effect classes rather
    // than a per-tool list: an agentic run measures, calculates and moves atoms
    // dozens of times, and confirming each one makes the work impossible.
    auto* autonomyRow = new QHBoxLayout;
    autonomyRow->addWidget(new QLabel(tr("Autonomy:"), content));
    m_autonomyBox = new QComboBox(content);
    m_autonomyBox->addItem(tr("Ask before acting"), int(ToolAutonomy::Ask));
    m_autonomyBox->addItem(tr("Auto in the program"), int(ToolAutonomy::InProgram));
    m_autonomyBox->addItem(tr("Full auto"), int(ToolAutonomy::Full));
    m_autonomyBox->setToolTip(tr(
        "Ask: reading and display run freely, everything else asks.\n"
        "Auto in the program: calculations and structure changes run too -- they stay "
        "inside qurcuma and a structure change is undoable with Ctrl+Z.\n"
        "Full auto: writing files and starting external programs as well. Nothing asks."));
    connect(m_autonomyBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto level = ToolAutonomy(m_autonomyBox->itemData(index).toInt());
        updateAutonomyNote();
        emit autonomyChanged(level);
    });
    autonomyRow->addWidget(m_autonomyBox, 1);
    layout->addLayout(autonomyRow);

    // The level is never hidden: a run that needs no confirmation must still be
    // visibly a run that needs no confirmation.
    m_autonomyNote = new QLabel(content);
    m_autonomyNote->setWordWrap(true);
    m_autonomyNote->hide();
    layout->addWidget(m_autonomyNote);

    // The conversation is a column of widgets: each turn's reasoning has to be its
    // own foldable block that stays in the history, which one text view cannot do.
    m_scroll = new QScrollArea(content);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::StyledPanel);
    m_messages = new QWidget(m_scroll);
    m_messageLayout = new QVBoxLayout(m_messages);
    m_messageLayout->setContentsMargins(6, 6, 6, 6);
    m_messageLayout->setSpacing(6);
    m_messageLayout->addStretch(1);   // blocks are inserted before this
    m_scroll->setWidget(m_messages);
    layout->addWidget(m_scroll, 1);

    m_status = new QLabel(content);
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit(content);
    m_input->setPlaceholderText(
        tr("Ask about the loaded structure — reading and display run freely, "
           "anything that calculates or writes asks first."));
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

QLabel* ChatDock::addBlock(const QString& who, const QString& text, const QString& colour)
{
    if (!m_messageLayout)
        return nullptr;
    auto* label = new QLabel(m_messages);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    label->setTextFormat(Qt::RichText);
    label->setProperty("who", who);
    label->setProperty("colour", colour);
    // Every block keeps its unescaped text, not just the streamed answer: that is
    // what transcript() reads, and recovering it from the rendered HTML would be a
    // second escaping bug waiting to happen.
    label->setProperty("plain", text);
    label->setText(QStringLiteral("<b style=\"color:%1\">%2:</b> %3")
                       .arg(colour, who.toHtmlEscaped(), text.toHtmlEscaped()));
    // Before the trailing stretch, so the column keeps growing downwards.
    m_messageLayout->insertWidget(m_messageLayout->count() - 1, label);
    scrollToEnd();
    return label;
}

void ChatDock::setAutonomy(ToolAutonomy autonomy)
{
    if (!m_autonomyBox)
        return;
    QSignalBlocker block(m_autonomyBox);
    const int index = m_autonomyBox->findData(int(autonomy));
    if (index >= 0)
        m_autonomyBox->setCurrentIndex(index);
    updateAutonomyNote();
}

ToolAutonomy ChatDock::autonomy() const
{
    if (!m_autonomyBox)
        return ToolAutonomy::Ask;
    return ToolAutonomy(m_autonomyBox->currentData().toInt());
}

void ChatDock::updateAutonomyNote()
{
    if (!m_autonomyNote)
        return;
    switch (autonomy()) {
    case ToolAutonomy::Ask:
        m_autonomyNote->hide();
        return;
    case ToolAutonomy::InProgram:
        m_autonomyNote->setText(tr("Calculations and structure changes run without asking. "
                                   "Ctrl+Z takes a structure change back."));
        m_autonomyNote->setStyleSheet(QStringLiteral("color:#d4a017; font-weight:bold;"));
        break;
    case ToolAutonomy::Full:
        m_autonomyNote->setText(tr("Nothing asks — files are written and external programs "
                                   "started on the assistant's own decision."));
        m_autonomyNote->setStyleSheet(QStringLiteral("color:#c62828; font-weight:bold;"));
        break;
    }
    m_autonomyNote->show();
}

QString ChatDock::transcript() const
{
    if (!m_messageLayout)
        return QString();

    QStringList lines;
    for (int i = 0; i < m_messageLayout->count(); ++i) {
        QWidget* widget = m_messageLayout->itemAt(i)->widget();
        if (!widget)
            continue;   // the trailing stretch
        if (auto* label = qobject_cast<QLabel*>(widget)) {
            lines << QStringLiteral("%1: %2").arg(label->property("who").toString(),
                                                  label->property("plain").toString());
        } else if (auto* section = qobject_cast<CollapsibleSection*>(widget)) {
            // Folded or not: the reasoning is part of what happened, and someone
            // pasting a session elsewhere wants it.
            if (auto* text = section->findChild<QTextEdit*>())
                lines << QStringLiteral("[%1]\n%2").arg(section->title(), text->toPlainText());
        }
    }
    return lines.join(QStringLiteral("\n\n"));
}

void ChatDock::copyTranscript()
{
    const QString text = transcript();
    QGuiApplication::clipboard()->setText(text);
    setStatus(text.isEmpty()
            ? tr("Nothing to copy yet.")
            : tr("Conversation copied — %1 characters.").arg(QLocale().toString(text.size())));
}

void ChatDock::scrollToEnd()
{
    // After the layout has run, not before -- the maximum is not final yet.
    QTimer::singleShot(0, this, [this] {
        if (m_scroll && m_scroll->verticalScrollBar())
            m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->maximum());
    });
}

void ChatDock::appendReasoning(const QString& text)
{
    if (!m_messageLayout)
        return;
    if (!m_currentReasoning) {
        m_currentReasoning = new CollapsibleSection(tr("Reasoning"), m_messages);
        m_currentReasoningText = new QTextEdit(m_messages);
        m_currentReasoningText->setReadOnly(true);
        m_currentReasoningText->setMaximumHeight(180);
        auto* inner = new QVBoxLayout;
        inner->setContentsMargins(0, 0, 0, 0);
        inner->addWidget(m_currentReasoningText);
        m_currentReasoning->setContentLayout(inner);
        m_currentReasoning->setExpanded(true);   // open while it is happening
        m_messageLayout->insertWidget(m_messageLayout->count() - 1, m_currentReasoning);
    }

    m_currentReasoningText->moveCursor(QTextCursor::End);
    m_currentReasoningText->insertPlainText(text);
    if (auto* bar = m_currentReasoningText->verticalScrollBar())
        bar->setValue(bar->maximum());
    m_currentReasoning->setTitle(tr("Reasoning (%1 characters)")
            .arg(QLocale().toString(m_currentReasoningText->toPlainText().size())));
    scrollToEnd();
}

void ChatDock::appendAnswer(const QString& text)
{
    if (text.isEmpty())
        return;
    // The answer starting is the moment the thinking stops being interesting.
    if (m_currentReasoning && m_currentReasoning->isExpanded())
        m_currentReasoning->setExpanded(false);

    if (!m_currentAnswer) {
        m_currentAnswer = addBlock(tr("Assistant"), text, QStringLiteral("#2e7d32"));
        // Keep the plain text alongside: the next fragment extends it, and
        // recovering it from the rendered HTML would be a second escaping bug
        // waiting to happen.
        if (m_currentAnswer)
            m_currentAnswer->setProperty("plain", text);
        return;
    }
    // Extend the block already on screen rather than starting a new paragraph per
    // fragment. The stored plain text is kept alongside so escaping stays correct.
    const QString grown = m_currentAnswer->property("plain").toString() + text;
    m_currentAnswer->setProperty("plain", grown);
    m_currentAnswer->setText(QStringLiteral("<b style=\"color:#2e7d32\">%1:</b> %2")
                                 .arg(tr("Assistant"), grown.toHtmlEscaped()));
    scrollToEnd();
}

void ChatDock::beginTurn()
{
    // Deliberately NOT clearing anything: the previous turn's reasoning stays in
    // the conversation, folded, and can be reopened at any time.
    m_currentAnswer = nullptr;
    m_currentReasoning = nullptr;
    m_currentReasoningText = nullptr;
}

void ChatDock::attachSession(LlmSession* session)
{
    m_session = session;
    if (!session)
        return;

    connect(session, &LlmSession::assistantChunk, this, &ChatDock::appendAnswer);
    connect(session, &LlmSession::reasoningChunk, this, &ChatDock::appendReasoning);

    connect(session, &LlmSession::assistantMessage, this, [this](const QString& text) {
        // Already on screen when it was streamed; appending would double it.
        if (m_currentAnswer)
            return;
        if (!text.isEmpty())
            addBlock(tr("Assistant"), text, QStringLiteral("#2e7d32"));
        if (m_currentReasoning)
            m_currentReasoning->setExpanded(false);
    });

    connect(session, &LlmSession::toolStarted, this,
        [this](const QString& name, const QJsonObject& args) {
            // A tool call ends the current answer block: what follows belongs to
            // the next round and gets its own.
            m_currentAnswer = nullptr;
            if (m_currentReasoning)
                m_currentReasoning->setExpanded(false);
            const QString shown = args.isEmpty()
                ? QString()
                : QStringLiteral(" %1").arg(QString::fromUtf8(
                      QJsonDocument(args).toJson(QJsonDocument::Compact)));
            addBlock(tr("Tool"), name + shown, QStringLiteral("#616161"));
        });
    connect(session, &LlmSession::toolFinished, this,
        [this](const QString& name, const ToolResult& result) {
            if (result.ok)
                return;  // the failure text is what matters, and it goes to the model too
            addBlock(tr("Tool failed"), QStringLiteral("%1: %2").arg(name, result.error),
                QStringLiteral("#c62828"));
        });
    connect(session, &LlmSession::toolRefused, this,
        [this](const QString& name, const QString& reason) {
            addBlock(tr("Refused"), QStringLiteral("%1: %2").arg(name, reason),
                QStringLiteral("#ef6c00"));
        });
    connect(session, &LlmSession::failed, this, [this](const QString& error) {
        addBlock(tr("Error"), error, QStringLiteral("#c62828"));
    });
    connect(session, &LlmSession::busyChanged, this, [this](bool busy) {
        setBusy(busy);
        if (!busy && m_currentReasoning)
            m_currentReasoning->setExpanded(false);
    });
}

void ChatDock::submit()
{
    if (!m_session || m_input->text().trimmed().isEmpty())
        return;
    const QString question = m_input->text().trimmed();
    m_input->clear();
    beginTurn();
    addBlock(tr("You"), question, QStringLiteral("#1565c0"));
    m_session->ask(question);
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

void ChatDock::setModels(const QStringList& models, const QString& current)
{
    if (!m_modelBox)
        return;
    QSignalBlocker block(m_modelBox);
    m_modelBox->clear();
    m_modelBox->addItems(models);
    if (!current.isEmpty())
        m_modelBox->setCurrentText(current);
    else if (!models.isEmpty())
        m_modelBox->setCurrentIndex(0);
}

QString ChatDock::currentModel() const
{
    return m_modelBox ? m_modelBox->currentText() : QString();
}

void ChatDock::setModelInfo(const LlmModelInfo& info)
{
    if (!m_modelInfo)
        return;
    if (!info.detailsKnown) {
        m_modelInfo->hide();   // the endpoint said nothing; do not invent anything
        return;
    }

    QStringList parts;
    if (info.contextLength > 0)
        parts << tr("%1 tokens context").arg(QLocale().toString(info.contextLength));
    if (info.supportsVision)
        parts << tr("can see images");

    if (info.supportsTools) {
        m_modelInfo->setStyleSheet(QStringLiteral("color: palette(mid);"));
        parts.prepend(tr("can call tools"));
    } else {
        // The single most useful thing to say: this one will chat about the
        // structure and never actually look at it.
        m_modelInfo->setStyleSheet(QStringLiteral("color: #c62828;"));
        parts.prepend(tr("cannot call tools, so it will answer from guesswork"));
    }
    m_modelInfo->setText(parts.join(QStringLiteral(" · ")));
    m_modelInfo->show();
}

void ChatDock::setStatus(const QString& text, bool isError)
{
    if (!m_status)
        return;
    m_status->setText(text);
    m_status->setStyleSheet(isError ? QStringLiteral("color: #c62828;") : QString());
    m_status->setVisible(!text.isEmpty());
}
