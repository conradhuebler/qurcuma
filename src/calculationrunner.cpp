// calculationrunner.cpp - External-process orchestration for calculations.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - see calculationrunner.h.

#include "calculationrunner.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>

namespace {
/// Build a per-run file name, optionally stamped with the current time so that
/// successive runs in the same directory don't clobber each other.
QString uniqueFileName(const QString& base, const QString& extension, bool unique)
{
    if (unique) {
        const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        return extension.isEmpty() ? QString("%1_%2").arg(base, timestamp)
                                   : QString("%1_%2.%3").arg(base, timestamp, extension);
    }
    return extension.isEmpty() ? base : QString("%1.%2").arg(base, extension);
}
}  // namespace

CalculationRunner::CalculationRunner(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    // Command-line completer suggestions per program.
    m_programCommands["curcuma"] = QStringList{
        "--align", "--rmsd", "--cluster", "--compare", "--convert", "--distance",
        "--docking", "--energy", "--geometry", "--md", "--md-analysis",
        "--reactive", "--traj-rmsd", "--opt",
    };
    m_programCommands["xtb"] = QStringList{
        "--opt", "--md", "--hess", "--ohess", "--bhess", "--grad", "--ograd",
        "--scc", "--vip", "--vipea", "--sp", "--gfn0", "--gfn1", "--gfn2",
        "--gfnff", "--alpb", "--gbsa", "--cosmo", "--wbo", "--pop", "--molden",
        "--dipole", "--chrg", "--uhf",
    };

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        emit outputReceived(QString::fromUtf8(m_process->readAllStandardOutput()));
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        emit errorReceived(QString::fromUtf8(m_process->readAllStandardError()));
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this](int code, QProcess::ExitStatus) { handleFinished(code); });
}

CalculationEntry CalculationRunner::start(const CalculationRequest& req)
{
    const QString sep = QDir::separator();

    // Generate the per-run file names (each may carry its own timestamp).
    const QString structureFile = uniqueFileName(req.structureBase, req.structureExt, req.uniqueFileNames);
    const QString trjFile = uniqueFileName(req.structureBase, "trj" + req.structureExt, req.uniqueFileNames);
    const QString inputFile = uniqueFileName(req.inputBase, req.inputExt, req.uniqueFileNames);
    const QString outputFile = uniqueFileName("output", "log", req.uniqueFileNames);

    // Persist the structure and input editors to disk.
    QFile structFile(req.calcDir + sep + structureFile);
    if (structFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        structFile.write(req.structureText.toUtf8());
        structFile.close();
    }
    QFile inputFileObj(req.calcDir + sep + inputFile);
    if (inputFileObj.open(QIODevice::WriteOnly | QIODevice::Text)) {
        inputFileObj.write(req.inputText.toLatin1());
        inputFileObj.close();
    }

    // Describe the run for the history + finished().
    CalculationEntry entry;
    entry.id = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    entry.program = req.program;
    entry.command = req.command;
    entry.structureFile = structureFile;
    entry.outputFile = outputFile;
    entry.timestamp = QDateTime::currentDateTime();
    entry.status = "started";

    m_trjFile.clear();

    if (req.program == "orca") {
        // ORCA takes the input file name as its sole argument.
        m_process->setWorkingDirectory(req.calcDir);
        m_process->setProgram(req.orcaBinaryPath + "/orca");
        m_process->setArguments(QStringList() << inputFile);
        QFile::copy(req.calcDir + sep + structureFile,
                    req.calcDir + sep + req.structureBase + ".xyz");
    } else {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("OMP_NUM_THREADS", QString::number(req.threads));
        m_process->setEnvironment(environment.toStringList());
        m_process->setWorkingDirectory(req.calcDir);
        m_process->setProgram(req.programPath);

        QStringList args;
        if (req.program == "curcuma") {
            // curcuma <verb> <structure> <rest...>
            args = req.command.split(" ", Qt::SkipEmptyParts);
            if (args.size() >= 1) {
                args.insert(1, structureFile);
            }
        } else if (req.program == "xtb") {
            // xtb <structure> <rest...>; xtb writes its optimised geometry to
            // xtbopt.xyz/.log, which handleFinished() renames to our trj file.
            args << structureFile;
            args.append(req.command.split(" ", Qt::SkipEmptyParts));
            m_trjFile = trjFile;
        }
        m_process->setArguments(args);
    }

    // stdout + stderr both stream into the single output log (appended).
    m_process->setStandardOutputFile(req.calcDir + sep + outputFile, QIODevice::Append);
    m_process->setStandardErrorFile(req.calcDir + sep + outputFile, QIODevice::Append);

    m_entry = entry;
    m_process->start();
    return entry;
}

void CalculationRunner::handleFinished(int exitCode)
{
    // xtb writes the optimised geometry to xtbopt.xyz / xtbopt.log — rename it to
    // our per-run trajectory file so the viewer/analysis find it. (The rename
    // target keeps the original bare-name semantics: relative to the app CWD.)
    if (m_entry.program == "xtb" && !m_trjFile.isEmpty()) {
        const QString dir = m_process->workingDirectory();
        for (const QString& src : {QStringLiteral("xtbopt.xyz"), QStringLiteral("xtbopt.log")}) {
            const QString path = dir + QDir::separator() + src;
            if (QFile::exists(path)) {
                QFile::rename(path, m_trjFile);
            }
        }
    }
    emit finished(m_entry, exitCode);
}

void CalculationRunner::cancel()
{
    if (isRunning()) {
        m_process->kill();
    }
}

bool CalculationRunner::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

QStringList CalculationRunner::commandsFor(const QString& program) const
{
    return m_programCommands.value(program);
}
