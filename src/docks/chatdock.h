// chatdock.h - The assistant, as a dock.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A view onto an LlmSession: it shows the conversation and
// what the assistant did, and it takes questions. It holds no policy and runs no
// tools; both belong to the session.
#pragma once

#include "dockconfig.h"

#include <QDockWidget>
#include <QString>

class LlmSession;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
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

    /// A line above the input, for "no endpoint configured" and the like.
    void setStatus(const QString& text, bool isError = false);

signals:
    /// The user picked another endpoint profile.
    void profileChanged(const QString& name);

private:
    void setupUI();
    void submit();
    void appendBlock(const QString& who, const QString& text, const QString& colour);
    void setBusy(bool busy);

    LlmSession* m_session = nullptr;
    QTextEdit* m_conversation = nullptr;
    QLineEdit* m_input = nullptr;
    QPushButton* m_sendButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QComboBox* m_profileBox = nullptr;
    QLabel* m_status = nullptr;
};
