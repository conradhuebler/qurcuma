// qurcumachathost.h - What qurcuma tells CuteChat's ChatDock about itself.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The assistant's dock is the shared CuteChat one. Everything that makes
// it qurcuma's lives here: the tool registry and dispatcher (shared with the script dock and the
// command palette), the words of the dock, and two callbacks into MainWindow for the parts that
// need the window: the system prompt (it follows the autonomy level) and the approval dialog
// (graded by the same ladder as the script dock, with per-session grants).
//
// The host is built before MainWindow has its tools: the dock is part of the dock set and the
// View menu needs it. The callbacks are therefore set afterwards; until then the prompt is empty
// and nothing is approved.
#pragma once

#ifdef USE_LLM

#include <cutechat/chathost.h>

#include <functional>

class QurcumaChatHost : public ChatHost {
public:
    /// @p dispatcher is the one MainWindow configured (log sink); it belongs to the GUI thread.
    explicit QurcumaChatHost(ToolDispatcher* dispatcher);

    /// Called before every question (the prompt depends on autonomy and thinking effort).
    std::function<QString()> prompt;
    /// Called on the GUI thread; the dock records the decision in the chat.
    std::function<bool(const ToolSpec&, const QJsonObject&)> approve;

    QString systemPrompt() const override;
    void registerTools(ToolRegistry&) override {} // the registry is the application's own (sharedRegistry)
    ToolRegistry* sharedRegistry() override;
    ToolDispatcher* sharedDispatcher() override { return m_dispatcher; }
    bool approveToolCall(const ToolSpec& spec, const QJsonObject& args, QWidget* parent) override;

    // Tools wait here (job_status, watch_simulation): the agent loop must not be the GUI thread.
    bool agentOnWorkerThread() const override { return true; }
    bool showAgentControls() const override { return true; }
    bool rememberAutonomy() const override { return true; }
    QString settingsPrefix() const override { return QStringLiteral("llm"); }
    ChatUiTexts uiTexts() const override;

private:
    ToolDispatcher* m_dispatcher;
};

#endif // USE_LLM
