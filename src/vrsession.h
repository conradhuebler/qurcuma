/*
 * <Qurcuma VR session: shows the viewer scene in an OpenXR headset.>
 * Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVector3D>
#include <QQmlEngine>

#include <memory>

class SceneController;
class XrSceneBridge;

/**
 * @brief OpenXR session showing the current molecule in a headset (Claude Generated 2026).
 *
 * Renders the desktop viewer's SceneController: a QML engine hosts src/qml/viewerxr.qml
 * (Qt Quick 3D XR) with "controller" = that controller (flags, colours, bounds, atom
 * positions) and "xrInstancing" = an XrSceneBridge, which hands the controller's instance
 * tables to the headset scene as MirrorInstancing objects. Nothing is computed twice; the
 * headset keeps its own molecule rotation. A controller-ray pick arrives as atomPicked().
 */
class VrSession : public QObject {
    Q_OBJECT
    /// Text shown on the in-headset panel (frame, selection, grab strength).
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    /// True while an interactive simulation runs; only then a trigger pull on an atom grabs it.
    Q_PROPERTY(bool simulationActive READ simulationActive NOTIFY simulationActiveChanged)
    /// Thermostat target (K) of the Simulation dock; 0 = unknown.
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    /// Instantaneous temperature (K) of the running MD, as in the Temperature chart; 0 = none yet
    /// (no MD since the run started, or an optimization). Claude Generated 2026.
    Q_PROPERTY(double systemTemperature READ systemTemperature NOTIFY systemTemperatureChanged)
    /// Range of the Simulation dock's temperature slider, shared by the headset slider (Claude Generated 2026).
    Q_PROPERTY(double temperatureMin READ temperatureMin NOTIFY temperatureRangeChanged)
    Q_PROPERTY(double temperatureMax READ temperatureMax NOTIFY temperatureRangeChanged)
    /// Method key of the Simulation dock ("gfnff", "uff", "gfn2", "gfn1").
    Q_PROPERTY(QString method READ method NOTIFY methodChanged)
    /// File browser: directory shown and its entries ({name, isDir}), directories first.
    Q_PROPERTY(QString currentDir READ currentDir NOTIFY currentDirChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY currentDirChanged)
    /// Structured status for the panels (Claude Generated 2026): frame (0-based) of frameCount,
    /// trajectory playback, selected atom(s) ("C #12 (+2)", empty = none) and grab strength.
    Q_PROPERTY(int frame READ frame NOTIFY statusChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY statusChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY statusChanged)
    Q_PROPERTY(QString selectionText READ selectionText NOTIFY statusChanged)
    Q_PROPERTY(double grabStrength READ grabStrength NOTIFY statusChanged)
    /// Run paused (while simulationActive) and the last run's end line ("Finished: ..." / "Aborted: ...",
    /// empty while a run is active or before the first one).
    Q_PROPERTY(bool simulationPaused READ simulationPaused NOTIFY simulationStatusChanged)
    Q_PROPERTY(QString simulationMessage READ simulationMessage NOTIFY simulationStatusChanged)
    /// Kind of the active run (Claude Generated 2026): "md", "opt", empty while none runs; lights the
    /// MD or Opt button of the Simulation panel.
    Q_PROPERTY(QString simulationMode READ simulationMode NOTIFY simulationStatusChanged)
    /// Structure shown (Claude Generated 2026): file name (empty = none, e.g. a new scene) and the
    /// molecule's name from the file (XYZ comment, PDB TITLE/COMPND, MOL2 name; empty if it has none).
    Q_PROPERTY(QString structureFile READ structureFile NOTIFY structureChanged)
    Q_PROPERTY(QString structureTitle READ structureTitle NOTIFY structureChanged)
    /// Surroundings (Claude Generated 2026): 360 degree panorama (equirectangular image) shown as the
    /// sky box, and a 3D scene file (glTF/GLB/OBJ, metres) around the user; empty = none.
    Q_PROPERTY(QUrl panorama READ panorama NOTIFY surroundingsChanged)
    Q_PROPERTY(QUrl surroundings READ surroundings NOTIFY surroundingsChanged)
    /// Centimetres per unit of the surroundings file: 100 = metres (glTF convention), 1 = centimetres.
    Q_PROPERTY(double surroundingsScale READ surroundingsScale NOTIFY surroundingsChanged)
    /// Input mode (Claude Generated 2026): false = controllers (ray + trigger, viewerxr.qml Hand),
    /// true = tracked hands (pinch, direct manipulation, VrHandInput.qml). Written by the desktop menu
    /// and by the headset's View panel.
    Q_PROPERTY(bool handInput READ handInput WRITE setHandInput NOTIFY handInputChanged)
public:
    /// Grab sources of the two hands (controllers or tracked hands); 0 is the desktop mouse.
    static constexpr int RightGrabSource = 1;
    static constexpr int LeftGrabSource = 2;

    /// @p scene is the desktop viewer's controller; it must outlive the session (or stop() first).
    explicit VrSession(SceneController* scene, QObject* parent = nullptr);
    ~VrSession() override;

