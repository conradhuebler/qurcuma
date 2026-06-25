// lesson.cpp - Teaching scenario ("Lesson") data model + JSON serialization
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - see lesson.h for the design overview.

#include "lesson.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace {

// SimulationConfig::Mode <-> compact string ("md" / "opt").
QString modeToString(SimulationConfig::Mode mode)
{
    return mode == SimulationConfig::Mode::GeometryOptimization
        ? QStringLiteral("opt") : QStringLiteral("md");
}

SimulationConfig::Mode modeFromString(const QString& s)
{
    return s == QStringLiteral("opt")
        ? SimulationConfig::Mode::GeometryOptimization
        : SimulationConfig::Mode::MolecularDynamics;
}

// Filesystem-safe slug from a structure name (lowercase, [a-z0-9_] only).
QString slugify(const QString& name, int fallbackIndex)
{
    QString out;
    out.reserve(name.size());
    for (const QChar c : name) {
        if (c.isLetterOrNumber())
            out.append(c.toLower());
        else if (!out.isEmpty() && out.back() != QLatin1Char('_'))
            out.append(QLatin1Char('_'));
    }
    while (out.endsWith(QLatin1Char('_')))
        out.chop(1);
    if (out.isEmpty())
        out = QStringLiteral("structure_%1").arg(fallbackIndex + 1);
    return out;
}

} // namespace

QJsonObject simConfigToJson(const SimulationConfig& cfg)
{
    QJsonObject o;
    o["mode"] = modeToString(cfg.mode);
    o["method"] = cfg.method;
    o["optimizer"] = cfg.optimizer;
    o["temperature"] = cfg.temperature;
    o["timestep"] = cfg.timestep;
    o["steps"] = cfg.steps;
    o["convergence"] = cfg.convergence;
    o["optKeepParameters"] = cfg.optKeepParameters;
    o["writeTrajectory"] = cfg.writeTrajectory;
    o["fpsLimit"] = cfg.fpsLimit;
    o["performanceAnalysis"] = cfg.performanceAnalysis;
    o["performanceInterval"] = cfg.performanceInterval;
    o["gpu"] = cfg.gpu;
    o["hmass"] = cfg.hmass;

    // RATTLE
    o["rattleMode"] = cfg.rattleMode;
    o["rattle12"] = cfg.rattle12;
    o["rattle13"] = cfg.rattle13;
    o["rattleTol12"] = cfg.rattleTol12;
    o["rattleTol13"] = cfg.rattleTol13;
    o["rattleMaxIter"] = cfg.rattleMaxIter;

    o["topologyMode"] = cfg.topologyMode;

    // Thermostat
    o["thermostat"] = cfg.thermostat;
    o["thermostatCoupling"] = cfg.thermostatCoupling;
    o["andersenProbability"] = cfg.andersenProbability;
    o["noseChainLength"] = cfg.noseChainLength;

    // RMSD metadynamics
    o["rmsdMtd"] = cfg.rmsdMtd;
    o["rmsdMtdK"] = cfg.rmsdMtdK;
    o["rmsdMtdAlpha"] = cfg.rmsdMtdAlpha;
    o["rmsdMtdAtoms"] = cfg.rmsdMtdAtoms;
    o["rmsdMtdRefFile"] = cfg.rmsdMtdRefFile;
    o["rmsdMtdMaxGaussians"] = cfg.rmsdMtdMaxGaussians;
    o["rmsdMtdMaxHeight"] = cfg.rmsdMtdMaxHeight;
    o["rmsdMtdEconv"] = cfg.rmsdMtdEconv;
    o["rmsdMtdPace"] = cfg.rmsdMtdPace;
    o["rmsdMtdWtmtd"] = cfg.rmsdMtdWtmtd;
    o["rmsdMtdDt"] = cfg.rmsdMtdDt;
    o["rmsdMtdFreezeInherited"] = cfg.rmsdMtdFreezeInherited;

    // Confinement walls (encode "pressure" via box volume + composition)
    o["wallEnabled"] = cfg.wallEnabled;
    o["wallType"] = cfg.wallType;
    o["wallHarmonic"] = cfg.wallHarmonic;
    o["wallXmin"] = cfg.wallXmin;  o["wallXmax"] = cfg.wallXmax;
    o["wallYmin"] = cfg.wallYmin;  o["wallYmax"] = cfg.wallYmax;
    o["wallZmin"] = cfg.wallZmin;  o["wallZmax"] = cfg.wallZmax;
    o["wallRadius"] = cfg.wallRadius;
    o["wallTemp"] = cfg.wallTemp;
    o["wallBeta"] = cfg.wallBeta;

    // Temperature ramp + regions
    o["tempRamp"] = cfg.tempRamp;
    o["tempSchedule"] = cfg.tempSchedule;
    QJsonArray regions;
    for (const TempRegion& r : cfg.tempRegions) {
        QJsonObject ro;
        ro["atoms"] = r.atoms;
        ro["temperature"] = r.temperature;
        ro["schedule"] = r.schedule;
        regions.append(ro);
    }
    o["tempRegions"] = regions;
    return o;
}

