// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — CurcumaLogger's output capture.
//
// The property that matters is thread-locality. Two CurcumaMethods run at once in
// qurcuma -- the MD worker and anything started from the chat dock -- and a
// process-wide sink would hand each of them the other's output, which is worse
// than no capture at all because it looks like it worked.

#include <src/core/curcuma_logger.h>

#include <QCoreApplication>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static int g_failed = 0;

static void check(bool ok, const std::string& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failed;
}

using Level = CurcumaLogger::SinkLevel;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    CurcumaLogger::set_verbosity(3);   // so info/warn actually reach the helpers

    check(CurcumaLogger::active_sink_count() == 0, "no sink is installed to begin with");

    // --- a scope captures, and the level survives ---------------------------
    {
        std::vector<std::pair<Level, std::string>> got;
        CurcumaLogger::SinkScope scope([&got](Level level, const std::string& text) {
            got.emplace_back(level, text);
        });
        check(CurcumaLogger::active_sink_count() == 1, "the scope installs one sink");
        check(scope.id() != 0, "and identifies itself, so records can name their run");

        CurcumaLogger::error("something broke");
        CurcumaLogger::warn("something is odd");

        bool sawError = false;
        bool sawWarning = false;
        for (const auto& entry : got) {
            if (entry.first == Level::Error && entry.second == "something broke")
                sawError = true;
            if (entry.first == Level::Warning && entry.second == "something is odd")
                sawWarning = true;
        }
        check(sawError, "an error reaches the sink at Error");
        check(sawWarning, "a warning reaches it at Warning");
        check(!got.empty() && got.front().second.find("[ERROR]") == std::string::npos,
            "the sink gets the bare message -- prefix and colour are the terminal's business");
    }
    check(CurcumaLogger::active_sink_count() == 0, "and the scope removes itself again");

    // --- nothing is captured outside a scope --------------------------------
    {
        CurcumaLogger::error("this one has no listener");
        check(CurcumaLogger::active_sink_count() == 0, "logging without a sink is harmless");
    }

    // --- nesting: both scopes see it ----------------------------------------
    {
        int outer = 0;
        int inner = 0;
        CurcumaLogger::SinkScope outerScope([&outer](Level, const std::string&) { ++outer; });
        {
            CurcumaLogger::SinkScope innerScope([&inner](Level, const std::string&) { ++inner; });
            check(CurcumaLogger::active_sink_count() == 2, "scopes stack");
            CurcumaLogger::error("nested");
        }
        check(inner == 1, "the inner scope saw it");
        check(outer == 1,
            "and so did the outer one -- a capture of a whole job keeps working when "
            "something inside installs its own");
        CurcumaLogger::error("after the inner one ended");
        check(outer == 2 && inner == 1, "once the inner scope is gone only the outer remains");
    }

    // --- THE point: a sink does not see another thread's output -------------
    {
        std::atomic<int> mine { 0 };
        std::atomic<int> theirs { 0 };

        CurcumaLogger::SinkScope scope([&mine](Level, const std::string&) { ++mine; });

        std::thread other([&theirs] {
            // This thread has no sink of its own; its output must reach nobody.
            CurcumaLogger::error("from the other thread");
            {
                CurcumaLogger::SinkScope ownScope([&theirs](Level, const std::string&) { ++theirs; });
                CurcumaLogger::error("captured only over there");
            }
        });
        other.join();

        CurcumaLogger::error("from the main thread");
        check(mine.load() == 1,
            "a sink sees only what its own thread logged, not the other thread's");
        check(theirs.load() == 1, "and the other thread's scope saw only its own");
    }

    // --- a sink that logs must not recurse ----------------------------------
    {
        int calls = 0;
        CurcumaLogger::SinkScope scope([&calls](Level, const std::string&) {
            ++calls;
            if (calls < 100)
                CurcumaLogger::error("a sink that logs");   // would recurse without the guard
        });
        CurcumaLogger::error("start");
        check(calls == 1, "re-entering the sink is refused rather than overflowing the stack");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