    /// Create the XR view. Returns false if the QML could not be loaded.
    bool start();
    /// Tear the XR view down (ends the OpenXR session): QML engine first, then the bridge.
    void stop();
    bool isActive() const { return m_engine != nullptr; }

    QString statusText() const { return m_statusText; }
    void setStatusText(const QString& text);
    bool simulationActive() const { return m_simulationActive; }
    void setSimulationActive(bool on);
    double temperature() const { return m_temperature; }
    void setTemperature(double kelvin);
    double systemTemperature() const { return m_systemTemperature; }
    void setSystemTemperature(double kelvin);
    double temperatureMin() const { return m_temperatureMin; }
    double temperatureMax() const { return m_temperatureMax; }
    void setTemperatureRange(double minimum, double maximum);
    QString method() const { return m_method; }
    void setMethod(const QString& key);

    int frame() const { return m_frame; }
    int frameCount() const { return m_frameCount; }
    bool playing() const { return m_playing; }
    QString selectionText() const { return m_selectionText; }
    double grabStrength() const { return m_grabStrength; }
    /// Update the structured panel status; emits statusChanged() only on a change (Claude Generated 2026).
    void setStatus(int frame, int frameCount, bool playing, const QString& selection, double grabStrength);
    bool simulationPaused() const { return m_simulationPaused; }
    QString simulationMessage() const { return m_simulationMessage; }
    /// Pause state and end line of the run (Claude Generated 2026).
    void setSimulationStatus(bool paused, const QString& message);
    QString simulationMode() const { return m_simulationMode; }
    void setSimulationMode(const QString& mode);
    QString structureFile() const { return m_structureFile; }
    QString structureTitle() const { return m_structureTitle; }
    void setStructure(const QString& fileName, const QString& title);

    QUrl panorama() const { return m_panorama; }
    QUrl surroundings() const { return m_surroundings; }
    double surroundingsScale() const { return m_surroundingsScale; }
    /// Local files for the sky box panorama and the surroundings scene (empty paths clear them), and
    /// the scene's units in centimetres.
    void setSurroundings(const QString& panoramaPath, const QString& scenePath, double sceneScale);

    bool handInput() const { return m_handInput; }
    void setHandInput(bool on);

    QString currentDir() const { return m_currentDir; }
    QVariantList entries() const { return m_entries; }
    /// Directory the browser opens in (falls back to the home directory).
    void setStartDirectory(const QString& dir);
    /// Descend into a sub-directory of currentDir (from the headset panel).
    Q_INVOKABLE void enter(const QString& name);
    Q_INVOKABLE void up();
    /// Ask the application to open a structure file of currentDir.
    Q_INVOKABLE void choose(const QString& name);

signals:
    /// Atom hit by the controller ray (atom index, mapped from the instance row; -1 = nothing hit).
    void atomPicked(int atomIndex);
    /// The OpenXR session ended (headset removed, runtime stopped, or stop()).
    void ended();
    /// Grab in progress: displacement (Angstrom, model-local) from the atom to the controller ray's
    /// target point at the atom's depth. Re-sent while the trigger is held. @p source is the hand
    /// (RightGrabSource / LeftGrabSource): both hands may pull at once (Claude Generated 2026).
    void grabForce(int source, int atomIndex, QVector3D displacement);
    void grabReleased(int source);
    /// Button on the in-headset panel: "playPause", "nextFrame", "previousFrame", "grabUp", "grabDown",
    /// and the simulation buttons "simStartMD", "simStartOpt", "simStop", "simPause", "simMethod:<key>", "simTemp:<K>" (thermostat target)
    /// and "simTempRange:<min>:<max>" (the temperature slider's range, K).
    void command(const QString& name);
    void statusTextChanged();
    void temperatureChanged();
    void systemTemperatureChanged();
    void temperatureRangeChanged();
    void methodChanged();
    void currentDirChanged();
    /// Absolute path of a structure file picked in the headset file browser.
    void fileRequested(const QString& path);
    void simulationActiveChanged();
    void statusChanged();
    void simulationStatusChanged();
    void surroundingsChanged();
    void handInputChanged();
    void structureChanged();

private:
    SceneController* m_scene = nullptr;      // desktop controller, not owned
    XrSceneBridge* m_instancing = nullptr;   // mirrors of m_scene's instance tables, owned
    std::unique_ptr<QQmlEngine> m_engine;
    QObject* m_root = nullptr;
    QString m_statusText;
    bool m_simulationActive = false;
    double m_temperature = 0.0;
    double m_systemTemperature = 0.0;
    double m_temperatureMin = 1.0;
    double m_temperatureMax = 1000.0;
    QString m_method;
    QString m_currentDir;
    QVariantList m_entries;
    int m_frame = 0;
    int m_frameCount = 0;
    bool m_playing = false;
    QString m_selectionText;
    double m_grabStrength = 0.0;
    bool m_simulationPaused = false;
    QString m_simulationMessage;
    QString m_simulationMode;
    QString m_structureFile;
    QString m_structureTitle;
    QUrl m_panorama;
    QUrl m_surroundings;
    double m_surroundingsScale = 100.0;
    bool m_handInput = false;

    void refreshEntries();
};
