// qurcumachathost.cpp - What qurcuma tells CuteChat's ChatDock about itself.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - see the header.
#ifdef USE_LLM

#include "qurcumachathost.h"

#include "dockconfig.h"

#include <cutechat/toolregistry.h>

#include <QCoreApplication>

QurcumaChatHost::QurcumaChatHost(ToolDispatcher* dispatcher)
    : m_dispatcher(dispatcher)
{
}

QString QurcumaChatHost::systemPrompt() const
{
    return prompt ? prompt() : QString();
}

ToolRegistry* QurcumaChatHost::sharedRegistry()
{
    return &ToolRegistry::instance();
}

bool QurcumaChatHost::approveToolCall(const ToolSpec& spec, const QJsonObject& args, QWidget*)
{
    return approve ? approve(spec, args) : false;
}

ChatUiTexts QurcumaChatHost::uiTexts() const
{
    ChatUiTexts t;
    t.dockTitle = DockConfig::ChatDockTitle;
    t.dockObjectName = DockConfig::ChatDockObjectName; // the saved window state knows the dock by this name
    t.inputPlaceholder = QCoreApplication::translate("QurcumaChatHost",
        "Ask about the loaded structure — reading and display run freely, "
        "anything that calculates or writes asks first.");
    t.askLabel = QCoreApplication::translate("QurcumaChatHost", "Ask before acting");
    t.inProgramLabel = QCoreApplication::translate("QurcumaChatHost", "Auto in the program");
    t.fullLabel = QCoreApplication::translate("QurcumaChatHost", "Full auto");
    t.autonomyTip = QCoreApplication::translate("QurcumaChatHost",
        "Ask: reading and display run freely, everything else asks.\n"
        "Auto in the program: calculations and structure changes run too -- they stay "
        "inside qurcuma and a structure change is undoable with Ctrl+Z.\n"
        "Full auto: writing files and starting external programs as well. Nothing asks.");
    t.inProgramNote = QCoreApplication::translate("QurcumaChatHost",
        "Calculations and structure changes run without asking. Ctrl+Z takes a structure change back.");
    t.fullNote = QCoreApplication::translate("QurcumaChatHost",
        "Nothing asks — files are written and external programs started on the assistant's own decision.");
    t.chatFileLabel = QCoreApplication::translate("QurcumaChatHost", "qurcuma chat");
    t.approvalTitle = QCoreApplication::translate("QurcumaChatHost", "Assistant");
    t.noContextNotice = QString(); // qurcuma binds no chat to a structure; the history holds them all
    return t;
}

#endif // USE_LLM
