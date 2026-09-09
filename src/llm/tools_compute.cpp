// tools_compute.cpp - Tools that let the model have something calculated.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_compute.h"

#include "core/toolregistry.h"
#include "curcumajob.h"
#include "curcuma_schemas.h"
#include "view.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

/// Finished jobs, so job_status can answer after the fact. A calculation is
/// asynchronous by necessity, and a model that asked for one has to be able to
/// come back for the answer.
struct JobStore {
    QHash<QString, CurcumaJobResult> finished;
    QStringList order;
};

QJsonObject schemaFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonObject resultToJson(const CurcumaJobResult& result)
{
    QJsonObject o;
    o.insert(QStringLiteral("job_id"), result.jobId);
    o.insert(QStringLiteral("command"), result.command);
    o.insert(QStringLiteral("status"), result.ok ? QStringLiteral("finished")
                                                 : QStringLiteral("failed"));
    o.insert(QStringLiteral("elapsed_ms"), static_cast<double>(result.elapsedMs));
    if (result.ok)
        o.insert(QStringLiteral("results"), result.results);
    else
        o.insert(QStringLiteral("error"), result.error);
    return o;
}

}  // namespace

int registerComputeTools(ToolRegistry& registry, const ComputeToolContext& context)
{
    MoleculeViewer* const viewer = context.viewer;
    CurcumaJob* const job = context.job;
    if (!viewer || !job)
        return 0;

    auto store = std::make_shared<JobStore>();
    QObject::connect(job, &CurcumaJob::finished, job, [store](const CurcumaJobResult& result) {
        store->finished.insert(result.jobId, result);
        store->order.append(result.jobId);
    });

    int added = 0;
    const auto add = [&registry, &added](const ToolSpec& spec) {
        if (registry.add(spec))
            ++added;
    };

    // --- run_single_point ---------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("run_single_point");
        spec.category = QStringLiteral("compute");
        spec.description = QStringLiteral(
            "Calculate the energy of the structure currently loaded, with curcuma, in process. "
            "Returns immediately with a job_id; ask job_status for the answer. The parameters "
            "come from curcuma's own registry -- describe_job(\"sp\") lists all of them.");
        spec.effect = ToolEffect::Compute;   // asks before it runs
        spec.affinity = ToolAffinity::Gui;   // reads the viewer's atoms
        spec.paramSchema = curcumaJobSchema(QStringLiteral("sp"));

        spec.handler = [viewer, job](const QJsonObject& args) {
            const QVector<moldata::Atom> atoms = viewer->getCurrentFrameAtoms();
            if (atoms.isEmpty())
                return ToolResult::failure(QStringLiteral("no structure is loaded"));

            CurcumaJobRequest request;
            request.command = QStringLiteral("sp");
            request.atoms = atoms;
            request.controller = args;

            QString error;
            const QString jobId = job->start(request, &error);
            if (jobId.isEmpty())
                return ToolResult::failure(error);

            QJsonObject data;
            data.insert(QStringLiteral("status"), QStringLiteral("started"));
            data.insert(QStringLiteral("job_id"), jobId);
            data.insert(QStringLiteral("atom_count"), atoms.size());
            return ToolResult::success(data,
                QStringLiteral("Started; ask job_status with job_id \"%1\".").arg(jobId));
        };
        add(spec);
    }

    // --- job_status ---------------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("job_status");
        spec.category = QStringLiteral("compute");
        spec.description = QStringLiteral(
            "Where a calculation stands. Without a job_id it reports what is running now. "
            "A finished job keeps its result, so you can come back for it.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;
        spec.paramSchema = schemaFromJson(R"JSON({
          "type": "object",
          "properties": {
            "job_id": { "type": "string", "description": "the job to ask about" }
          }
        })JSON");

        spec.handler = [job, store](const QJsonObject& args) {
            const QString wanted = args.value(QStringLiteral("job_id")).toString();
            if (!wanted.isEmpty()) {
                if (store->finished.contains(wanted))
                    return ToolResult::success(resultToJson(store->finished.value(wanted)));
                if (job->runningJobId() == wanted) {
                    QJsonObject data;
                    data.insert(QStringLiteral("job_id"), wanted);
                    data.insert(QStringLiteral("status"), QStringLiteral("running"));
                    return ToolResult::success(data, QStringLiteral("Still running."));
                }
                return ToolResult::failure(QStringLiteral("no job called \"%1\"").arg(wanted));
            }

            QJsonObject data;
            data.insert(QStringLiteral("running"), job->isRunning());
            if (job->isRunning())
                data.insert(QStringLiteral("job_id"), job->runningJobId());
            QJsonArray done;
            for (const QString& id : store->order)
                done.append(id);
            data.insert(QStringLiteral("finished_jobs"), done);
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- describe_job -------------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("describe_job");
        spec.category = QStringLiteral("compute");
        spec.description = QStringLiteral(
            "Everything curcuma knows about a command: which modules configure it and every "
            "parameter with its type, default, unit and permitted values. The schema of "
            "run_single_point shows only the handful marked primary; this is the long tail.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;
        spec.paramSchema = schemaFromJson(R"JSON({
          "type": "object",
          "properties": {
            "command": { "type": "string", "description": "e.g. sp, opt, md, rmsd, analysis" }
          },
          "required": ["command"]
        })JSON");

        spec.handler = [](const QJsonObject& args) {
            const QString command = args.value(QStringLiteral("command")).toString();
            const QJsonObject details = curcumaJobDetails(command);
            if (details.value(QStringLiteral("modules")).toArray().isEmpty()) {
                return ToolResult::failure(
                    QStringLiteral("curcuma has no module registered for \"%1\"; commands with "
                                   "parameters include sp, opt, md, rmsd, analysis, confscan, "
                                   "dock, hessian").arg(command));
            }
            ToolResult result = ToolResult::success(details);
            result.truncated = true;   // it is a listing, not the whole engine
            return result;
        };
        add(spec);
    }

    return added;
}
