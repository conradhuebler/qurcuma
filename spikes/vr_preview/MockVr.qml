// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Stand-in for VrSession (src/vrsession.h) in the offline preview (Claude Generated 2026): the same
// properties, the command() signal and the file browser calls, with a small fake directory tree and
// enough behaviour that the panels can be clicked through. stateName picks a starting state.
import QtQuick

QtObject {
    id: mock

    property string stateName: "idle"

    // ---- VrSession properties -------------------------------------------------------------------
    property string statusText: ""
    property bool simulationActive: false
    property bool simulationPaused: false
    property string simulationMessage: ""
    property string simulationMode: ""
    property string structureFile: "caffeine.xyz"
    property string structureTitle: "Caffeine"
    property double temperature: 300
    property double systemTemperature: 0
    property double temperatureMin: 1
    property double temperatureMax: 1000
    property string method: "gfnff"
    property int frame: 0
    property int frameCount: 1
    property bool playing: false
    property string selectionText: ""
    property double grabStrength: 0.1
    property string currentDir: "/home/user/structures"
    property var entries: []
    property bool handInput: stateName === "hands"   // layout within arm's reach (Claude Generated 2026)

    // ---- VrSession signals (callable from the panels, as on the C++ object) ---------------------
    signal command(string name)
    signal atomPicked(int atomIndex)
    signal fileRequested(string path)
    signal grabForce(int source, int atomIndex, vector3d displacement)
    signal grabReleased(int source)

    readonly property var tree: ({
        "/home/user": ["structures/"],
        "/home/user/structures": ["conformers/", "lessons/", "benzene_dimer.mol2", "caffeine.xyz",
            "conf_28.xyz", "ethanol_md.vtf", "haber_bosch_box.xyz", "peptide_1l2y.pdb",
            "water_cluster_20.xyz", "a_rather_long_trajectory_name_from_a_parameter_scan_2026.vtf"],
        "/home/user/structures/conformers": ["conf_01.xyz", "conf_02.xyz", "conf_03.xyz"],
        "/home/user/structures/lessons": []
    })

    function list(dir) {
        var out = []
        var names = tree[dir] || []
        for (var i = 0; i < names.length; ++i) {
            var isDir = names[i].endsWith("/")
            out.push({ name: isDir ? names[i].slice(0, -1) : names[i], isDir: isDir })
        }
        return out
    }
    function setDir(dir) {
        entries = list(dir)   // VrSession emits currentDirChanged for both
        currentDir = dir
    }
    function enter(name) {
        if (tree[currentDir + "/" + name] !== undefined)
            setDir(currentDir + "/" + name)
    }
    function up() {
        var cut = currentDir.lastIndexOf("/")
        if (cut > 0)
            setDir(currentDir.slice(0, cut))
    }
    function choose(name) {
        console.log("preview: open", currentDir + "/" + name)
        structureFile = name; structureTitle = ""
        fileRequested(currentDir + "/" + name)
    }

    onCommand: (name) => {
        console.log("preview: command", name)
        if (name === "playPause") playing = frameCount > 1 && !playing
        else if (name === "nextFrame") frame = (frame + 1) % frameCount
        else if (name === "previousFrame") frame = (frame + frameCount - 1) % frameCount
        else if (name === "grabUp") grabStrength *= 1.25
        else if (name === "grabDown") grabStrength *= 0.8
        else if (name === "simStartMD" || name === "simStartOpt") {
            simulationActive = true; simulationPaused = false; simulationMessage = ""
            simulationMode = name === "simStartMD" ? "md" : "opt"
        } else if (name === "simPause") simulationPaused = !simulationPaused
        else if (name === "simStop") {
            simulationActive = false; simulationPaused = false; simulationMode = ""
            simulationMessage = "Finished: stopped by the user."
        } else if (name.indexOf("simMethod:") === 0) {
            if (!simulationActive) method = name.substring(10)
        } else if (name.indexOf("simTempRange:") === 0) {
            var range = name.split(":")
            temperatureMin = Number(range[1]); temperatureMax = Number(range[2])
            temperature = Math.max(temperatureMin, Math.min(temperatureMax, temperature))
        } else if (name.indexOf("simTemp:") === 0) {
            temperature = Math.max(temperatureMin, Math.min(temperatureMax, Number(name.slice(8))))
        }
    }

    Component.onCompleted: {
        setDir("/home/user/structures")
        if (stateName === "trajectory" || stateName === "files") {
            frameCount = 240; frame = 41; selectionText = "C #12 (+2)"
        } else if (stateName === "running" || stateName === "paused") {
            simulationActive = true; simulationPaused = stateName === "paused"; simulationMode = "md"
            temperature = 350; systemTemperature = 342; selectionText = "N #3"; grabStrength = 0.156
        } else if (stateName === "aborted") {
            simulationMessage = "Aborted: the temperature ran away (above 5000 K)."
        }
    }
}
