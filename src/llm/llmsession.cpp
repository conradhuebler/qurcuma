// llmsession.cpp - The agent loop.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "llmsession.h"

#include "core/loghub.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "llmclient.h"

#include <QJsonDocument>

namespace {

/// Read and Display run unattended; everything else has to be approved. This is
/// the operator's policy, expressed once.
bool needsApproval(ToolEffect effect)
{
    return effect != ToolEffect::Read && effect != ToolEffect::Display;
}

QJsonObject message(const QString& role, const QString& content)
{
    QJsonObject o;
    o.insert(QStringLiteral("role"), role);
    o.insert(QStringLiteral("content"), content);
    return o;
}

}  // namespace

LlmSession::LlmSession(LlmClient* client, ToolRegistry* registry, ToolDispatcher* dispatcher,
                       LogHub* hub, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_registry(registry)
    , m_dispatcher(dispatcher)
    , m_hub(hub)
{
    if (m_client) {
        connect(m_client, &LlmClient::finished, this, &LlmSession::onClientFinished);
        connect(m_client, &LlmClient::failed, this, &LlmSession::onClientFailed);
        connect(m_client, &LlmClient::contentChunk, this, &LlmSession::assistantChunk);
        connect(m_client, &LlmClient::reasoningChunk, this, &LlmSession::reasoningChunk);
    }
}

void LlmSession::setApprovalPolicy(ApprovalFn approve)
{
    m_approve = std::move(approve);
}

void LlmSession::setSystemPrompt(const QString& prompt)
{
    m_systemPrompt = prompt;
}

void LlmSession::setMaxIterations(int iterations)
{
    m_maxIterations = qMax(1, iterations);
}

void LlmSession::reset()
{
    m_messages = QJsonArray();
    m_iteration = 0;
}

void LlmSession::note(LogLevel level, const QString& text)
{
    if (m_hub)
        m_hub->append(QStringLiteral("llm"), level, text);
}

void LlmSession::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(busy);
}

QJsonArray LlmSession::toolCatalogue() const
{
    QJsonArray catalogue;
    if (!m_registry)
        return catalogue;
    for (const ToolSpec& spec : m_registry->all()) {
        QJsonObject function;
        function.insert(QStringLiteral("name"), spec.name);
        function.insert(QStringLiteral("description"), spec.description);
        function.insert(QStringLiteral("parameters"), spec.paramSchema);

        QJsonObject entry;
        entry.insert(QStringLiteral("type"), QStringLiteral("function"));
        entry.insert(QStringLiteral("function"), function);
        catalogue.append(entry);
    }
    return catalogue;
}

void LlmSession::ask(const QString& userMessage)
{
    if (m_busy) {
        emit failed(tr("the assistant is still working on the previous question"));
        return;
    }
    if (!m_client) {
        emit failed(tr("no endpoint client"));
        return;
    }

    if (m_messages.isEmpty() && !m_systemPrompt.isEmpty())
        m_messages.append(message(QStringLiteral("system"), m_systemPrompt));
    m_messages.append(message(QStringLiteral("user"), userMessage));

    m_iteration = 0;
    setBusy(true);
    sendRound();
}

void LlmSession::sendRound()
{
    ++m_iteration;
    m_client->send(m_messages, toolCatalogue());
}

void LlmSession::cancel()
{
    if (!m_busy)
        return;
    if (m_client)
        m_client->cancel();
    note(LogLevel::Info, tr("cancelled by the user"));
    endTurn();
}

void LlmSession::endTurn()
{
    setBusy(false);
    emit finished();
}

void LlmSession::onClientFailed(const QString& error)
{
    note(LogLevel::Warning, tr("endpoint error: %1").arg(error));
    setBusy(false);
    emit failed(error);
}

