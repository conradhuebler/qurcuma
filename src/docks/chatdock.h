// chatdock.h - The assistant, as a dock.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A view onto an LlmSession: it shows the conversation and
// what the assistant did, and it takes questions. It holds no policy and runs no
// tools; both belong to the session.
//
// The conversation is a column of widgets rather than one QTextEdit, because each
// turn's reasoning is its own collapsible block: it opens while the model thinks,
// folds away when the answer arrives, and STAYS in the history so it can be read
// again later. A single text view cannot do that.
#pragma once

#include "dockconfig.h"

#include "llm/llmconfig.h"  // LlmModelInfo

#include <QDockWidget>
#include <QString>

class CollapsibleSection;
class LlmSession;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QVBoxLayout;
struct ToolResult;

class ChatDock : public QDockWidget {
    Q_OBJECT

public:
    explicit ChatDock(QWidget* parent = nullptr);

    /// Follow @p session: show its answers, its tool calls and its failures, and
    /// send questions to it.
    void attachSession(LlmSession* session);

    /// Profile names for the selector; @p active is preselected.
    void setProfiles(const QStringList& names, const QString& active);
    QString currentProfile() const;

    /// Models the endpoint offers; @p current is preselected. The box stays
    /// editable so a model the endpoint does not list can still be typed.
    void setModels(const QStringList& models, const QString& current);
    QString currentModel() const;
    /// Context length and capabilities of the selected model, shown next to it.
    /// A model that cannot call tools is worth saying out loud: it will answer in
    /// prose and never touch the structure.
    void setModelInfo(const LlmModelInfo& info);

    /// A line above the input, for "no endpoint configured" and the like.
    void setStatus(const QString& text, bool isError = false);

signals:
    /// The user picked another endpoint profile.
    void profileChanged(const QString& name);
    /// The user picked another model.
    void modelChanged(const QString& model);

private:
    void setupUI();
    void submit();
    void setBusy(bool busy);

    /// Append a labelled block to the conversation and return it, so streamed
    /// text can keep extending the same one.
    QLabel* addBlock(const QString& who, const QString& text, const QString& colour);
    /// The collapsible reasoning block of the current turn, created on demand.
    void appendReasoning(const QString& text);
    void appendAnswer(const QString& text);
    void beginTurn();
    void scrollToEnd();

    LlmSession* m_session = nullptr;

    QScrollArea* m_scroll = nullptr;
    QWidget* m_messages = nullptr;
    QVBoxLayout* m_messageLayout = nullptr;   ///< holds the blocks, then a stretch

    QLineEdit* m_input = nullptr;
    QPushButton* m_sendButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QComboBox* m_profileBox = nullptr;
    QComboBox* m_modelBox = nullptr;
    QLabel* m_modelInfo = nullptr;
    QLabel* m_status = nullptr;

    /// Widgets of the turn in progress. Null between turns.
    QLabel* m_currentAnswer = nullptr;
    CollapsibleSection* m_currentReasoning = nullptr;
    QTextEdit* m_currentReasoningText = nullptr;
};