SimulationConfig simConfigFromJson(const QJsonObject& o)
{
    SimulationConfig cfg;  // start from defaults so missing keys stay sane
    cfg.mode = modeFromString(o.value("mode").toString(modeToString(cfg.mode)));
    cfg.method = o.value("method").toString(cfg.method);
    cfg.optimizer = o.value("optimizer").toString(cfg.optimizer);
    cfg.temperature = o.value("temperature").toDouble(cfg.temperature);
    cfg.timestep = o.value("timestep").toDouble(cfg.timestep);
    cfg.steps = o.value("steps").toInt(cfg.steps);
    cfg.convergence = o.value("convergence").toDouble(cfg.convergence);
    cfg.optKeepParameters = o.value("optKeepParameters").toBool(cfg.optKeepParameters);
    cfg.writeTrajectory = o.value("writeTrajectory").toBool(cfg.writeTrajectory);
    cfg.fpsLimit = o.value("fpsLimit").toInt(cfg.fpsLimit);
    cfg.performanceAnalysis = o.value("performanceAnalysis").toBool(cfg.performanceAnalysis);
    cfg.performanceInterval = o.value("performanceInterval").toInt(cfg.performanceInterval);
    cfg.gpu = o.value("gpu").toString(cfg.gpu);
    cfg.hmass = o.value("hmass").toDouble(cfg.hmass);

    cfg.rattleMode = o.value("rattleMode").toInt(cfg.rattleMode);
    cfg.rattle12 = o.value("rattle12").toBool(cfg.rattle12);
    cfg.rattle13 = o.value("rattle13").toBool(cfg.rattle13);
    cfg.rattleTol12 = o.value("rattleTol12").toDouble(cfg.rattleTol12);
    cfg.rattleTol13 = o.value("rattleTol13").toDouble(cfg.rattleTol13);
    cfg.rattleMaxIter = o.value("rattleMaxIter").toInt(cfg.rattleMaxIter);

    cfg.topologyMode = o.value("topologyMode").toString(cfg.topologyMode);

    cfg.thermostat = o.value("thermostat").toString(cfg.thermostat);
    cfg.thermostatCoupling = o.value("thermostatCoupling").toDouble(cfg.thermostatCoupling);
    cfg.andersenProbability = o.value("andersenProbability").toDouble(cfg.andersenProbability);
    cfg.noseChainLength = o.value("noseChainLength").toInt(cfg.noseChainLength);

    cfg.rmsdMtd = o.value("rmsdMtd").toBool(cfg.rmsdMtd);
    cfg.rmsdMtdK = o.value("rmsdMtdK").toDouble(cfg.rmsdMtdK);
    cfg.rmsdMtdAlpha = o.value("rmsdMtdAlpha").toDouble(cfg.rmsdMtdAlpha);
    cfg.rmsdMtdAtoms = o.value("rmsdMtdAtoms").toString(cfg.rmsdMtdAtoms);
    cfg.rmsdMtdRefFile = o.value("rmsdMtdRefFile").toString(cfg.rmsdMtdRefFile);
    cfg.rmsdMtdMaxGaussians = o.value("rmsdMtdMaxGaussians").toInt(cfg.rmsdMtdMaxGaussians);
    cfg.rmsdMtdMaxHeight = o.value("rmsdMtdMaxHeight").toInt(cfg.rmsdMtdMaxHeight);
    cfg.rmsdMtdEconv = o.value("rmsdMtdEconv").toDouble(cfg.rmsdMtdEconv);
    cfg.rmsdMtdPace = o.value("rmsdMtdPace").toInt(cfg.rmsdMtdPace);
    cfg.rmsdMtdWtmtd = o.value("rmsdMtdWtmtd").toBool(cfg.rmsdMtdWtmtd);
    cfg.rmsdMtdDt = o.value("rmsdMtdDt").toDouble(cfg.rmsdMtdDt);
    cfg.rmsdMtdFreezeInherited = o.value("rmsdMtdFreezeInherited").toBool(cfg.rmsdMtdFreezeInherited);

    cfg.wallEnabled = o.value("wallEnabled").toBool(cfg.wallEnabled);
    cfg.wallType = o.value("wallType").toInt(cfg.wallType);
    cfg.wallHarmonic = o.value("wallHarmonic").toBool(cfg.wallHarmonic);
    cfg.wallXmin = o.value("wallXmin").toDouble(cfg.wallXmin);
    cfg.wallXmax = o.value("wallXmax").toDouble(cfg.wallXmax);
    cfg.wallYmin = o.value("wallYmin").toDouble(cfg.wallYmin);
    cfg.wallYmax = o.value("wallYmax").toDouble(cfg.wallYmax);
    cfg.wallZmin = o.value("wallZmin").toDouble(cfg.wallZmin);
    cfg.wallZmax = o.value("wallZmax").toDouble(cfg.wallZmax);
    cfg.wallRadius = o.value("wallRadius").toDouble(cfg.wallRadius);
    cfg.wallTemp = o.value("wallTemp").toDouble(cfg.wallTemp);
    cfg.wallBeta = o.value("wallBeta").toDouble(cfg.wallBeta);

    cfg.tempRamp = o.value("tempRamp").toBool(cfg.tempRamp);
    cfg.tempSchedule = o.value("tempSchedule").toString(cfg.tempSchedule);
    cfg.tempRegions.clear();
    const QJsonArray regions = o.value("tempRegions").toArray();
    for (const QJsonValue& v : regions) {
        const QJsonObject ro = v.toObject();
        TempRegion r;
        r.atoms = ro.value("atoms").toString(r.atoms);
        r.temperature = ro.value("temperature").toDouble(r.temperature);
        r.schedule = ro.value("schedule").toString(r.schedule);
        cfg.tempRegions.push_back(r);
    }
    return cfg;
}

