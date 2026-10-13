/*
 * <Qurcuma VR session: mirrors the viewer scene into an OpenXR headset.>
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
#include <QVariantList>
#include <QVector3D>
#include <QQmlEngine>

#include <memory>

class SceneController;

/**
 * @brief OpenXR session showing the current molecule in a headset (Claude Generated 2026).
 *
 * Owns a second SceneController (instancing buffers belong to one Quick 3D scene, so the
 * desktop controller cannot be shared) and a QML engine hosting src/qml/viewerxr.qml
 * (Qt Quick 3D XR). The viewer mirrors structure, positions and selection into scene();
 * a controller-ray pick in the headset arrives as atomPicked().
 */
class VrSession : public QObject {
    Q_OBJECT
    /// Text shown on the in-headset panel (frame, selection, grab strength).
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    /// True while an interactive simulation runs; only then a trigger pull on an atom grabs it.
    Q_PROPERTY(bool simulationActive READ simulationActive NOTIFY simulationActiveChanged)
    /// Thermostat target (K) of the Simulation dock; 0 = unknown.
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
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
public:
    explicit VrSession(QObject* parent = nullptr);
    ~VrSession() override;

    /// Create the XR view. Returns false if the QML could not be loaded.
    bool start();
    /// Tear the XR view down (ends the OpenXR session).
    void stop();
    bool isActive() const { return m_engine != nullptr; }

    /// Scene the headset renders; feed it like the desktop SceneController.
    SceneController* scene() const { return m_scene; }

    QString statusText() const { return m_statusText; }
    void setStatusText(const QString& text);
    bool simulationActive() const { return m_simulationActive; }
    void setSimulationActive(bool on);
    double temperature() const { return m_temperature; }
    void setTemperature(double kelvin);
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
    /// Instance index of the atom hit by the controller ray (-1 = nothing hit).
    void atomPicked(int atomIndex);
    /// The OpenXR session ended (headset removed, runtime stopped, or stop()).
    void ended();
    /// Grab in progress: displacement (Angstrom, model-local) from the atom to the controller ray's
    /// target point at the atom's depth. Re-sent while the trigger is held.
    void grabForce(int atomIndex, QVector3D displacement);
    void grabReleased();
    /// Button on the in-headset panel: "playPause", "nextFrame", "previousFrame", "grabUp", "grabDown",
    /// and the simulation buttons "simStartMD", "simStartOpt", "simStop", "simPause", "simTempUp", "simTempDown", "simMethod:<key>".
    void command(const QString& name);
    void statusTextChanged();
    void temperatureChanged();
    void methodChanged();
    void currentDirChanged();
    /// Absolute path of a structure file picked in the headset file browser.
    void fileRequested(const QString& path);
    void simulationActiveChanged();
    void statusChanged();
    void simulationStatusChanged();

private:
    SceneController* m_scene = nullptr;
    std::unique_ptr<QQmlEngine> m_engine;
    QObject* m_root = nullptr;
    QString m_statusText;
    bool m_simulationActive = false;
    double m_temperature = 0.0;
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

    void refreshEntries();
};
