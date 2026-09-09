// tools_compute.cpp - Tools that let the model have something calculated.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_compute.h"

#include "atomselection.h"
#include "core/toolregistry.h"
#include "curcumajob.h"
#include "curcuma_schemas.h"
#include "view.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QWaitCondition>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

/// Finished jobs, so job_status can answer after the fact. A calculation is
/// asynchronous by necessity, and a model that asked for one has to be able to
/// come back for the answer.
///
/// It is written on the GUI thread (where CurcumaJob lives) and read from the
/// agent loop's thread, so it carries its own lock. The wait condition is what
/// turns job_status into a single call instead of a polling loop: the model asks
/// once with wait_seconds and is woken when the result lands.
struct JobStore {
    QMutex mutex;
    QWaitCondition arrived;
    QHash<QString, CurcumaJobResult> finished;
    QStringList order;
};

/// The two keys run_single_point adds on top of curcuma's own parameters. They are
/// qurcuma's, not the engine's, so they are named once here and stripped out again
/// before the controller is handed over.
const QLatin1String kSelection("atoms");
const QLatin1String kSelectionIndices("atom_indices");

/// curcuma's generated schema plus the selection keys. The registry rejects
/// unknown keys, so a parameter the tool understands has to be in the schema.
QJsonObject withSelection(QJsonObject schema)
{
    QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();

    QJsonObject selection;
    selection.insert(QStringLiteral("type"), QStringLiteral("string"));
    selection.insert(QStringLiteral("description"), QStringLiteral(
        "Restrict the calculation to part of the structure, in curcuma's selection grammar: "
        "\"F1\" for the first fragment (get_fragments lists them), \"1:20\" for a one-based "
        "atom range. Omitted means the whole structure."));
    properties.insert(kSelection, selection);

    // No "items": the validator honours only the keys it can enforce, so the
    // element type is stated where a model will actually read it.
    QJsonObject explicitIndices;
    explicitIndices.insert(QStringLiteral("type"), QStringLiteral("array"));
    explicitIndices.insert(QStringLiteral("description"), QStringLiteral(
        "Zero-based atom indices, as an alternative to atoms. Not both."));
    properties.insert(kSelectionIndices, explicitIndices);

    schema.insert(QStringLiteral("properties"), properties);
    return schema;
}

