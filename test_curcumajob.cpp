// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — a real curcuma calculation, in process.
//
// This is where the four things a child process would have given for free are
// checked together: the output is captured rather than lost to a terminal, the
// per-run state does not leak, the result comes back without a file, and nothing
// is left behind in the working directory.

#include "core/loghub.h"
#include "llm/curcumajob.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

/// Water, roughly.
static QVector<moldata::Atom> water(float dx = 0.0f)
{
    QVector<moldata::Atom> atoms;
    moldata::Atom o;
    o.element = QStringLiteral("O");
    o.position = QVector3D(0.0f + dx, 0.0f, 0.0f);
    moldata::Atom h1;
    h1.element = QStringLiteral("H");
    h1.position = QVector3D(0.758f + dx, 0.587f, 0.0f);
    moldata::Atom h2;
    h2.element = QStringLiteral("H");
    h2.position = QVector3D(-0.758f + dx, 0.587f, 0.0f);
    return { o, h1, h2 };
}

/// Run one job to completion.
static CurcumaJobResult runJob(CurcumaJob& job, const CurcumaJobRequest& request, QString& error)
{
    CurcumaJobResult out;
    QEventLoop loop;
    QObject::connect(&job, &CurcumaJob::finished, &loop, [&](const CurcumaJobResult& r) {
        out = r;
        loop.quit();
    });
    QTimer::singleShot(120000, &loop, [&loop] { loop.quit(); });
    const QString id = job.start(request, &error);
    if (id.isEmpty())
        return out;
    loop.exec();
    return out;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const QString workDir = QDir::currentPath();
    const QStringList before = QDir(workDir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    LogHub hub(2000);
    CurcumaJob job(&hub);

    check(!job.isRunning(), "nothing is running to begin with");
    check(CurcumaJob::supportedCommands().contains(QStringLiteral("sp")),
        "single point is offered in process");

    // --- an unknown command is refused by name ------------------------------
    {
        CurcumaJobRequest request;
        request.command = QStringLiteral("confscan");
        request.atoms = water();
        QString error;
        const QString id = job.start(request, &error);
        check(id.isEmpty() && error.contains(QStringLiteral("cannot run in process")),
            "an unsupported command is refused up front, not attempted and failed halfway");
    }

    // --- a real single point ------------------------------------------------
    {
        CurcumaJobRequest request;
        request.command = QStringLiteral("sp");
        request.atoms = water();
        request.controller.insert(QStringLiteral("method"), QStringLiteral("uff"));

        QString error;
        const CurcumaJobResult result = runJob(job, request, error);
        check(result.ok, QStringLiteral("a single point runs") + (result.ok ? QString() : QStringLiteral(" [%1]").arg(result.error)));
        check(result.results.contains(QStringLiteral("energy")),
            "and hands the energy back directly -- no file to read");
        check(result.results.value(QStringLiteral("unit")).toString() == QLatin1String("Eh"),
            "with its unit stated");
        check(!result.jobId.isEmpty(), "the run has an id");
        check(result.elapsedMs >= 0, "and a duration");
    }

    // --- the output was captured, and tagged with the run -------------------
    {
        LogQuery q;
        q.source = QStringLiteral("curcuma");
        q.limit = LogHub::kMaxQueryLimit;
        const QVector<LogRecord> records = hub.query(q);
        check(!records.isEmpty(),
            QStringLiteral("curcuma's own output reached the hub (%1 lines) instead of a terminal")
                .arg(records.size()));
        bool tagged = false;
        for (const LogRecord& r : records) {
            if (!r.jobId.isEmpty())
                tagged = true;
        }
        check(tagged, "and carries the job id, so two runs stay apart");
    }

    // --- a real RMSD, through the driver's own Results() --------------------
    {
        CurcumaJobRequest request;
        request.command = QStringLiteral("rmsd");
        request.atoms = water();
        request.secondAtoms = water(1.5f);   // same molecule, displaced

        QString error;
        const CurcumaJobResult result = runJob(job, request, error);
        check(result.ok, QStringLiteral("an RMSD runs") + (result.ok ? QString() : QStringLiteral(" [%1]").arg(result.error)));
        check(result.results.contains(QStringLiteral("rmsd")), "and returns the value");
        check(result.results.value(QStringLiteral("rmsd")).toDouble() < 1e-3,
            "which is ~0 for a rigid displacement of the same molecule");
        check(result.results.contains(QStringLiteral("permutation")),
            "the whole document comes from the driver, permutation included");
    }

    // --- one at a time ------------------------------------------------------
    {
        CurcumaJobRequest request;
        request.command = QStringLiteral("sp");
        request.atoms = water();
        request.controller.insert(QStringLiteral("method"), QStringLiteral("uff"));

        QString firstError;
        const QString first = job.start(request, &firstError);
        QString secondError;
        const QString second = job.start(request, &secondError);
        check(!first.isEmpty(), "the first job starts");
        check(second.isEmpty() && secondError.contains(QStringLiteral("already running")),
            "and a second is refused -- curcuma_core is OpenMP-parallel, two jobs "
            "would only take cores from each other");

        QEventLoop loop;
        QObject::connect(&job, &CurcumaJob::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(120000, &loop, [&loop] { loop.quit(); });
        loop.exec();
    }

    // --- nothing was left behind --------------------------------------------
    {
        const QStringList after = QDir(workDir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        QStringList added;
        for (const QString& entry : after) {
            if (!before.contains(entry))
                added << entry;
        }
        check(added.isEmpty(),
            QStringLiteral("the working directory is untouched -- no BMT folder, no stray files%1")
                .arg(added.isEmpty() ? QString() : QStringLiteral(" [%1]").arg(added.join(", "))));
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
