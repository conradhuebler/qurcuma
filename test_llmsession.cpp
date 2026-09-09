// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — the agent loop, driven through the real LlmClient
// against a stub endpoint. Deliberately not against a fake client: the question
// worth answering is whether client and session together do the right thing --
// whether a tool_calls block really reaches the dispatcher and the result really
// goes back in the shape the protocol wants. A fake would skip that seam.

#include "core/loghub.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "llm/llmclient.h"
#include "llm/llmsession.h"
#include "test_stubserver.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QTimer>
#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static QJsonObject obj(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

/// An assistant answer that asks for one tool call.
static QByteArray callResponse(const char* name, const char* argsJson)
{
    return QByteArray(R"({"choices":[{"message":{"role":"assistant","content":null,
      "tool_calls":[{"id":"call_1","type":"function","function":{"name":")")
        + name + R"(","arguments":")" + QByteArray(argsJson).replace("\"", "\\\"")
        + R"("}}]}}]})";
}

static QByteArray textResponse(const char* text)
{
    return QByteArray(R"({"choices":[{"message":{"role":"assistant","content":")") + text + R"("}}]})";
}

/// Run one turn to completion.
static void runTurn(LlmSession& session, const QString& question, QString& answer, QString& error)
{
    QEventLoop loop;
    answer.clear();
    error.clear();
    QObject::connect(&session, &LlmSession::assistantMessage, &loop,
        [&answer](const QString& text) { answer = text; });
    QObject::connect(&session, &LlmSession::finished, &loop, [&loop] { loop.quit(); });
    QObject::connect(&session, &LlmSession::failed, &loop, [&error, &loop](const QString& e) {
        error = e;
        loop.quit();
    });
    QTimer::singleShot(8000, &loop, [&loop] { loop.quit(); });
    session.ask(question);
    loop.exec();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    LogHub hub(1000);
    ToolRegistry registry;
    ToolDispatcher dispatcher(&registry, &hub);

    int readCalls = 0;
    int computeCalls = 0;
    {
        ToolSpec spec;
        spec.name = QStringLiteral("get_answer");
        spec.description = QStringLiteral("Return a number");
        spec.category = QStringLiteral("test");
        spec.effect = ToolEffect::Read;
        spec.paramSchema = obj(R"({"type":"object","properties":{"n":{"type":"integer"}}})");
        spec.handler = [&readCalls](const QJsonObject& args) {
            ++readCalls;
            return ToolResult::success(QJsonObject { { "answer", args.value("n").toInt(7) } });
        };
        registry.add(spec);
    }
    {
        ToolSpec spec;
        spec.name = QStringLiteral("start_run");
        spec.description = QStringLiteral("Start a calculation");
        spec.category = QStringLiteral("test");
        spec.effect = ToolEffect::Compute;  // needs approval
        spec.paramSchema = obj(R"({"type":"object"})");
        spec.handler = [&computeCalls](const QJsonObject&) {
            ++computeCalls;
            return ToolResult::success(QJsonObject { { "started", true } });
        };
        registry.add(spec);
    }

    StubServer server;
    LlmProfile profile;
    profile.name = QStringLiteral("stub");
    profile.baseUrl = server.baseUrl();
    profile.model = QStringLiteral("test-model");

    LlmClient client;
    client.setProfile(profile);
    LlmSession session(&client, &registry, &dispatcher, &hub);
    session.setSystemPrompt(QStringLiteral("You are a test."));

    QString answer;
    QString error;

    // --- a plain answer, no tools ------------------------------------------
    {
        server.reply(200, textResponse("just words"));
        runTurn(session, QStringLiteral("hello"), answer, error);
        check(answer == QLatin1String("just words"), "a plain answer comes through");
        check(readCalls == 0, "and runs no tools");

        const QJsonObject sent = QJsonDocument::fromJson(server.requests.first()).object();
        const QJsonArray tools = sent.value(QStringLiteral("tools")).toArray();
        check(tools.size() == 2, "the catalogue carries both registered tools");
        const QJsonObject first = tools.first().toObject().value(QStringLiteral("function")).toObject();
        check(first.contains(QStringLiteral("parameters")),
            "each entry carries its parameter schema -- that is what the model plans against");
        check(sent.value(QStringLiteral("messages")).toArray().size() == 2,
            "the system prompt precedes the question");
    }

    // --- a Read tool: runs unattended, result goes back ---------------------
    {
        session.reset();
        server.requests.clear();
        server.enqueue(callResponse("get_answer", R"({"n":42})"));
        server.enqueue(textResponse("the answer is 42"));
        runTurn(session, QStringLiteral("what is it"), answer, error);

        check(readCalls == 1, "a Read tool runs without being approved");
        check(answer == QLatin1String("the answer is 42"), "and the second round gives the answer");
        check(server.requests.size() == 2, "which took exactly two requests");

        const QJsonArray second = QJsonDocument::fromJson(server.requests.at(1))
                                      .object().value(QStringLiteral("messages")).toArray();
        bool sawToolMessage = false;
        bool sawAssistantCall = false;
        for (const QJsonValue& v : second) {
            const QJsonObject m = v.toObject();
            if (m.value(QStringLiteral("role")).toString() == QLatin1String("tool")) {
                sawToolMessage = true;
                check(m.value(QStringLiteral("tool_call_id")).toString() == QLatin1String("call_1"),
                    "the tool message refers back to the call id");
                check(m.value(QStringLiteral("content")).toString().contains(QStringLiteral("42")),
                    "and carries the tool's data");
            }
            if (m.contains(QStringLiteral("tool_calls")))
                sawAssistantCall = true;
        }
        check(sawToolMessage, "the result is sent back as a tool message");
        check(sawAssistantCall,
            "and the assistant's own call stays in the history -- the protocol needs the pair");
    }

    // --- a Compute tool without an approval policy: refused -----------------
    {
        session.reset();
        server.requests.clear();
        server.enqueue(callResponse("start_run", "{}"));
        server.enqueue(textResponse("understood, I will not"));
        runTurn(session, QStringLiteral("run it"), answer, error);

        check(computeCalls == 0,
            "with no approval policy set, a Compute tool is refused -- the safe default is the "
            "one nobody has to remember to switch on");
        const QJsonArray second = QJsonDocument::fromJson(server.requests.at(1))
                                      .object().value(QStringLiteral("messages")).toArray();
        bool told = false;
        for (const QJsonValue& v : second) {
            const QJsonObject m = v.toObject();
            if (m.value(QStringLiteral("role")).toString() == QLatin1String("tool")
                && m.value(QStringLiteral("content")).toString().contains(QStringLiteral("refused"))) {
                told = true;
            }
        }
        check(told, "and the model is told it was refused, so it can carry on differently");
    }

    // --- with a policy that says yes ----------------------------------------
    {
        QString approvedTool;
        session.setApprovalPolicy([&approvedTool](const ToolSpec& spec, const QJsonObject&) {
            approvedTool = spec.name;
            return true;
        });
        session.reset();
        server.requests.clear();
        server.enqueue(callResponse("start_run", "{}"));
        server.enqueue(textResponse("started"));
        runTurn(session, QStringLiteral("run it"), answer, error);

        check(computeCalls == 1, "an approved Compute tool runs");
        check(approvedTool == QLatin1String("start_run"), "and the policy saw which tool it was");
    }

    // --- a policy that says no ----------------------------------------------
    {
        session.setApprovalPolicy([](const ToolSpec&, const QJsonObject&) { return false; });
        session.reset();
        server.requests.clear();
        const int before = computeCalls;
        server.enqueue(callResponse("start_run", "{}"));
        server.enqueue(textResponse("fine"));
        runTurn(session, QStringLiteral("run it"), answer, error);
        check(computeCalls == before, "a declined call does not reach the handler");
    }
    session.setApprovalPolicy(nullptr);

    // --- a tool the model invented ------------------------------------------
    {
        session.reset();
        server.requests.clear();
        server.enqueue(callResponse("no_such_tool", "{}"));
        server.enqueue(textResponse("my mistake"));
        runTurn(session, QStringLiteral("go"), answer, error);
        const QJsonArray second = QJsonDocument::fromJson(server.requests.at(1))
                                      .object().value(QStringLiteral("messages")).toArray();
        bool told = false;
        for (const QJsonValue& v : second) {
            if (v.toObject().value(QStringLiteral("content")).toString()
                    .contains(QStringLiteral("no tool called"))) {
                told = true;
            }
        }
        check(told, "an invented tool name is answered with an error, not silence");
    }

    // --- arguments that are not JSON ----------------------------------------
    {
        session.reset();
        server.requests.clear();
        server.enqueue(R"({"choices":[{"message":{"role":"assistant","content":null,
          "tool_calls":[{"id":"call_1","type":"function",
                         "function":{"name":"get_answer","arguments":"{not json"}}]}}]})");
        server.enqueue(textResponse("sorry"));
        const int before = readCalls;
        runTurn(session, QStringLiteral("go"), answer, error);
        check(readCalls == before, "unparseable arguments never reach the handler");
        const QJsonArray second = QJsonDocument::fromJson(server.requests.at(1))
                                      .object().value(QStringLiteral("messages")).toArray();
        bool told = false;
        for (const QJsonValue& v : second) {
            if (v.toObject().value(QStringLiteral("content")).toString()
                    .contains(QStringLiteral("not a JSON object"))) {
                told = true;
            }
        }
        check(told, "and the model is told what was wrong with them");
    }

    // --- the shapes a real endpoint actually sends ---------------------------
    // Observed against Ollama (glm-5.3-flash:cloud, 09.09.2026): alongside
    // tool_calls the content field is an empty STRING, not null, and arguments is
    // a JSON string. Both are pinned here so a future refactor cannot quietly
    // start treating "" as an answer and end the turn without running anything.
    {
        session.reset();
        server.requests.clear();
        server.enqueue(R"({"choices":[{"message":{"role":"assistant","content":"",
          "tool_calls":[{"id":"call_t65agluy","type":"function",
                         "function":{"name":"get_answer","arguments":"{}"}}]}}],
          "finish_reason":"tool_calls"})");
        server.enqueue(textResponse("42 atoms"));
        const int before = readCalls;
        runTurn(session, QStringLiteral("how many"), answer, error);
        check(readCalls == before + 1,
            "an empty-string content next to tool_calls still runs the tool");
        check(answer == QLatin1String("42 atoms"), "and the turn reaches a real answer");
    }

    // --- a loop that will not converge --------------------------------------
    {
        session.reset();
        session.setMaxIterations(3);
        server.requests.clear();
        for (int i = 0; i < 10; ++i)
            server.enqueue(callResponse("get_answer", R"({"n":1})"));
        runTurn(session, QStringLiteral("loop forever"), answer, error);
        check(server.requests.size() <= 3,
            QStringLiteral("the iteration cap stops the loop (%1 requests)").arg(server.requests.size()));
        check(answer.contains(QStringLiteral("Stopped after")),
            "and says so instead of ending quietly");
    }

    // --- transport failure ---------------------------------------------------
    {
        session.reset();
        session.setMaxIterations(12);
        server.reply(500, "<html>nope</html>", "text/html");
        runTurn(session, QStringLiteral("go"), answer, error);
        check(!error.isEmpty(), "an endpoint failure surfaces as failed(), not as an empty answer");
    }

    // --- the audit trail saw it all -----------------------------------------
    {
        LogQuery q;
        q.source = QStringLiteral("tool");
        q.limit = LogHub::kMaxQueryLimit;
        check(hub.query(q).size() >= 3,
            "every dispatched tool left an audit record behind");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