/// The arguments curcuma should see: everything except qurcuma's selection keys.
QJsonObject withoutSelection(QJsonObject args)
{
    args.remove(kSelection);
    args.remove(kSelectionIndices);
    return args;
}

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
        {
            QMutexLocker lock(&store->mutex);
            store->finished.insert(result.jobId, result);
            store->order.append(result.jobId);
        }
        store->arrived.wakeAll();
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
            "Calculate the energy of the loaded structure, or of part of it, with curcuma, "
            "in process. Returns immediately with a job_id; ask job_status for the answer. "
            "For an interaction energy, run it three times -- whole, \"F1\", \"F2\" -- and "
            "subtract. The parameters come from curcuma's own registry; describe_job(\"sp\") "
            "lists all of them.");
        spec.effect = ToolEffect::Compute;   // asks before it runs
        spec.affinity = ToolAffinity::Gui;   // reads the viewer's atoms
        spec.paramSchema = withSelection(curcumaJobSchema(QStringLiteral("sp")));

        spec.handler = [viewer, job](const QJsonObject& args) {
            const QVector<moldata::Atom> all = viewer->getCurrentFrameAtoms();
            if (all.isEmpty())
                return ToolResult::failure(QStringLiteral("no structure is loaded"));

            QVector<moldata::Atom> atoms = all;
            int severed = 0;
            const QString expression = args.value(kSelection).toString();
            const QJsonArray indices = args.value(kSelectionIndices).toArray();
            const bool restricted = !expression.isEmpty() || !indices.isEmpty();
            if (restricted) {
                QVector<int> wanted;
                QString error;
                if (!resolveAtomSet(all, expression, indices, wanted, error))
                    return ToolResult::failure(error);
                atoms = subsetAtoms(all, wanted);
                severed = severedBondCount(all, wanted);
            }

            CurcumaJobRequest request;
            request.command = QStringLiteral("sp");
            request.atoms = atoms;
            request.controller = withoutSelection(args);

            QString error;
            int position = 0;
            const QString jobId = job->start(request, &error, &position);
            if (jobId.isEmpty())
                return ToolResult::failure(error);

            QJsonObject data;
            data.insert(QStringLiteral("status"),
                position == 0 ? QStringLiteral("started") : QStringLiteral("queued"));
            data.insert(QStringLiteral("job_id"), jobId);
            data.insert(QStringLiteral("atom_count"), atoms.size());
            if (position > 0)
                data.insert(QStringLiteral("queue_position"), position);

            QString note = position == 0
                ? QStringLiteral("Started; ask job_status with job_id \"%1\" and wait_seconds "
                                 "to be given the answer rather than polling for it.").arg(jobId)
                : QStringLiteral("Queued as number %1; one calculation runs at a time. Ask "
                                 "job_status with job_id \"%2\" and wait_seconds.")
                      .arg(position).arg(jobId);
            if (restricted) {
                data.insert(QStringLiteral("of_total"), all.size());
                data.insert(QStringLiteral("severed_bonds"), severed);
                // A subset that cuts covalent bonds leaves open valences behind, and
                // curcuma will happily compute an energy for the radical fragment.
                // Say so rather than letting the number be subtracted from a complex.
                note += severed == 0
                    ? QStringLiteral(" %1 of %2 atoms, no bond cut.").arg(atoms.size()).arg(all.size())
                    : QStringLiteral(" %1 of %2 atoms, but the selection cuts %3 covalent bond(s): "
                                     "the fragment has open valences and its energy is not "
                                     "comparable to the whole.")
                          .arg(atoms.size()).arg(all.size()).arg(severed);
            }
            return ToolResult::success(data, note);
        };
        add(spec);
    }

    // --- job_status ---------------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("job_status");
        spec.category = QStringLiteral("compute");
        spec.description = QStringLiteral(
            "Where a calculation stands. With wait_seconds it waits for the answer instead "
            "of returning at once, which is one call rather than a poll every second. "
            "Without a job_id it reports what is running and what is queued.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;
        spec.paramSchema = schemaFromJson(R"JSON({
          "type": "object",
          "properties": {
            "job_id": { "type": "string", "description": "the job to ask about" },
            "wait_seconds": { "type": "integer", "minimum": 0, "maximum": 60,
                              "description": "wait up to this long for the job to finish (default 0)" }
          }
        })JSON");

        spec.handler = [job, store](const QJsonObject& args) {
            const QString wanted = args.value(QStringLiteral("job_id")).toString();
            const int waitSeconds = qBound(0, args.value(QStringLiteral("wait_seconds")).toInt(), 60);

            if (!wanted.isEmpty()) {
                QElapsedTimer clock;
                clock.start();
                QMutexLocker lock(&store->mutex);
                forever {
                    if (store->finished.contains(wanted))
                        return ToolResult::success(resultToJson(store->finished.value(wanted)));

                    // Asked outside the store's lock would be a race with the job
                    // finishing between the two questions; CurcumaJob has its own
                    // lock and never reaches back here, so nesting is safe.
                    const CurcumaJobState state = job->state();
                    const bool running = state.running == wanted;
                    const int queuePosition = state.queued.indexOf(wanted) + 1;
                    if (!running && queuePosition == 0)
                        return ToolResult::failure(QStringLiteral("no job called \"%1\"").arg(wanted));

                    const qint64 left = qint64(waitSeconds) * 1000 - clock.elapsed();
                    if (left <= 0) {
                        QJsonObject data;
                        data.insert(QStringLiteral("job_id"), wanted);
                        data.insert(QStringLiteral("status"),
                            running ? QStringLiteral("running") : QStringLiteral("queued"));
                        if (!running)
                            data.insert(QStringLiteral("queue_position"), queuePosition);
                        return ToolResult::success(data, running
                                ? QStringLiteral("Still running.")
                                : QStringLiteral("Still queued, number %1.").arg(queuePosition));
                    }
                    // Woken by the finished handler, or by the timeout to re-check
                    // that the job is still there at all.
                    store->arrived.wait(&store->mutex, qMin<qint64>(left, 500));
                }
            }

            const CurcumaJobState state = job->state();
            QJsonObject data;
            data.insert(QStringLiteral("running"), !state.running.isEmpty());
            if (!state.running.isEmpty())
                data.insert(QStringLiteral("job_id"), state.running);
            QJsonArray queued;
            for (const QString& id : state.queued)
                queued.append(id);
            data.insert(QStringLiteral("queued_jobs"), queued);
            QJsonArray done;
            {
                QMutexLocker lock(&store->mutex);
                for (const QString& id : store->order)
                    done.append(id);
            }
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