QJsonObject lessonToJson(const Lesson& lesson, bool inlineXyz)
{
    QJsonObject root;
    root["format"] = QStringLiteral("qurcuma-lesson");
    root["version"] = 1;

    // Metadata
    QJsonObject meta;
    meta["title"] = lesson.meta.title;
    meta["description"] = lesson.meta.description;
    meta["license"] = lesson.meta.license;
    meta["language"] = lesson.meta.language;
    meta["created"] = lesson.meta.created;
    meta["modified"] = lesson.meta.modified;
    meta["qurcumaVersion"] = lesson.meta.qurcumaVersion;
    QJsonArray authors;
    for (const LessonAuthor& a : lesson.meta.authors) {
        QJsonObject ao;
        ao["name"] = a.name;
        ao["orcid"] = a.orcid;
        ao["institution"] = a.institution;
        ao["email"] = a.email;
        authors.append(ao);
    }
    meta["authors"] = authors;
    meta["keywords"] = QJsonArray::fromStringList(lesson.meta.keywords);
    root["meta"] = meta;

    // Structures
    QJsonArray structures;
    for (const LessonStructure& s : lesson.structures) {
        QJsonObject so;
        so["name"] = s.name;
        so["description"] = s.description;
        so["role"] = s.role;
        if (inlineXyz)
            so["xyz"] = s.xyz;
        else
            so["file"] = s.file;
        so["sim"] = simConfigToJson(s.sim);
        structures.append(so);
    }
    root["structures"] = structures;
    return root;
}

