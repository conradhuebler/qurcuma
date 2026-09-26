// test_recipes.cpp - Claude Generated 2026 (UX stage 6 S1/S2)
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Simulation settings as JSON (src/lesson.*) and recipes (src/recipe.h):
//  - simConfigToJson/simConfigFromJson round trip with every field moved away from
//    its default, so a field written but not read (or the reverse) shows up here;
//  - the lesson's optional panel list;
//  - recipes: a recipe never changes method, charge, spin or the working
//    preferences, and a built-in switches off every feature it does not use.

#include <QJsonArray>
#include <QJsonObject>

#include <iostream>
#include <string>

#include "lesson.h"
#include "recipe.h"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!condition)
        ++g_failures;
}

// A configuration with every field away from its default.
SimulationConfig oddConfig()
{
    SimulationConfig c;
    c.mode = SimulationConfig::Mode::GeometryOptimization;
    c.method = QStringLiteral("gfn2");
    c.optimizer = QStringLiteral("ancopt");
    c.charge = -2;
    c.spin = 1;
    c.temperature = 412.5;
    c.timestep = 0.5;
    c.steps = 777;
    c.convergence = 3e-5;
    c.optKeepParameters = false;
    c.writeTrajectory = true;
    c.fpsLimit = 12;
    c.performanceAnalysis = true;
    c.performanceInterval = 42;
    c.gpu = QStringLiteral("auto");
    c.rattleMode = 2;
    c.rattle12 = false;
    c.rattle13 = true;
    c.rattleTol12 = 2e-4;
    c.rattleTol13 = 3e-3;
    c.rattleMaxIter = 55;
    c.topologyMode = QStringLiteral("react");
    c.hmass = 3.0;
    c.thermostat = QStringLiteral("nosehover");
    c.thermostatCoupling = 25.0;
    c.andersenProbability = 0.01;
    c.noseChainLength = 5;
    c.rmsdMtd = true;
    c.rmsdMtdK = 0.02;
    c.rmsdMtdAlpha = 7.0;
    c.rmsdMtdAtoms = QStringLiteral("1-10");
    c.rmsdMtdRefFile = QStringLiteral("ref.xyz");
    c.rmsdMtdMaxGaussians = 99;
    c.rmsdMtdMaxHeight = 4;
    c.rmsdMtdDepositStride = 15.0;
    c.rmsdMtdRdep = 0.4;
    c.rmsdMtdWtmtd = true;
    c.rmsdMtdDt = 1500.0;
    c.rmsdMtdFreezeInherited = true;
    c.wallEnabled = true;
    c.wallType = 2;
    c.wallHarmonic = false;
    c.wallXmin = -1.0; c.wallXmax = 2.0;
    c.wallYmin = -3.0; c.wallYmax = 4.0;
    c.wallZmin = -5.0; c.wallZmax = 6.0;
    c.wallRadius = 9.0;
    c.wallTemp = 500.0;
    c.wallBeta = 3.0;
    c.tempRamp = true;
    c.tempSchedule = QStringLiteral("600:steps:100;300:reach:5");
    c.tempRegions.push_back({ QStringLiteral("1:3"), 450.0, QStringLiteral("500:steps:10") });
    return c;
}

} // namespace

