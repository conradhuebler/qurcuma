// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — LlmConfig and LlmClient against a stub server inside the
// test process. No network is touched: ctest must not depend on an endpoint being
// reachable, and a test that silently passes when the machine is offline is worse
// than no test.

#include "llm/llmclient.h"
#include "llm/llmconfig.h"

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

/// Run one request to completion. Returns true on finished(), false on failed().
static bool roundTrip(LlmClient& client, const QJsonArray& messages, const QJsonArray& tools,
                      QJsonObject& message, QString& error)
{
    QEventLoop loop;
    bool ok = false;
    QObject::connect(&client, &LlmClient::finished, &loop, [&](const QJsonObject& m) {
        message = m;
        ok = true;
        loop.quit();
    });
    QObject::connect(&client, &LlmClient::failed, &loop, [&](const QString& e) {
        error = e;
        ok = false;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, [&loop] { loop.quit(); });  // never hang the suite
    client.send(messages, tools);
    loop.exec();
    return ok;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // --- LlmConfig ----------------------------------------------------------
    {
        LlmConfig config;
        QString error;
        const bool ok = config.loadFromJson(R"JSON({
          "active_profile": "hosted",
          "profiles": [
            {"name":"local","base_url":"http://localhost:11434/v1","model":"qwen"},
            {"name":"hosted","base_url":"https://api.example.com/v1/","model":"gpt","api_key_env":"QURCUMA_TEST_KEY","supports_vision":true}
          ]
        })JSON", &error);
        check(ok, QStringLiteral("a well-formed config loads") + (ok ? QString() : QStringLiteral(" [%1]").arg(error)));
        check(config.profiles().size() == 2, "both profiles are read");

        LlmProfile active;
        check(config.activeProfile(active) && active.name == QLatin1String("hosted"),
            "active_profile picks the named one");
        check(active.supportsVision, "flags come through");
        check(active.chatCompletionsUrl() == QLatin1String("https://api.example.com/v1/chat/completions"),
            "a trailing slash in base_url does not produce a double slash");

        qputenv("QURCUMA_TEST_KEY", "sk-from-environment");
        check(LlmConfig::apiKeyFor(active) == QLatin1String("sk-from-environment"),
            "the key is read from the environment the profile names");
        LlmProfile keyless;
        config.profile(QStringLiteral("local"), keyless);
        check(LlmConfig::apiKeyFor(keyless).isEmpty(),
            "a profile naming no variable yields no key");
    }
    {
        LlmConfig config;
        QString error;
        const bool ok = config.loadFromJson(
            R"JSON({"profiles":[{"name":"x","base_url":"u","model":"m","api_key":"sk-oops"}]})JSON",
            &error);
        check(!ok && error.contains(QStringLiteral("api_key")),
            "a key written into the config file is refused outright, not warned about");
    }
    {
        LlmConfig config;
        QString error;
        check(!config.loadFromJson(R"JSON({"profiles":[{"name":"x"}]})JSON", &error)
                && error.contains(QStringLiteral("base_url")),
            "an incomplete profile is refused and says what is missing");
        check(!config.loadFromJson("not json at all", &error) && error.contains(QStringLiteral("JSON")),
            "a malformed file is refused with a readable reason");
    }

    // --- LlmClient ----------------------------------------------------------
    StubServer server;
    check(server.isListening(), "the stub server is listening");

    LlmProfile profile;
    profile.name = QStringLiteral("stub");
    profile.baseUrl = server.baseUrl();
    profile.model = QStringLiteral("test-model");
    // The block below exercises the non-streaming path deliberately; streaming has
    // its own section further down with its own canned bodies.
    profile.stream = false;

    const QJsonArray messages { QJsonObject { { "role", "user" }, { "content", "hello" } } };
    const QJsonArray tools { QJsonObject {
        { "type", "function" },
        { "function", QJsonObject { { "name", "get_structure_summary" } } } } };

    // Request shape, and no Authorization header without a key.
    {
        LlmClient client;
        client.setProfile(profile);
        server.reply(200, R"JSON({"choices":[{"message":{"role":"assistant","content":"hi"}}]})JSON");

        QJsonObject message;
        QString error;
        const bool ok = roundTrip(client, messages, tools, message, error);
        check(ok, QStringLiteral("a plain answer arrives") + (ok ? QString() : QStringLiteral(" [%1]").arg(error)));
        check(message.value(QStringLiteral("content")).toString() == QLatin1String("hi"),
            "and its content is handed on");

        const QJsonObject sent = QJsonDocument::fromJson(server.requestBody).object();
        check(sent.value(QStringLiteral("model")).toString() == QLatin1String("test-model"),
            "the request names the profile's model");
        check(sent.value(QStringLiteral("messages")).toArray().size() == 1, "and carries the messages");
        check(sent.value(QStringLiteral("tools")).toArray().size() == 1,
            "and the tool catalogue -- it travels with every request, there is no discovery");
        check(sent.value(QStringLiteral("tool_choice")).toString() == QLatin1String("auto"),
            "tool_choice is set when tools are sent");
        check(!server.requestHeaders.toLower().contains("authorization"),
            "no Authorization header without a key -- a bare Bearer breaks local endpoints");
    }

    // With a key.
    {
        LlmClient client;
        client.setProfile(profile);
        client.setApiKey(QStringLiteral("sk-secret"));
        server.reply(200, R"JSON({"choices":[{"message":{"role":"assistant","content":"ok"}}]})JSON");
        QJsonObject message;
        QString error;
        roundTrip(client, messages, {}, message, error);
        check(server.requestHeaders.contains("Authorization: Bearer sk-secret"),
            "the key goes into the Authorization header when there is one");
        const QJsonObject sent = QJsonDocument::fromJson(server.requestBody).object();
        check(!sent.contains(QStringLiteral("tools")),
            "no tools field when there are no tools");
    }

    // A tool call comes back.
    {
        LlmClient client;
        client.setProfile(profile);
        server.reply(200, R"JSON({"choices":[{"message":{"role":"assistant","content":null,
          "tool_calls":[{"id":"call_1","type":"function",
                         "function":{"name":"measure","arguments":"{\"kind\":\"distance\",\"atoms\":[0,1]}"}}]}}]})JSON");
        QJsonObject message;
        QString error;
        const bool ok = roundTrip(client, messages, tools, message, error);
        check(ok, "a tool call arrives as a normal answer");
        const QJsonArray calls = message.value(QStringLiteral("tool_calls")).toArray();
        check(calls.size() == 1, "with one call in it");
        check(calls.first().toObject().value(QStringLiteral("function")).toObject()
                  .value(QStringLiteral("name")).toString() == QLatin1String("measure"),
            "naming the tool the model wants run");
    }

    // Failures.
    {
        LlmClient client;
        client.setProfile(profile);
        server.reply(401, R"JSON({"error":{"message":"Incorrect API key provided"}})JSON");
        QJsonObject message;
        QString error;
        check(!roundTrip(client, messages, {}, message, error)
                && error.contains(QStringLiteral("Incorrect API key")),
            "an endpoint's own error message is preferred over the HTTP status");
    }
    {
        LlmClient client;
        client.setProfile(profile);
        server.reply(502, "<html><body>Bad Gateway</body></html>", "text/html");
        QJsonObject message;
        QString error;
        check(!roundTrip(client, messages, {}, message, error)
                && error.contains(QStringLiteral("502")) && error.contains(QStringLiteral("html")),
            "a proxy's HTML error page is reported with a slice of what came back");
    }
    {
        LlmClient client;
        client.setProfile(profile);
        server.reply(200, R"JSON({"choices":[]})JSON");
        QJsonObject message;
        QString error;
        check(!roundTrip(client, messages, {}, message, error) && error.contains(QStringLiteral("choices")),
            "an answer without choices is a failure, not an empty success");
    }
    {
        LlmClient client;  // no profile at all
        QJsonObject message;
        QString error;
        check(!roundTrip(client, messages, {}, message, error) && error.contains(QStringLiteral("profile")),
            "sending without a configured profile fails immediately and says so");
    }

    // --- model discovery ----------------------------------------------------
    // Shapes taken from a real Ollama (09.09.2026): /v1/models is the plain OpenAI
    // listing, and /api/show carries the capability list plus a context length
    // under an architecture-prefixed key.
    {
        LlmClient client;
        client.setProfile(profile);
        server.reply(200, R"JSON({"object":"list","data":[
            {"id":"zeta:latest"},{"id":"alpha:cloud"},{"id":"mid:9b"}]})JSON");

        QEventLoop loop;
        QStringList got;
        QObject::connect(&client, &LlmClient::modelsListed, &loop, [&](const QStringList& m) {
            got = m;
            loop.quit();
        });
        QTimer::singleShot(5000, &loop, [&loop] { loop.quit(); });
        client.listModels();
        loop.exec();

        check(got.size() == 3, "the model listing is read");
        check(got == QStringList({ "alpha:cloud", "mid:9b", "zeta:latest" }),
            "and sorted, so the box does not reshuffle between runs");
    }
    {
        LlmClient client;
        client.setProfile(profile);
        server.reply(200, R"JSON({"capabilities":["completion","thinking","tools","vision"],
            "model_info":{"glm5_next.context_length":1048576,"general.architecture":"glm5_next"}})JSON");

        QEventLoop loop;
        LlmModelInfo info;
        QObject::connect(&client, &LlmClient::modelDescribed, &loop, [&](const LlmModelInfo& i) {
            info = i;
            loop.quit();
        });
        QTimer::singleShot(5000, &loop, [&loop] { loop.quit(); });
        client.describeModel(QStringLiteral("glm-5.3-flash:cloud"));
        loop.exec();

        check(info.detailsKnown, "the endpoint's model details are read");
        check(info.supportsTools, "and say whether it can call tools at all");
        check(info.supportsVision, "and whether it can see images");
        check(info.contextLength == 1048576,
            "the context length is found under its architecture-prefixed key");
    }
    {
        // An endpoint that is not Ollama answers this with an error page. That is
        // not a failure worth reporting -- the extra detail is a bonus.
        LlmClient client;
        client.setProfile(profile);
        server.reply(404, "<html>not found</html>", "text/html");

        QEventLoop loop;
        LlmModelInfo info;
        info.detailsKnown = true;  // must be overwritten
        QObject::connect(&client, &LlmClient::modelDescribed, &loop, [&](const LlmModelInfo& i) {
            info = i;
            loop.quit();
        });
        QTimer::singleShot(5000, &loop, [&loop] { loop.quit(); });
        client.describeModel(QStringLiteral("whatever"));
        loop.exec();
        check(!info.detailsKnown,
            "an endpoint without that facility yields \"unknown\" rather than a guess");
    }

    // --- which model actually gets sent -------------------------------------
    {
        LlmProfile withModel = profile;
        withModel.model = QStringLiteral("from-profile");
        LlmClient client;
        client.setProfile(withModel);
        check(client.effectiveModel() == QLatin1String("from-profile"),
            "the profile's model is used when nothing overrides it");
        client.setModel(QStringLiteral("from-ui"));
        check(client.effectiveModel() == QLatin1String("from-ui"),
            "and the session's choice wins over it");
    }
    {
        LlmProfile modelless = profile;
        modelless.model.clear();
        check(modelless.isValid(),
            "a profile without a model is still valid -- the model is picked from the endpoint");
        LlmClient client;
        client.setProfile(modelless);
        QJsonObject message;
        QString error;
        check(!roundTrip(client, messages, {}, message, error)
                && error.contains(QStringLiteral("no model")),
            "but sending without any model fails with a reason, not an empty request");
    }

    // --- streaming -----------------------------------------------------------
    // Shapes measured against Ollama (glm-5.3-flash:cloud, 09.09.2026): lines of
    // "data: {json}" closed by "data: [DONE]", with reasoning in delta.reasoning.
    LlmProfile streamed = profile;
    streamed.stream = true;

    {
        LlmClient client;
        client.setProfile(streamed);
        server.reply(200,
            "data: {\"choices\":[{\"delta\":{\"role\":\"assistant\",\"content\":\"Hel\"}}]}\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"lo \"}}]}\n"
            "\n"
            ": a comment line that must be ignored\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"world\"}}]}\n"
            "data: [DONE]\n",
            "text/event-stream");

        QStringList chunks;
        QObject::connect(&client, &LlmClient::contentChunk, &client,
            [&chunks](const QString& c) { chunks << c; });
        QJsonObject message;
        QString error;
        const bool ok = roundTrip(client, messages, {}, message, error);
        check(ok, QStringLiteral("a streamed answer completes") + (ok ? QString() : QStringLiteral(" [%1]").arg(error)));
        check(chunks.size() == 3, "each piece arrives as its own chunk while it streams");
        check(message.value(QStringLiteral("content")).toString() == QLatin1String("Hello world"),
            "and the finished message carries the whole thing");
    }

    // Reasoning is shown but must not travel back into the next request.
    {
        LlmClient client;
        client.setProfile(streamed);
        server.reply(200,
            "data: {\"choices\":[{\"delta\":{\"reasoning\":\"17\"}}]}\n"
            "data: {\"choices\":[{\"delta\":{\"reasoning\":\"*3\"}}]}\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"51\"}}]}\n"
            "data: [DONE]\n",
            "text/event-stream");

        QString thinking;
        QObject::connect(&client, &LlmClient::reasoningChunk, &client,
            [&thinking](const QString& c) { thinking += c; });
        QJsonObject message;
        QString error;
        roundTrip(client, messages, {}, message, error);
        check(thinking == QLatin1String("17*3"), "reasoning arrives in its own signal");
        check(message.value(QStringLiteral("content")).toString() == QLatin1String("51"),
            "the answer is kept apart from it");
        check(!message.contains(QStringLiteral("reasoning")),
            "and the reasoning is NOT in the message -- the history is resent every "
            "round and nothing downstream needs the scratch work");
    }

    // OpenAI splits function.arguments across chunks; only the index ties them.
    {
        LlmClient client;
        client.setProfile(streamed);
        server.reply(200,
            "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_9\",\"type\":\"function\","
            "\"function\":{\"name\":\"measure\",\"arguments\":\"{\\\"kind\\\":\"}}]}}]}\n"
            "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,"
            "\"function\":{\"arguments\":\"\\\"distance\\\"}\"}}]}}]}\n"
            "data: [DONE]\n",
            "text/event-stream");

        QJsonObject message;
        QString error;
        const bool ok = roundTrip(client, messages, tools, message, error);
        check(ok, "a fragmented tool call completes");
        const QJsonArray calls = message.value(QStringLiteral("tool_calls")).toArray();
        check(calls.size() == 1, "and assembles into one call");
        const QJsonObject fn = calls.first().toObject().value(QStringLiteral("function")).toObject();
        check(fn.value(QStringLiteral("name")).toString() == QLatin1String("measure"),
            "keeping the name that came in the first piece");
        check(fn.value(QStringLiteral("arguments")).toString()
                  == QLatin1String("{\"kind\":\"distance\"}"),
            "with the arguments joined across chunks, not overwritten");
        check(calls.first().toObject().value(QStringLiteral("id")).toString()
                  == QLatin1String("call_9"),
            "and the id from whichever piece carried it");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