Lesson lessonFromJson(const QJsonObject& obj, QString* error)
{
    Lesson lesson;
    if (obj.value("format").toString() != QStringLiteral("qurcuma-lesson")) {
        if (error)
            *error = QStringLiteral("Not a Qurcuma lesson file (missing format marker).");
        return lesson;
    }

    const QJsonObject meta = obj.value("meta").toObject();
    lesson.meta.title = meta.value("title").toString();
    lesson.meta.description = meta.value("description").toString();
    lesson.meta.license = meta.value("license").toString(lesson.meta.license);
    lesson.meta.language = meta.value("language").toString(lesson.meta.language);
    lesson.meta.created = meta.value("created").toString();
    lesson.meta.modified = meta.value("modified").toString();
    lesson.meta.qurcumaVersion = meta.value("qurcumaVersion").toString();
    for (const QJsonValue& v : meta.value("authors").toArray()) {
        const QJsonObject ao = v.toObject();
        LessonAuthor a;
        a.name = ao.value("name").toString();
        a.orcid = ao.value("orcid").toString();
        a.institution = ao.value("institution").toString();
        a.email = ao.value("email").toString();
        lesson.meta.authors.push_back(a);
    }
    for (const QJsonValue& v : meta.value("keywords").toArray())
        lesson.meta.keywords << v.toString();

    for (const QJsonValue& v : obj.value("structures").toArray()) {
        const QJsonObject so = v.toObject();
        LessonStructure s;
        s.name = so.value("name").toString();
        s.description = so.value("description").toString();
        s.role = so.value("role").toString();
        s.xyz = so.value("xyz").toString();   // inline variant
        s.file = so.value("file").toString(); // reference variant
        s.sim = simConfigFromJson(so.value("sim").toObject());
        lesson.structures.push_back(s);
    }
    return lesson;
}

QString atomsToXyz(const QVector<MoleculeViewer::Atom>& atoms, const QString& comment)
{
    QString out;
    QTextStream ts(&out);
    ts << atoms.size() << '\n' << comment << '\n';
    for (const MoleculeViewer::Atom& a : atoms) {
        ts << a.element << ' '
           << QString::number(a.position.x(), 'f', 6) << ' '
           << QString::number(a.position.y(), 'f', 6) << ' '
           << QString::number(a.position.z(), 'f', 6) << '\n';
    }
    return out;
}

bool xyzToAtoms(const QString& xyz, QVector<MoleculeViewer::Atom>& atoms)
{
    atoms.clear();
    const QStringList lines = xyz.split(QLatin1Char('\n'));
    if (lines.size() < 3)
        return false;
    bool ok = false;
    const int n = lines.at(0).trimmed().toInt(&ok);
    if (!ok || n <= 0)
        return false;
    // line 0 = count, line 1 = comment, atoms start at line 2.
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    for (int i = 0; i < n && (2 + i) < lines.size(); ++i) {
        const QStringList t = lines.at(2 + i).trimmed().split(ws, Qt::SkipEmptyParts);
        if (t.size() < 4)
            continue;
        MoleculeViewer::Atom a;
        a.element = t.at(0);
        a.position = QVector3D(t.at(1).toFloat(), t.at(2).toFloat(), t.at(3).toFloat());
        atoms.push_back(a);
    }
    return !atoms.isEmpty();
}

bool extractLesson(Lesson& lesson, const QString& targetDir, QString* error)
{
    QDir dir(targetDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QStringLiteral("Could not create directory: %1").arg(targetDir);
        return false;
    }

    QSet<QString> usedNames;
    for (int i = 0; i < lesson.structures.size(); ++i) {
        LessonStructure& s = lesson.structures[i];
        QString base = slugify(s.name, i);
        QString fileName = base + QStringLiteral(".xyz");
        int dup = 2;
        while (usedNames.contains(fileName))
            fileName = QStringLiteral("%1_%2.xyz").arg(base).arg(dup++);
        usedNames.insert(fileName);

        QFile f(dir.filePath(fileName));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (error)
                *error = QStringLiteral("Could not write structure file: %1").arg(fileName);
            return false;
        }
        QByteArray data = s.xyz.toUtf8();
        if (!data.endsWith('\n'))
            data.append('\n');
        f.write(data);
        f.close();
        s.file = fileName;  // record the reference for the sidecar
    }

    // Sidecar with file references (not inline xyz).
    QFile sidecar(dir.filePath(QStringLiteral("lesson.json")));
    if (!sidecar.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Could not write sidecar lesson.json");
        return false;
    }
    sidecar.write(QJsonDocument(lessonToJson(lesson, /*inlineXyz=*/false)).toJson(QJsonDocument::Indented));
    sidecar.close();
    return true;
}