int main()
{
    std::cout << "Simulation JSON and recipe test" << std::endl;
    std::cout << "===============================" << std::endl;

    // --- SimulationConfig JSON round trip -------------------------------------------
    const SimulationConfig odd = oddConfig();
    const QJsonObject oddJson = simConfigToJson(odd);
    check(simConfigToJson(simConfigFromJson(oddJson)) == oddJson,
        "every SimulationConfig field survives JSON and back");
    const SimulationConfig back = simConfigFromJson(oddJson);
    check(back.charge == -2 && back.spin == 1, "charge and unpaired electrons are stored");
    check(back.rmsdMtdDepositStride == 15.0 && back.rmsdMtdRdep == 0.4,
        "the strided RMSD-MTD parameters are stored");
    check(!oddJson.contains(QStringLiteral("rmsdMtdEconv")),
        "the legacy RMSD-MTD convergence threshold is not written");
    const SimulationConfig defaults;
    check(simConfigToJson(simConfigFromJson(QJsonObject())) == simConfigToJson(defaults),
        "missing keys fall back to the defaults");

    // --- Lesson panels -------------------------------------------------------------
    Lesson lesson;
    lesson.meta.title = QStringLiteral("Panels");
    LessonStructure s;
    s.name = QStringLiteral("water");
    s.xyz = QStringLiteral("1\n\nO 0 0 0\n");
    lesson.structures.push_back(s);
    lesson.panels = { QStringLiteral("ProjectDock"), QStringLiteral("SimulationDock") };
    QString error;
    const Lesson lessonBack = lessonFromJson(lessonToJson(lesson, true), &error);
    check(error.isEmpty() && lessonBack.panels == lesson.panels, "a lesson keeps its open panels");
    lesson.panels.clear();
    const QJsonObject noPanels = lessonToJson(lesson, true);
    check(!noPanels.contains(QStringLiteral("layout")) && lessonFromJson(noPanels).panels.isEmpty(),
        "a lesson without panels writes no layout and reads back none");

    // --- Recipes -------------------------------------------------------------------
    const QJsonObject protocol = recipes::protocolOf(odd);
    bool noKept = true;
    for (const QString& key : recipes::keptKeys())
        noKept = noKept && !protocol.contains(key);
    check(noKept, "a recipe's protocol holds none of the kept keys");

    // A stored recipe that (wrongly) carries kept keys must still not change them.
    SimulationRecipe sneaky{ QStringLiteral("Sneaky"), QString(), recipes::protocolOf(defaults) };
    sneaky.protocol.insert(QStringLiteral("method"), QStringLiteral("uff"));
    sneaky.protocol.insert(QStringLiteral("charge"), 0);
    sneaky.protocol.insert(QStringLiteral("writeTrajectory"), false);
    const SimulationConfig kept = recipes::apply(odd, sneaky);
    check(kept.method == odd.method && kept.charge == odd.charge && kept.spin == odd.spin
            && kept.topologyMode == odd.topologyMode && kept.gpu == odd.gpu
            && kept.fpsLimit == odd.fpsLimit && kept.writeTrajectory == odd.writeTrajectory
            && kept.optKeepParameters == odd.optKeepParameters,
        "applying a recipe keeps method, charge, spin, topology, GPU and working preferences");

    const QVector<SimulationRecipe> builtIns = recipes::builtIn();
    auto find = [&builtIns](const QString& name) {
        for (const SimulationRecipe& r : builtIns)
            if (r.name == name)
                return r;
        return SimulationRecipe{};
    };
    const SimulationConfig md = recipes::apply(odd, find(QStringLiteral("MD 300 K")));
    check(md.mode == SimulationConfig::Mode::MolecularDynamics && md.temperature == 300.0
            && md.steps == 10000 && md.timestep == 1.0 && md.thermostat == QStringLiteral("csvr"),
        "MD 300 K sets the MD protocol");
    check(!md.tempRamp && md.tempRegions.isEmpty() && md.rattleMode == 0 && !md.rmsdMtd && !md.wallEnabled,
        "MD 300 K switches off ramp, regions, RATTLE, RMSD-MTD and walls left over from before");
    const SimulationConfig relax = recipes::apply(odd, find(QStringLiteral("Quick relax")));
    check(relax.mode == SimulationConfig::Mode::GeometryOptimization && relax.convergence == 1e-3
            && relax.steps == 1000,
        "Quick relax uses curcuma's loose preset (1e-3 Eh/Bohr, 1000 iterations)");
    const SimulationConfig heat = recipes::apply(defaults, find(QStringLiteral("Heat up")));
    check(heat.tempRamp && heat.temperature == 50.0
            && heat.tempSchedule == QStringLiteral("300:steps:10000;300:reach:10"),
        "Heat up starts at 50 K and ramps to 300 K");
    const SimulationConfig confined = recipes::apply(defaults, find(QStringLiteral("Confined")));
    check(confined.wallEnabled && confined.wallType == 1 && confined.wallRadius == 0.0,
        "Confined adds an automatically sized spherical wall");

    bool distinctNames = true;
    for (int i = 0; i < builtIns.size(); ++i)
        for (int j = i + 1; j < builtIns.size(); ++j)
            distinctNames = distinctNames && builtIns[i].name.compare(builtIns[j].name, Qt::CaseInsensitive) != 0;
    check(builtIns.size() == 4 && distinctNames, "four built-in recipes with distinct names");
    check(recipes::isBuiltInName(QStringLiteral("quick RELAX")) && !recipes::isBuiltInName(QStringLiteral("Sneaky")),
        "built-in recipe names are recognised case-insensitively");

    const SimulationRecipe mine{ QStringLiteral("Mine"), QStringLiteral("d"), protocol };
    const SimulationRecipe mineBack = recipes::fromJson(recipes::toJson(mine));
    check(mineBack.name == mine.name && mineBack.description == mine.description
            && mineBack.protocol == mine.protocol,
        "a user recipe survives JSON and back");

    std::cout << std::endl;
    if (g_failures == 0) {
        std::cout << "All checks passed." << std::endl;
        return 0;
    }
    std::cout << g_failures << " check(s) failed." << std::endl;
    return 1;
}