void LlmSession::onClientFinished(const QJsonObject& assistantMessage)
{
    // The assistant's turn goes into the history verbatim, tool_calls included:
    // the protocol requires that every tool result refer back to a call the
    // history actually contains.
    m_messages.append(assistantMessage);

    const QJsonArray toolCalls = assistantMessage.value(QStringLiteral("tool_calls")).toArray();
    if (toolCalls.isEmpty()) {
        const QString text = assistantMessage.value(QStringLiteral("content")).toString();
        emit this->assistantMessage(text);
        endTurn();
        return;
    }

    if (m_iteration >= m_maxIterations) {
        // Say it in the conversation as well, so a follow-up question sees why the
        // work stopped rather than finding a silently truncated history.
        const QString reason = tr("Stopped after %1 rounds of tool calls without a final answer.")
                                   .arg(m_maxIterations);
        m_messages.append(message(QStringLiteral("system"), reason));
        note(LogLevel::Warning, reason);
        emit this->assistantMessage(reason);
        endTurn();
        return;
    }

    runToolCalls(toolCalls);
    if (m_busy)
        sendRound();
}

void LlmSession::runToolCalls(const QJsonArray& toolCalls)
{
    for (const QJsonValue& value : toolCalls) {
        const QJsonObject call = value.toObject();
        const QString id = call.value(QStringLiteral("id")).toString();
        const QJsonObject function = call.value(QStringLiteral("function")).toObject();
        const QString name = function.value(QStringLiteral("name")).toString();

        // Arguments arrive as a JSON *string*, not an object. A model that emits
        // something unparseable has to be told, not silently given an empty object.
        const QByteArray raw = function.value(QStringLiteral("arguments")).toString().toUtf8();
        QJsonObject args;
        QString argumentError;
        if (!raw.isEmpty()) {
            QJsonParseError parseError {};
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                argumentError = tr("the arguments were not a JSON object: %1")
                                    .arg(parseError.errorString());
            } else {
                args = doc.object();
            }
        }

        const auto reply = [this, &id, &name](bool ok, const QString& payload) {
            QJsonObject toolMessage;
            toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
            toolMessage.insert(QStringLiteral("tool_call_id"), id);
            toolMessage.insert(QStringLiteral("name"), name);
            toolMessage.insert(QStringLiteral("content"), payload);
            m_messages.append(toolMessage);
            Q_UNUSED(ok)
        };

        if (!argumentError.isEmpty()) {
            reply(false, tr("error: %1").arg(argumentError));
            emit toolRefused(name, argumentError);
            continue;
        }

        ToolSpec spec;
        if (!m_registry || !m_registry->tool(name, spec)) {
            const QString error = tr("there is no tool called \"%1\"").arg(name);
            reply(false, tr("error: %1").arg(error));
            emit toolRefused(name, error);
            continue;
        }

        if (needsApproval(spec.effect)) {
            const bool approved = m_approve && m_approve(spec, args);
            if (!approved) {
                const QString reason = m_approve
                    ? tr("the operator declined this call")
                    : tr("%1 tools need approval and no approval policy is set")
                          .arg(toolEffectName(spec.effect));
                reply(false, tr("refused: %1").arg(reason));
                note(LogLevel::Warning, tr("refused %1: %2").arg(name, reason));
                emit toolRefused(name, reason);
                continue;
            }
        }

        emit toolStarted(name, args);
        const ToolResult result = m_dispatcher
            ? m_dispatcher->dispatch(name, args)
            : ToolResult::failure(tr("no dispatcher"));
        emit toolFinished(name, result);

        if (!result.ok) {
            reply(false, tr("error: %1").arg(result.error));
            continue;
        }

        QString payload = result.data.isEmpty()
            ? result.text
            : QString::fromUtf8(QJsonDocument(result.data).toJson(QJsonDocument::Compact));
        if (!result.text.isEmpty() && !result.data.isEmpty())
            payload = result.text + QLatin1Char('\n') + payload;

        // The history is resent in full every round, so one oversized result is
        // paid for again on every subsequent call.
        if (payload.size() > m_maxToolResultChars) {
            payload = payload.left(m_maxToolResultChars)
                + tr("\n… cut after %1 characters; narrow the query or ask for the next page")
                      .arg(m_maxToolResultChars);
        } else if (result.truncated) {
            payload += tr("\n(the tool had more to give; ask for the next page)");
        }
        reply(true, payload);
    }
}
