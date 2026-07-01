// viewer.cpp — Qt Quick 3D backed MoleculeViewer (renderer migration from Qt3D).
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// The public API matches the former Qt3D MoleculeViewer exactly; internally the
// scene is an embedded QQuickView (src/qml/viewer3d.qml) driven by SceneController.
// Mouse/picking/grab are handled in C++ (eventFilter) and applied to the controller.
// Claude Generated 2026.
#include "view.h"

#include "bondeditor.h"
#include "elementdata.h"

#include "src/core/elements.h"
#include "forceinjector.h"
#include "performanceoptimizer.h"
#include "scenecontroller.h"
#include "selectionmanager.h"
#include "xyzparser.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSpinBox>
#include <QMouseEvent>
// Offscreen high-resolution image export (QQuickRenderControl + QRhi).
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QSGRendererInterface>
#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif
#include <rhi/qrhi.h>
#include <QPushButton>
#include <QSet>
#include <QQmlContext>
#include <QQuickView>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>

MoleculeViewer::MoleculeViewer(QWidget* parent)
    : QWidget(parent)
{
    m_selectionManager = new SelectionManager(this);
    m_bondEditor = new BondEditor(this);
    m_perfOpt = new PerformanceOptimizer(this);

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(500);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MoleculeViewer::onAutoSaveTimer);
    connect(m_bondEditor, &BondEditor::structureChanged, this, &MoleculeViewer::onStructureChanged);

    setupViewer();
    // MeasurementOverlay (Qt3D) is not constructed; the Quick3D port lands in M2.
}

MoleculeViewer::~MoleculeViewer() = default;

void MoleculeViewer::setupViewer()
{
    m_scene = new SceneController(this);

    m_quickView = new QQuickView();
    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_quickView->rootContext()->setContextProperty(QStringLiteral("controller"), m_scene);
    m_quickView->setSource(QUrl(QStringLiteral("qrc:/qml/src/qml/viewer3d.qml")));
    if (m_quickView->status() == QQuickView::Error) {
        for (const QQmlError& e : m_quickView->errors())
            qWarning() << "viewer3d.qml:" << e.toString();
    }

    m_container = QWidget::createWindowContainer(m_quickView, this);
    m_container->setMouseTracking(true);
    m_container->setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupControlPanel();
    layout->addWidget(m_controlPanel);
    layout->addWidget(m_container);

    applyAppearanceToController();

    // Mouse events arrive on the QQuickView (a QWindow), like the former Qt3DWindow.
    m_quickView->installEventFilter(this);
}

void MoleculeViewer::applyAppearanceToController()
{
    if (!m_scene)
        return;
    m_scene->setBackgroundColor(m_backgroundColor);
    m_scene->setRenderingMode(static_cast<int>(m_renderingMode));
    m_scene->setColorScheme(static_cast<int>(m_colorScheme));
    m_scene->setAtomScaleFactor(m_atomScaleFactor);
    m_scene->setBondThickness(m_bondThickness);
    m_scene->setAtomTransparency(m_atomTransparency);
    m_scene->setSsao(m_ssaoEnabled, m_ssaoIntensity);
    m_scene->setBloom(m_bloomEnabled);
    m_scene->setHdr(m_hdrEnabled);
    m_scene->setExposure(m_exposure);
    m_scene->setFog(m_fogEnabled, m_fogIntensity);
    m_scene->setFogDistance(m_fogDistance);
    for (int i = 0; i < 4; ++i)
        m_scene->setCornerLight(i, m_cornerLightEnabled[i]);
}

// ---------------------------------------------------------------------------
// Mouse interaction (drives the SceneController transform / grab)
// ---------------------------------------------------------------------------
bool MoleculeViewer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_quickView) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_leftMousePressed = true;
                m_lastMousePos = me->position().toPoint();
                m_leftPressPos = m_lastMousePos;
                m_leftDragged = false;
                m_movingSelection = false;
                m_rubberBanding = false;
                m_emptyPressPending = false;
                if (m_editMode && !m_simulationActive) {
                    // Edit mode: press on a selected/picked atom starts a move; press on
                    // empty space clears selection on release (a plain click) or rotates.
                    const int picked = pickAtomAtScreenPos(m_lastMousePos);
                    const bool append = me->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
                    if (picked >= 0) {
                        if (!m_selectedAtoms.contains(picked))
                            selectAtom(picked, append);
                        m_movingSelection = true;
                        m_moveSnapshotTaken = false;  // snapshot lazily on first drag
                        m_moveRefLocal = selectionCentroidLocal();
                        m_dragAnchorGlobal = me->globalPosition().toPoint();  // cursor-lock pin
                        if (m_quickView) m_quickView->setCursor(Qt::ClosedHandCursor);
                        if (m_container) m_container->setCursor(Qt::ClosedHandCursor);
                    } else if (append) {
                        // Ctrl/Shift + drag on empty space = rubber-band (box) select.
                        m_rubberBanding = true;
                        m_rubberStart = m_lastMousePos;
                        if (m_scene)
                            m_scene->setRubberBand(QRectF(m_rubberStart, m_rubberStart), true);
                    } else {
                        m_emptyPressPending = true;  // plain click clears; plain drag rotates
                    }
                    return true;
                }
                if (m_simulationActive) {
                    int picked = pickAtomAtScreenPos(m_lastMousePos);
                    if (picked >= 0) {
                        m_grabbedAtom = picked;
                        if (m_quickView) m_quickView->setCursor(Qt::ClosedHandCursor);
                        if (m_container) m_container->setCursor(Qt::ClosedHandCursor);
                    }
                }
                return true;
            } else if (me->button() == Qt::RightButton) {
                m_rightMousePressed = true;
                m_lastMousePos = me->position().toPoint();
                m_rightPressPos = m_lastMousePos;
                m_rightDragged = false;
                return true;
            } else if (me->button() == Qt::MiddleButton) {
                resetView();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_leftMousePressed = false;
                if (m_editMode && !m_simulationActive) {
                    if (m_quickView) m_quickView->setCursor(Qt::ArrowCursor);
                    if (m_container) m_container->setCursor(Qt::ArrowCursor);
                    if (m_rubberBanding) {
                        m_rubberBanding = false;
                        if (m_scene) {
                            const QRectF r = QRectF(m_rubberStart, me->position().toPoint()).normalized();
                            const float w = m_quickView ? m_quickView->width() : 1.0f;
                            const float h = m_quickView ? m_quickView->height() : 1.0f;
                            const QVector<int> hits = m_scene->atomsInScreenRect(r, w, h);
                            if (!hits.isEmpty())
                                selectAtoms(hits, /*append=*/true);  // box-select adds
                            m_scene->setRubberBand(QRectF(), false);
                        }
                        computeCollisions();
                        m_emptyPressPending = false;
                        return true;
                    }
                    if (m_movingSelection) {
                        m_movingSelection = false;
                        if (m_leftDragged)
                            finalizeEdit();  // re-detect bonds + recheck clashes after a move
                    } else if (m_emptyPressPending && !m_leftDragged) {
                        clearSelection();      // plain click on empty space
                        computeCollisions();
                    }
                    m_emptyPressPending = false;
                    return true;
                }
                if (m_grabbedAtom >= 0) {
                    m_grabbedAtom = -1;
                    emit atomGrabReleased();
                    if (m_scene) m_scene->setForceArrows({}); // clear arrows on release
                    Qt::CursorShape shape = m_simulationActive ? Qt::SizeAllCursor : Qt::ArrowCursor;
                    if (m_quickView) m_quickView->setCursor(shape);
                    if (m_container) m_container->setCursor(shape);
                } else if (!m_leftDragged) {
                    // A click (no drag): select / measure / bond-edit by mode.
                    const int picked = pickAtomAtScreenPos(me->position().toPoint());
                    const bool append = me->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
                    if (picked < 0) {
                        if (!append)
                            clearSelection();
                    } else if (m_bondEditMode != 0) {
                        // Accumulate an atom pair, then apply the edit.
                        selectAtom(picked, /*append=*/true);
                        if (m_selectedAtoms.size() >= 2) {
                            performBondEdit(m_selectedAtoms[0], m_selectedAtoms[1]);
                            clearSelection();
                        }
                    } else if (m_measurementMode != 0) {
                        // Click marks an atom; clicking a marked atom de-marks it. Type is
                        // auto-detected from the count (2/3/4); a 5th new pick restarts.
                        if (m_selectedAtoms.contains(picked)) {
                            m_selectedAtoms.removeAll(picked);
                        } else {
                            if (m_selectedAtoms.size() >= 4)
                                m_selectedAtoms.clear();
                            m_selectedAtoms.append(picked);
                        }
                        if (m_selectionManager) {
                            m_selectionManager->clearSelection();
                            for (int a : m_selectedAtoms)
                                m_selectionManager->selectAtom(a, true);
                        }
                        if (m_scene)
                            m_scene->setSelection(m_selectedAtoms);
                        emit selectionChanged(m_selectedAtoms);
                        updateMeasurement();
                    } else {
                        selectAtom(picked, append);
                    }
                }
                return true;
            } else if (me->button() == Qt::RightButton) {
                m_rightMousePressed = false;
                // Right-click (no pan-drag) clears the selection / measurement marks.
                if (!m_rightDragged && !m_selectedAtoms.isEmpty()) {
                    clearSelection();
                    if (m_editMode)
                        computeCollisions();
                }
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint pos = me->position().toPoint();
            if (m_editMode && !m_simulationActive && m_leftMousePressed && m_movingSelection) {
                const QPoint d = pos - m_lastMousePos;
                if (d.isNull())
                    return true;  // absorbs the synthetic move from a cursor-lock warp
                if ((pos - m_leftPressPos).manhattanLength() > 3)
                    m_leftDragged = true;
                if (m_leftDragged && !m_moveSnapshotTaken) {
                    emit editSnapshotRequested(tr("Before move"));  // pre-move state for undo
                    m_moveSnapshotTaken = true;
                }
                const bool depth = me->modifiers() & Qt::ShiftModifier;
                if (m_scene) {
                    const float w = m_quickView ? m_quickView->width() : 1.0f;
                    const float h = m_quickView ? m_quickView->height() : 1.0f;
                    const QVector3D delta = depth
                        ? m_scene->screenDragToModelDelta(0, 0, d.y(), m_moveRefLocal, w, h)
                        : m_scene->screenDragToModelDelta(d.x(), d.y(), 0, m_moveRefLocal, w, h);
                    moveSelection(delta);
                    m_moveRefLocal = selectionCentroidLocal();  // keep depth scale current
                }
                if (m_dragCursorLock) {
                    // Pin the cursor at the press point: warp it back so the drag never
                    // runs off-screen and motion is relative (the zero-delta guard above
                    // absorbs the synthetic move this warp generates).
                    QCursor::setPos(m_dragAnchorGlobal);
                    m_lastMousePos = m_leftPressPos;
                } else {
                    m_lastMousePos = pos;
                }
                return true;
            }
            if (m_editMode && !m_simulationActive && m_leftMousePressed && m_rubberBanding) {
                m_leftDragged = true;
                if (m_scene)
                    m_scene->setRubberBand(QRectF(m_rubberStart, pos).normalized(), true);
                m_lastMousePos = pos;
                return true;
            }
            if (m_grabbedAtom >= 0 && m_leftMousePressed) {
                m_lastMousePos = pos;
                QVector3D modelLocalForce = computeGrabForce(pos, m_grabbedAtom);
                // curcuma adds force to the gradient; accel = -gradient, so negate
                // to make the atom follow the cursor.
                emit atomForceRequested(m_grabbedAtom, -modelLocalForce, m_grabAlpha, m_grabMaxShells);
                updateForceVectors();
                return true;
            }
            if (m_leftMousePressed) {
                if ((pos - m_leftPressPos).manhattanLength() > 3)
                    m_leftDragged = true;
                handleMouseRotation(pos);
                m_lastMousePos = pos;
                return true;
            } else if (m_rightMousePressed) {
                if ((pos - m_rightPressPos).manhattanLength() > 3)
                    m_rightDragged = true;
                handleMousePan(pos);
                m_lastMousePos = pos;
                return true;
            }
            // No button: hover feedback — highlight the atom under the cursor.
            {
                const int hov = pickAtomAtScreenPos(pos);
                if (m_scene)
                    m_scene->setHoverAtom(hov);
                const Qt::CursorShape shape = (hov >= 0)
                    ? Qt::PointingHandCursor
                    : (m_simulationActive ? Qt::SizeAllCursor : Qt::ArrowCursor);
                if (m_quickView) m_quickView->setCursor(shape);
                if (m_container) m_container->setCursor(shape);
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && m_editMode && !m_simulationActive) {
                // Double-click selects the whole connected molecule (fragment).
                const int picked = pickAtomAtScreenPos(me->position().toPoint());
                const bool append = me->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
                if (picked >= 0) {
                    selectFragment(picked, append);
                    m_movingSelection = false;  // a subsequent press starts the move
                    m_moveRefLocal = selectionCentroidLocal();
                    computeCollisions();
                }
                return true;
            }
            break;
        }
        case QEvent::Wheel: {
            auto* we = static_cast<QWheelEvent*>(event);
            handleMouseZoom(we->angleDelta().y());
            return true;
        }
        case QEvent::Leave: {
            if (m_scene)
                m_scene->setHoverAtom(-1); // drop hover highlight when leaving the view
            if (m_grabbedAtom < 0) {
                Qt::CursorShape shape = m_simulationActive ? Qt::SizeAllCursor : Qt::ArrowCursor;
                if (m_quickView) m_quickView->setCursor(shape);
                if (m_container) m_container->setCursor(shape);
            }
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MoleculeViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void MoleculeViewer::handleMouseRotation(const QPoint& currentPos)
{
    const QPoint delta = currentPos - m_lastMousePos;
    const float sens = 0.5f;
    applyModelRotation(delta.x() * sens, -delta.y() * sens);
}

// Claude Generated 2026 - incremental model rotation about the view axes (right=+X,
// up=+Y). Used by mouse drag and by keyboard rotation in Edit mode (regains the depth
// degree of freedom that a 2D in-plane move alone cannot reach).
void MoleculeViewer::applyModelRotation(float horizDeg, float vertDeg, float rollDeg)
{
    if (!m_scene)
        return;
    const QQuaternion horiz = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), horizDeg);
    const QQuaternion vert = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), vertDeg);
    const QQuaternion roll = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), rollDeg);
    m_modelRotation = (horiz * vert * roll) * m_modelRotation;
    m_modelRotation.normalize();
    m_scene->setRootRotation(m_modelRotation);
}

// Claude Generated 2026 - WASD/QE scene rotation (W/S pitch, A/D yaw, Q/E roll). With
// @p nudge (Shift) in Edit mode, the same keys translate the selection in the view
// plane (Q/E = depth). Called from MainWindow's app-level key filter so it works no
// matter which widget has focus.
void MoleculeViewer::rotateSceneByKey(int key, bool nudge)
{
    if (nudge && m_editMode && !m_selectedAtoms.isEmpty() && m_scene) {
        QVector3D worldStep;
        switch (key) {
        case Qt::Key_W: worldStep = QVector3D(0, kNudgeStep, 0); break;
        case Qt::Key_S: worldStep = QVector3D(0, -kNudgeStep, 0); break;
        case Qt::Key_A: worldStep = QVector3D(-kNudgeStep, 0, 0); break;
        case Qt::Key_D: worldStep = QVector3D(kNudgeStep, 0, 0); break;
        case Qt::Key_Q: worldStep = QVector3D(0, 0, kNudgeStep); break;   // toward viewer
        case Qt::Key_E: worldStep = QVector3D(0, 0, -kNudgeStep); break;
        default: return;
        }
        moveSelection(m_scene->rootRotation().inverted().rotatedVector(worldStep));
        return;
    }
    constexpr float step = 12.0f;  // degrees per keypress
    switch (key) {
    case Qt::Key_A: applyModelRotation(-step, 0, 0); break;  // yaw left
    case Qt::Key_D: applyModelRotation(step, 0, 0); break;   // yaw right
    case Qt::Key_W: applyModelRotation(0, step, 0); break;   // pitch up
    case Qt::Key_S: applyModelRotation(0, -step, 0); break;  // pitch down
    case Qt::Key_Q: applyModelRotation(0, 0, -step); break;  // roll left
    case Qt::Key_E: applyModelRotation(0, 0, step); break;   // roll right
    default: break;
    }
}

void MoleculeViewer::handleMousePan(const QPoint& currentPos)
{
    if (!m_scene)
        return;
    const QPoint delta = currentPos - m_lastMousePos;
    const float sens = m_scene->cameraDistance() * 0.005f;
    // right=+X, up=+Y; screen Y is down so +delta.y moves content down -> camera up.
    const QVector3D panDelta(-delta.x() * sens, delta.y() * sens, 0.0f);
    m_scene->setPan(m_scene->pan() + panDelta);
}

void MoleculeViewer::handleMouseZoom(int delta)
{
    if (!m_scene)
        return;
    const float factor = (delta > 0) ? 0.9f : 1.1f;
    m_scene->setCameraDistance(m_scene->cameraDistance() * factor);
}

int MoleculeViewer::pickAtomAtScreenPos(const QPoint& screenPos) const
{
    if (!m_scene || !m_quickView)
        return -1;
    return m_scene->pickAtom(screenPos.x(), screenPos.y(),
        m_quickView->width(), m_quickView->height());
}

QVector3D MoleculeViewer::computeGrabForce(const QPoint& mousePos, int atomIndex) const
{
    if (!m_scene || !m_quickView)
        return QVector3D();
    return m_scene->computeGrabForce(mousePos.x(), mousePos.y(), atomIndex,
        m_quickView->width(), m_quickView->height(), m_grabStrength);
}

QVector3D MoleculeViewer::modelToWorld(const QVector3D& localPos) const
{
    return m_scene ? m_scene->modelToWorld(localPos) : localPos;
}

// ---------------------------------------------------------------------------
// Force-vector overlay (opt-in)
// ---------------------------------------------------------------------------
void MoleculeViewer::setForceVectorsVisible(bool on)
{
    m_forceVectorsVisible = on;
    if (m_scene)
        m_scene->setForceVectorsVisible(on);
    if (on)
        updateForceVectors();
}

// ---------------------------------------------------------------------------
// Confinement-wall overlay (curcuma harmonic walls, driven by Simulation config)
// ---------------------------------------------------------------------------
// Claude Generated 2026 - The wireframe is shown only when both the config has
// walls enabled AND the Display-panel override is on. Geometry is built in
// intrinsic atom coordinates and lives under moleculeRoot (rotates with the
// molecule). curcuma auto-sizes walls when rect bounds or radius are 0; those
// auto-sized values are not exposed back, so we only draw explicit (non-zero)
// bounds/radius — silent otherwise, never wrong geometry.
void MoleculeViewer::setConfinementBox(bool on, int type,
    const QVector3D& min, const QVector3D& max, float radius)
{
    m_wallEnabled = on;
    m_wallType = type;
    m_wallMin = min;
    m_wallMax = max;
    m_wallRadius = radius;
    applyWallVisibility();
}

void MoleculeViewer::setWallVisibleOverride(bool on)
{
    m_wallVisibleOverride = on;
    applyWallVisibility();
}

// Claude Generated 2026 - Forward the Display-panel opacity slider to the
// scene controller (the QML material binds to controller.wallOpacity).
void MoleculeViewer::setWallOpacity(qreal opacity)
{
    if (m_scene)
        m_scene->setWallOpacity(opacity);
}

qreal MoleculeViewer::getWallOpacity() const
{
    return m_scene ? m_scene->wallOpacity() : 1.0;
}

void MoleculeViewer::setWallPotentialViz(bool enabled)
{
    m_potVizEnabled = enabled;
    if (m_scene)
        m_scene->setWallPotentialViz(enabled, m_wallHarmonic, m_wallTemp, m_wallBeta);
}

void MoleculeViewer::setWallPotentialParams(bool harmonic, double wallTemp, float wallBeta)
{
    m_wallHarmonic = harmonic;
    m_wallTemp = wallTemp;
    m_wallBeta = wallBeta;
    // Always update stored params in SceneController (handles both shells and arrows).
    if (m_scene)
        m_scene->setWallPotentialViz(m_potVizEnabled, harmonic, wallTemp, wallBeta);
}

void MoleculeViewer::setWallVectorField(bool enabled, int resolution)
{
    m_potArrowsEnabled = enabled;
    m_potArrowResolution = resolution;
    if (m_scene)
        m_scene->setWallVectorField(enabled, resolution);
}

void MoleculeViewer::applyWallVisibility()
{
    if (!m_scene)
        return;
    if (!m_wallEnabled || !m_wallVisibleOverride) {
        m_scene->setWallVisible(false);
        return;
    }
    if (m_wallType == 2) {
        // Rectangular: needs explicit, ordered bounds to draw a real cuboid.
        const bool valid = m_wallMax.x() > m_wallMin.x()
            && m_wallMax.y() > m_wallMin.y()
            && m_wallMax.z() > m_wallMin.z();
        if (valid)
            m_scene->setWallBox(m_wallMin, m_wallMax);
        else
            m_scene->setWallVisible(false);
    } else if (m_wallType == 1) {
        if (m_wallRadius > 0.0f)
            m_scene->setWallSphere(m_wallRadius);
        else
            m_scene->setWallVisible(false);
    } else {
        m_scene->setWallVisible(false);
    }
    computeWallViolations();  // recolour box + emit count for the current frame
}

// Claude Generated 2026 - Count atoms of the current frame outside the
// configured wall region (rect: any axis outside [min,max]; spheric: |r| >
// radius), recolour the wireframe red on violations, and emit the count so the
// Simulation widget can show a live "N atoms outside" status. No-op when walls
// are disabled/hidden or no molecule is loaded.
void MoleculeViewer::computeWallViolations()
{
    if (!m_wallEnabled || !m_wallVisibleOverride || !m_scene) {
        if (m_wallViolationCount != 0) {
            m_wallViolationCount = 0;
            emit wallViolationChanged(0);
        }
        return;
    }
    const int frame = (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size())
        ? m_currentFrame : (m_trajectoryAtoms.isEmpty() ? -1 : 0);
    if (frame < 0) {
        if (m_wallViolationCount != 0) {
            m_wallViolationCount = 0;
            emit wallViolationChanged(0);
            m_scene->setWallColor(QColor(200, 200, 205));
        }
        return;
    }
    const QVector<Atom>& atoms = m_trajectoryAtoms[frame];
    int count = 0;
    if (m_wallType == 2) {
        for (const auto& a : atoms) {
            const QVector3D& p = a.position;
            if (p.x() < m_wallMin.x() || p.x() > m_wallMax.x()
                || p.y() < m_wallMin.y() || p.y() > m_wallMax.y()
                || p.z() < m_wallMin.z() || p.z() > m_wallMax.z())
                ++count;
        }
    } else if (m_wallType == 1) {
        for (const auto& a : atoms) {
            if (a.position.length() > m_wallRadius)
                ++count;
        }
    }
    if (count != m_wallViolationCount) {
        m_wallViolationCount = count;
        emit wallViolationChanged(count);
#ifdef DEBUG_ON
        qDebug() << "Wall violations:" << count;
#endif
    }
    // Red when any atom is outside, grey when all inside.
    m_scene->setWallColor(count > 0 ? QColor(230, 70, 70) : QColor(200, 200, 205));
}

void MoleculeViewer::buildForceAdjacency()
{
    if (m_trajectoryAtoms.isEmpty() || m_trajectoryBonds.isEmpty()) {
        m_forceAdjacency.clear();
        return;
    }
    m_forceAdjacency = forceinjector::buildAdjacency(
        m_trajectoryAtoms[0].size(), m_trajectoryBonds[0]);
}

void MoleculeViewer::updateForceVectors()
{
    if (!m_scene)
        return;
    if (!m_forceVectorsVisible || m_grabbedAtom < 0 || m_trajectoryAtoms.isEmpty()) {
        m_scene->setForceArrows({});
        return;
    }
    const int frame = (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) ? m_currentFrame : 0;
    const QVector<Atom>& atoms = m_trajectoryAtoms[frame];
    if (m_grabbedAtom >= atoms.size()) {
        m_scene->setForceArrows({});
        return;
    }

    // Non-negated force = the direction the atom actually moves (toward the cursor).
    const QVector3D modelForce = computeGrabForce(m_lastMousePos, m_grabbedAtom);
    if (modelForce.lengthSquared() < 1e-10f) {
        m_scene->setForceArrows({});
        return;
    }

    QVector<SceneController::Arrow> arrows;
    const float kScale = 8.0f;
    const float kMin = 0.6f;
    const float kMax = qMax(3.0f, m_scene->sceneExtent());

    auto pushArrow = [&](int i, const QVector3D& modelF, bool seed) {
        if (modelF.lengthSquared() < 1e-12f)
            return;
        const QVector3D worldF = m_modelRotation.rotatedVector(modelF);
        SceneController::Arrow a;
        a.origin = modelToWorld(atoms[i].position);
        a.dir = worldF.normalized();
        a.length = qBound(kMin, worldF.length() * kScale, kMax);
        a.color = seed ? QColor(255, 230, 60) : QColor(255, 140, 0);
        arrows.append(a);
    };

    // Mirror exactly what the integrator applies: the same shell distribution.
    if (!m_forceAdjacency.isEmpty()) {
        Eigen::Vector3d f(modelForce.x(), modelForce.y(), modelForce.z());
        Eigen::MatrixXd dist = forceinjector::distributeForce(
            m_grabbedAtom, f, m_forceAdjacency, m_grabAlpha, m_grabMaxShells, atoms.size());
        for (int i = 0; i < atoms.size(); ++i) {
            Eigen::Vector3d row = dist.row(i);
            if (row.squaredNorm() > 1e-14)
                pushArrow(i, QVector3D(float(row.x()), float(row.y()), float(row.z())), i == m_grabbedAtom);
        }
    } else {
        pushArrow(m_grabbedAtom, modelForce, true);
    }
    m_scene->setForceArrows(arrows);
}

// ---------------------------------------------------------------------------
// Measurement overlay (M2): distance (2), angle (3), dihedral (4)
// ---------------------------------------------------------------------------
void MoleculeViewer::updateMeasurement()
{
    if (!m_scene)
        return;
    const int frame = (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) ? m_currentFrame : -1;
    if (m_measurementMode == 0 || frame < 0) {
        m_scene->setMeasurement({}, QString());
        return;
    }
    const QVector<Atom>& atoms = m_trajectoryAtoms[frame];

    // Auto-detect the measurement TYPE from how many atoms are picked:
    // 2 = distance, 3 = angle, 4 = dihedral. Capped at 4; a 5th new pick restarts.
    QVector<int> idx;
    for (int a : m_selectedAtoms)
        if (a >= 0 && a < atoms.size())
            idx.append(a);
    if (idx.size() > 4)
        idx = idx.mid(idx.size() - 4, 4);

    // Lines between consecutive picks (drawn even while incomplete).
    QVector<QPair<QVector3D, QVector3D>> lines;
    for (int i = 0; i + 1 < idx.size(); ++i)
        lines.append({ modelToWorld(atoms[idx[i]].position), modelToWorld(atoms[idx[i + 1]].position) });

    QStringList names;
    for (int a : idx)
        names << QStringLiteral("%1%2").arg(atoms[a].element).arg(a);
    const int n = idx.size();

    QString text;
    if (n == 0) {
        text = tr("Measure — click atoms: 2 = distance, 3 = angle, 4 = dihedral");
    } else if (n == 1) {
        text = tr("Measure: %1 — pick another atom (click an atom again to deselect)").arg(names[0]);
    } else {
        // Show ALL geometric quantities for the picked set, not just the single
        // "auto-detected" one: every pairwise distance, the chain angles, and the
        // dihedral(s). Claude Generated.
        auto pos = [&](int k) { return atoms[idx[k]].position; };
        auto angleAt = [&](int a, int b, int c) {
            return qRadiansToDegrees(qAcos(qBound(-1.0f,
                QVector3D::dotProduct((pos(a) - pos(b)).normalized(), (pos(c) - pos(b)).normalized()), 1.0f)));
        };
        QStringList parts;

        QStringList dists;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                dists << QStringLiteral("%1–%2 %3").arg(names[i], names[j])
                             .arg((pos(j) - pos(i)).length(), 0, 'f', 3);
        parts << tr("d[Å]:  ") + dists.join(QStringLiteral("   "));

        if (n >= 3) {
            QStringList angs;
            for (int i = 0; i + 2 < n; ++i)
                angs << QStringLiteral("%1-%2-%3 %4°").arg(names[i], names[i + 1], names[i + 2])
                            .arg(angleAt(i, i + 1, i + 2), 0, 'f', 1);
            parts << tr("∠[°]:  ") + angs.join(QStringLiteral("   "));
        }
        if (n >= 4) {
            QStringList dihs;
            for (int i = 0; i + 3 < n; ++i) {
                const QVector3D b1 = pos(i + 1) - pos(i), b2 = pos(i + 2) - pos(i + 1), b3 = pos(i + 3) - pos(i + 2);
                const QVector3D nn1 = QVector3D::crossProduct(b1, b2), nn2 = QVector3D::crossProduct(b2, b3);
                const QVector3D mm = QVector3D::crossProduct(nn1, b2.normalized());
                const float dih = qRadiansToDegrees(qAtan2(QVector3D::dotProduct(mm, nn2),
                    QVector3D::dotProduct(nn1, nn2)));
                dihs << QStringLiteral("%1-%2-%3-%4 %5°")
                            .arg(names[i], names[i + 1], names[i + 2], names[i + 3]).arg(dih, 0, 'f', 1);
            }
            parts << tr("φ[°]:  ") + dihs.join(QStringLiteral("   "));
        }
        text = parts.join(QStringLiteral("\n"));
    }
    m_scene->setMeasurement(lines, text);
}

// ---------------------------------------------------------------------------
// Bond editing via an atom pair (M2)
// ---------------------------------------------------------------------------
void MoleculeViewer::performBondEdit(int a, int b)
{
    if (a == b || m_currentFrame < 0 || m_currentFrame >= m_trajectoryBonds.size())
        return;
    QVector<Bond>& bonds = m_trajectoryBonds[m_currentFrame];
    int found = -1;
    for (int i = 0; i < bonds.size(); ++i)
        if ((bonds[i].atom1 == a && bonds[i].atom2 == b) || (bonds[i].atom1 == b && bonds[i].atom2 == a)) {
            found = i;
            break;
        }
    switch (m_bondEditMode) {
    case 1: // add
        if (found < 0)
            bonds.append({ a, b, 1 });
        break;
    case 2: // delete
        if (found >= 0)
            bonds.remove(found);
        break;
    case 3: // cycle order 1->2->3->1
        if (found >= 0)
            bonds[found].bondOrder = (bonds[found].bondOrder % 3) + 1;
        break;
    default:
        return;
    }
    refreshVisualization(); // rebuild geometry, keep the camera
    buildForceAdjacency();
    onStructureChanged();   // trigger XYZ auto-save (if a file is loaded)
}

// ---------------------------------------------------------------------------
// Scene population
// ---------------------------------------------------------------------------
void MoleculeViewer::syncSceneToController(int frameIndex, bool resetCamera, bool fullRebuild,
    bool keepView)
{
    if (!m_scene || frameIndex < 0 || frameIndex >= m_trajectoryAtoms.size())
        return;
    const QVector<Atom>& atoms = m_trajectoryAtoms[frameIndex];

    if (fullRebuild) {
        QVector<SceneController::AtomDatum> sa;
        sa.reserve(atoms.size());
        for (const Atom& a : atoms)
            sa.append({ a.position, a.element, a.charge });
        QVector<SceneController::BondDatum> sb;
        if (frameIndex < m_trajectoryBonds.size()) {
            const QVector<Bond>& bonds = m_trajectoryBonds[frameIndex];
            sb.reserve(bonds.size());
            for (const Bond& b : bonds)
                sb.append({ b.atom1, b.atom2, b.bondOrder });
        }
        m_scene->setStructure(sa, sb, keepView); // keepView: no bounds/camera change
        m_scene->setSelection(m_selectedAtoms);
        if (keepView) {
            // structure editing: keep the user's exact orientation/zoom/pan.
        } else if (!resetCamera) {
            // Keep the user's current orientation/zoom across a refresh.
            m_scene->setRootRotation(m_modelRotation);
        } else {
            m_modelRotation = QQuaternion();
        }
    } else {
        QVector<QVector3D> pos;
        pos.reserve(atoms.size());
        for (const Atom& a : atoms)
            pos.append(a.position);
        m_scene->updatePositions(pos);
    }
}

void MoleculeViewer::clearScene()
{
    if (m_scene)
        m_scene->clear();
    m_modelRotation = QQuaternion();
}

void MoleculeViewer::clearScenePublic()
{
    clearScene();
}

void MoleculeViewer::addMolecule(const QVector<Atom>& atoms, const QVector<Bond>& bonds)
{
    if (atoms.isEmpty()) {
        qWarning() << "Empty atom list - cannot calculate molecule center";
        return;
    }

    QVector3D mn = atoms[0].position;
    QVector3D mx = atoms[0].position;
    for (const Atom& atom : atoms) {
        mn = QVector3D(qMin(mn.x(), atom.position.x()), qMin(mn.y(), atom.position.y()), qMin(mn.z(), atom.position.z()));
        mx = QVector3D(qMax(mx.x(), atom.position.x()), qMax(mx.y(), atom.position.y()), qMax(mx.z(), atom.position.z()));
    }
    m_moleculeCenter = (mn + mx) * 0.5f;
    m_moleculeRadius = (mx - mn).length() * 0.5f;
    if (m_moleculeRadius < 1.0f)
        m_moleculeRadius = 10.0f;

    const QVector<Bond> actualBonds = bonds.isEmpty() ? detectBonds(atoms) : bonds;

    m_trajectoryAtoms.clear();
    m_trajectoryBonds.clear();
    m_trajectoryAtoms.append(atoms);
    m_trajectoryBonds.append(actualBonds);
    m_frameCount = 1;
    m_currentFrame = 0;

    // Single structure: no frame nav / playback.
    if (m_frameControlWidget)
        m_frameControlWidget->setVisible(false);
    if (m_playbackWidget)
        m_playbackWidget->setVisible(false);

    if (m_bondEditor)
        m_bondEditor->setAtoms(atoms);
    if (m_perfOpt)
        m_perfOpt->setAtomCount(atoms.size());

    syncSceneToController(0, /*resetCamera=*/true, /*fullRebuild=*/true);
    buildForceAdjacency();

    m_moleculeDirty = false;
    emit moleculeUpdated(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
}

// Claude Generated 2026 - Merge a molecule into the current scene (single-frame only):
// append atoms/bonds, select the new atoms, and start placement without a camera jump.
void MoleculeViewer::appendMolecule(const QVector<Atom>& newAtoms, const QVector<Bond>& newBonds)
{
    if (newAtoms.isEmpty())
        return;
    if (m_trajectoryAtoms.isEmpty()) {
        addMolecule(newAtoms, newBonds);   // nothing loaded yet -> behave like a load
        return;
    }
    if (!canEditStructure()) {
        qWarning() << "appendMolecule: only single-frame structures can be edited";
        return;
    }
    emit editSnapshotRequested(tr("Before add molecule"));  // pre-merge state for undo
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    const int base = atoms.size();
    atoms += newAtoms;

    const QVector<Bond> appended = newBonds.isEmpty() ? detectBonds(newAtoms) : newBonds;
    if (m_currentFrame >= m_trajectoryBonds.size())
        m_trajectoryBonds.resize(m_currentFrame + 1);
    for (const Bond& b : appended)
        m_trajectoryBonds[m_currentFrame].append({ b.atom1 + base, b.atom2 + base, b.bondOrder });

    if (m_bondEditor)
        m_bondEditor->setAtoms(atoms);
    if (m_perfOpt)
        m_perfOpt->setAtomCount(atoms.size());

    // Select the newly added atoms and rebuild without recentring the camera.
    QVector<int> added;
    for (int i = base; i < atoms.size(); ++i)
        added.append(i);
    m_selectedAtoms = added;
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
    selectAtoms(added, /*append=*/false);
    buildForceAdjacency();
    if (!m_editMode)
        setEditMode(true);          // placement implies edit mode
    m_moveRefLocal = selectionCentroidLocal();
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
}

void MoleculeViewer::setOverlayWorkspace(const QVector<Atom>& refAtoms,
    const QVector<Bond>& refBonds, bool refVisible, const QVector<OverlaySpec>& overlays,
    bool resetView)
{
    if (!m_scene)
        return;
    if (resetView) {
        if (refAtoms.isEmpty()) {
            clearOverlays();
            return;
        }
        // Reference changed: make it the primary structure (reframes the camera, and the
        // structure reset clears any existing overlays).
        addMolecule(refAtoms, refBonds);
    } else {
        // Only the overlay set changed: keep the current primary + camera.
        clearOverlays();
    }
    setPrimaryVisible(refVisible);
    for (const OverlaySpec& o : overlays) {
        const int idx = addOverlay(o.atoms, o.tint, o.sizeScale);
        if (idx >= 0 && !o.visible)
            m_scene->setOverlayVisible(idx, false);
    }
}

int MoleculeViewer::addOverlay(const QVector<Atom>& targetAtoms, const QColor& tint,
    float sizeScale, const QVector<Bond>& targetBonds)
{
    if (targetAtoms.isEmpty() || !m_scene)
        return -1;

    const QVector<Bond> tBonds = targetBonds.isEmpty() ? detectBonds(targetAtoms) : targetBonds;
    QVector<SceneController::AtomDatum> ta;
    ta.reserve(targetAtoms.size());
    for (const Atom& a : targetAtoms)
        ta.append({ a.position, a.element, a.charge });
    QVector<SceneController::BondDatum> tb;
    tb.reserve(tBonds.size());
    for (const Bond& b : tBonds)
        tb.append({ b.atom1, b.atom2, b.bondOrder });
    return m_scene->addOverlayStructure(ta, tb, tint, sizeScale);
}

void MoleculeViewer::setOverlayTint(int index, const QColor& tint)
{
    if (m_scene)
        m_scene->setOverlayTint(index, tint);
}

void MoleculeViewer::setOverlaySize(int index, float sizeScale)
{
    if (m_scene)
        m_scene->setOverlaySize(index, sizeScale);
}

void MoleculeViewer::setOverlayVisible(int index, bool visible)
{
    if (m_scene)
        m_scene->setOverlayVisible(index, visible);
}

void MoleculeViewer::setPrimaryVisible(bool visible)
{
    if (m_scene)
        m_scene->setPrimaryVisible(visible);
}

void MoleculeViewer::clearOverlays()
{
    if (m_scene)
        m_scene->clearOverlay();
}

int MoleculeViewer::overlayCount() const
{
    return m_scene ? m_scene->overlayCount() : 0;
}

void MoleculeViewer::setTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds)
{
    m_trajectoryAtoms = atoms;
    m_frameCount = atoms.size();
    m_currentFrame = 0;
    m_moleculeDirty = false;

    m_trajectoryBonds.clear();
    if (bonds.isEmpty() || (bonds.size() == atoms.size() && bonds[0].isEmpty())) {
        for (int i = 0; i < atoms.size(); ++i)
            m_trajectoryBonds.append(detectBonds(atoms[i]));
    } else {
        m_trajectoryBonds = bonds;
    }

    if (m_frameSlider && m_frameLabel && m_frameJumpBox && m_frameControlWidget) {
        m_frameSlider->setMaximum(m_frameCount - 1);
        m_frameJumpBox->setMaximum(m_frameCount - 1);
        m_frameLabel->setText(QString("1/%1").arg(m_frameCount));
        m_frameControlWidget->setVisible(m_frameCount > 1);
    }
    if (m_playbackWidget)
        m_playbackWidget->setVisible(m_frameCount > 1);

    if (m_frameCount > 0)
        showFrame(0);

    buildForceAdjacency();
    emit trajectoryLoaded(m_frameCount);
    if (m_frameCount > 0)
        emit moleculeUpdated(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
}

void MoleculeViewer::showFrame(int frameIndex)
{
    if (frameIndex < 0 || frameIndex >= m_trajectoryAtoms.size())
        return;
    m_currentFrame = frameIndex;

    const QVector<Atom>& atoms = m_trajectoryAtoms[frameIndex];
    if (!atoms.isEmpty()) {
        QVector3D mn = atoms[0].position;
        QVector3D mx = atoms[0].position;
        for (const Atom& atom : atoms) {
            mn = QVector3D(qMin(mn.x(), atom.position.x()), qMin(mn.y(), atom.position.y()), qMin(mn.z(), atom.position.z()));
            mx = QVector3D(qMax(mx.x(), atom.position.x()), qMax(mx.y(), atom.position.y()), qMax(mx.z(), atom.position.z()));
        }
        m_moleculeCenter = (mn + mx) * 0.5f;
        m_moleculeRadius = (mx - mn).length() * 0.5f;
    }

    syncSceneToController(frameIndex, /*resetCamera=*/true, /*fullRebuild=*/true);

    if (m_frameSlider && m_frameLabel && m_frameJumpBox) {
        m_frameSlider->blockSignals(true);
        m_frameSlider->setValue(m_currentFrame);
        m_frameSlider->blockSignals(false);
        m_frameJumpBox->blockSignals(true);
        m_frameJumpBox->setValue(m_currentFrame);
        m_frameJumpBox->blockSignals(false);
        m_frameLabel->setText(QString("%1/%2").arg(m_currentFrame + 1).arg(m_frameCount));
    }

    updateMeasurement(); // keep measurement lines/values on the new frame
    computeWallViolations();  // recolour box + status for the new frame
    emit frameChanged(m_currentFrame);
}

void MoleculeViewer::updateFramePositions(int frameIndex)
{
    if (frameIndex < 0 || frameIndex >= m_trajectoryAtoms.size())
        return;
    m_currentFrame = frameIndex;
    syncSceneToController(frameIndex, /*resetCamera=*/false, /*fullRebuild=*/false);

    if (m_frameSlider && m_frameLabel && m_frameJumpBox) {
        m_frameSlider->blockSignals(true);
        m_frameSlider->setValue(m_currentFrame);
        m_frameSlider->blockSignals(false);
        m_frameJumpBox->blockSignals(true);
        m_frameJumpBox->setValue(m_currentFrame);
        m_frameJumpBox->blockSignals(false);
        m_frameLabel->setText(QString("%1/%2").arg(m_currentFrame + 1).arg(m_frameCount));
    }
    emit frameChanged(m_currentFrame);
}

void MoleculeViewer::nextFrame()
{
    if (m_currentFrame < m_trajectoryAtoms.size() - 1)
        showFrame(m_currentFrame + 1);
}

void MoleculeViewer::previousFrame()
{
    if (m_currentFrame > 0)
        showFrame(m_currentFrame - 1);
}

namespace {
// Claude Generated 2026 - order-independent comparison of two bond sets (file-provided bonds may
// not be in ascending (i,j) order, so compare as a set rather than element-wise).
quint64 bondPairKey(int i, int j)
{
    return (static_cast<quint64>(qMin(i, j)) << 32) | static_cast<quint32>(qMax(i, j));
}
bool bondSetEqual(const QVector<MoleculeViewer::Bond>& a, const QVector<MoleculeViewer::Bond>& b)
{
    if (a.size() != b.size())
        return false;
    QSet<quint64> sa;
    sa.reserve(a.size());
    for (const MoleculeViewer::Bond& x : a)
        sa.insert(bondPairKey(x.atom1, x.atom2));
    for (const MoleculeViewer::Bond& x : b)
        if (!sa.contains(bondPairKey(x.atom1, x.atom2)))
            return false;
    return true;
}
}  // namespace

void MoleculeViewer::updateSimulationFrame(SimulationFramePtr frame)
{
    if (!frame)
        return;
    const auto& positions = frame->positions;
    const int n = static_cast<int>(positions.size());

    // Topology change -> rebuild (carry element/charge where possible).
    if (m_trajectoryAtoms.isEmpty() || m_trajectoryAtoms[0].size() != n) {
        QVector<Atom> atoms;
        atoms.reserve(n);
        const bool haveCache = !m_trajectoryAtoms.isEmpty();
        for (int i = 0; i < n; ++i) {
            Atom a;
            a.position = positions[i];
            if (haveCache && i < m_trajectoryAtoms[0].size()) {
                a.element = m_trajectoryAtoms[0][i].element;
                a.charge = m_trajectoryAtoms[0][i].charge;
            } else {
                a.element = QStringLiteral("C");
            }
            atoms.append(a);
        }
        addMolecule(atoms, {});
        return;
    }

    // In-place position update (keep element/charge).
    auto& refAtoms = m_trajectoryAtoms[0];
    for (int i = 0; i < n; ++i)
        refAtoms[i].position = positions[i];

    // Dynamic bonds: re-detect the bond graph from the new geometry so bond breaking/formation in
    // MD/Opt reactions is reflected by the drawn bonds. Only the (rare) topology-change frames
    // rebuild the bond instancing; stable frames stay on the fast position-only path. The bonds-only
    // rebuild keeps the camera/bounds fixed so a reaction event does not jolt the view.
    // Claude Generated 2026.
    bool topologyChanged = false;
    if (m_dynamicBonds && !m_trajectoryBonds.isEmpty()) {
        QVector<Bond> newBonds = detectBondsHysteresis(refAtoms, m_trajectoryBonds[0]);
        if (!bondSetEqual(newBonds, m_trajectoryBonds[0])) {
            m_trajectoryBonds[0] = newBonds;
            topologyChanged = true;
        }
    }

    syncSceneToController(0, /*resetCamera=*/false, /*fullRebuild=*/false);
    if (topologyChanged && m_scene) {
        QVector<SceneController::BondDatum> sb;
        sb.reserve(m_trajectoryBonds[0].size());
        for (const Bond& b : m_trajectoryBonds[0])
            sb.append({ b.atom1, b.atom2, b.bondOrder });
        m_scene->updateBonds(sb);
    }
    computeWallViolations();  // live MD: recolour box + status as atoms cross walls

    // Throttled cache notify (once per worker run).
    if (!m_moleculeDirty) {
        m_moleculeDirty = true;
        emit moleculeUpdated(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
    }

    // Re-send the grab force at sim cadence while a grab is active.
    if (m_grabbedAtom >= 0) {
        QVector3D modelLocalForce = computeGrabForce(m_lastMousePos, m_grabbedAtom);
        emit atomForceRequested(m_grabbedAtom, -modelLocalForce, m_grabAlpha, m_grabMaxShells);
        updateForceVectors();
    }
}

// ---------------------------------------------------------------------------
// Camera / view commands
// ---------------------------------------------------------------------------
void MoleculeViewer::setDefaultView()
{
    m_modelRotation = QQuaternion();
    if (m_scene) {
        m_scene->setRootRotation(m_modelRotation);
        m_scene->resetView();
    }
}

void MoleculeViewer::resetView() { setDefaultView(); }

void MoleculeViewer::resetViewToMolecule() { setDefaultView(); }

// Claude Generated 2026 - Translate every trajectory frame so that the mass-weighted
// centre-of-mass coincides with the coordinate origin. Uses curcuma Elements tables
// for consistent masses. After shifting, the current frame is reloaded with a camera reset.
void MoleculeViewer::centerAtOrigin()
{
    if (m_trajectoryAtoms.isEmpty()) return;
    for (QVector<Atom>& frame : m_trajectoryAtoms) {
        if (frame.isEmpty()) continue;
        double totalMass = 0.0;
        QVector3D com;
        for (const Atom& a : frame) {
            const int z = Elements::String2Element(a.element.toLower().toStdString());
            const double mass = (z > 0 && z < static_cast<int>(Elements::AtomicMass.size()))
                ? Elements::AtomicMass[z] : 12.011;
            com += a.position * static_cast<float>(mass);
            totalMass += mass;
        }
        if (totalMass > 0.0) com /= static_cast<float>(totalMass);
        for (Atom& a : frame) a.position -= com;
    }
    showFrame(m_currentFrame);
}

void MoleculeViewer::getSelectedBounds(QVector3D& center, float& radius)
{
    if (m_trajectoryAtoms.isEmpty() || m_currentFrame >= m_trajectoryAtoms.size()) {
        center = m_moleculeCenter;
        radius = m_moleculeRadius;
        return;
    }
    const auto& atoms = m_trajectoryAtoms[m_currentFrame];
    if (atoms.isEmpty() || m_selectedAtoms.isEmpty()) {
        center = m_moleculeCenter;
        radius = m_moleculeRadius;
        return;
    }
    QVector3D sum(0, 0, 0);
    for (int idx : m_selectedAtoms)
        if (idx >= 0 && idx < atoms.size())
            sum += atoms[idx].position;
    center = sum / m_selectedAtoms.size();
    radius = 0.0f;
    for (int idx : m_selectedAtoms)
        if (idx >= 0 && idx < atoms.size())
            radius = qMax(radius, (atoms[idx].position - center).length());
    radius += 2.0f;
}

void MoleculeViewer::centerOnAtom(int atomIndex)
{
    if (!m_scene || m_trajectoryAtoms.isEmpty() || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    const auto& atoms = m_trajectoryAtoms[m_currentFrame];
    if (atomIndex < 0 || atomIndex >= atoms.size())
        return;
    m_scene->fitToBounds(modelToWorld(atoms[atomIndex].position), 3.0f);
}

void MoleculeViewer::zoomToSelection(const QVector<int>& atomIndices)
{
    if (atomIndices.isEmpty()) {
        fitAllInView();
        return;
    }
    m_selectedAtoms = atomIndices;
    QVector3D center;
    float radius = 0.0f;
    getSelectedBounds(center, radius);
    if (m_scene)
        m_scene->fitToBounds(modelToWorld(center), radius);
}

void MoleculeViewer::fitAllInView()
{
    if (m_scene)
        m_scene->fitToBounds(m_scene->sceneCenter(), m_scene->sceneExtent());
}

// ---------------------------------------------------------------------------
// Appearance / effects (now actually wired, vs the former Qt3D stubs)
// ---------------------------------------------------------------------------
void MoleculeViewer::setRenderingMode(RenderingMode mode)
{
    m_renderingMode = mode;
    if (m_scene)
        m_scene->setRenderingMode(static_cast<int>(mode));
    emit renderingModeChanged(mode);
}

void MoleculeViewer::setMaterialMode(MaterialMode mode) { m_materialMode = mode; }

void MoleculeViewer::setGlowIntensity(float intensity) { m_glowIntensity = qBound(1.0f, intensity, 2.0f); }

void MoleculeViewer::setColorScheme(ColorScheme scheme)
{
    m_colorScheme = scheme;
    if (m_scene)
        m_scene->setColorScheme(static_cast<int>(scheme));
    emit colorSchemeChanged(scheme);
}

void MoleculeViewer::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
    if (m_scene)
        m_scene->setBackgroundColor(color);
}

void MoleculeViewer::setCornerLightEnabled(int index, bool on)
{
    if (index < 0 || index > 3)
        return;
    m_cornerLightEnabled[index] = on;
    if (m_scene)
        m_scene->setCornerLight(index, on);
}

bool MoleculeViewer::isCornerLightEnabled(int index) const
{
    return (index >= 0 && index < 4) ? m_cornerLightEnabled[index] : false;
}

void MoleculeViewer::setAtomTransparency(float alpha)
{
    m_atomTransparency = qBound(0.0f, alpha, 1.0f);
    if (m_scene)
        m_scene->setAtomTransparency(m_atomTransparency);
}

void MoleculeViewer::setAtomShininess(float shininess) { m_atomShininess = shininess; }

void MoleculeViewer::setAtomScaleFactor(float scale)
{
    m_atomScaleFactor = scale;
    if (m_scene)
        m_scene->setAtomScaleFactor(scale);
}

void MoleculeViewer::setBondThickness(float thickness)
{
    m_bondThickness = thickness;
    if (m_scene)
        m_scene->setBondThickness(thickness);
}

void MoleculeViewer::setFogEnabled(bool enabled)
{
    m_fogEnabled = enabled;
    if (m_scene)
        m_scene->setFog(enabled, m_fogIntensity);
}

void MoleculeViewer::setFogIntensity(float intensity)
{
    m_fogIntensity = qBound(0.0f, intensity, 1.0f);
    if (m_scene)
        m_scene->setFog(m_fogEnabled, m_fogIntensity);
}

void MoleculeViewer::setFogDistance(float distance)
{
    m_fogDistance = qBound(0.0f, distance, 1.0f);
    if (m_scene)
        m_scene->setFogDistance(m_fogDistance);
}

void MoleculeViewer::setSSAOEnabled(bool enabled)
{
    m_ssaoEnabled = enabled;
    if (m_scene)
        m_scene->setSsao(enabled, m_ssaoIntensity);
}
void MoleculeViewer::setSSAOIntensity(float intensity)
{
    m_ssaoIntensity = qBound(0.0f, intensity, 2.0f);
    if (m_scene)
        m_scene->setSsao(m_ssaoEnabled, m_ssaoIntensity);
}
void MoleculeViewer::setSSAORadius(float radius) { m_ssaoRadius = qBound(0.01f, radius, 0.2f); }
void MoleculeViewer::setSSAOBias(float bias) { m_ssaoBias = qBound(0.0f, bias, 0.1f); }

void MoleculeViewer::setBloomEnabled(bool enabled)
{
    m_bloomEnabled = enabled;
    if (m_scene)
        m_scene->setBloom(enabled);
}
void MoleculeViewer::setBloomThreshold(float threshold) { m_bloomThreshold = qBound(0.5f, threshold, 1.5f); }
void MoleculeViewer::setBloomIntensity(float intensity) { m_bloomIntensity = qBound(0.0f, intensity, 2.0f); }
void MoleculeViewer::setHDREnabled(bool enabled)
{
    m_hdrEnabled = enabled;
    if (m_scene)
        m_scene->setHdr(enabled);
}
void MoleculeViewer::setExposure(float exposure)
{
    m_exposure = qBound(0.5f, exposure, 3.0f);
    if (m_scene)
        m_scene->setExposure(m_exposure);
}

void MoleculeViewer::setRotationMode(int mode)
{
    m_rotationMode = (mode == 1) ? RotationMode::CameraOrbit : RotationMode::Model;
}

void MoleculeViewer::setInstancingThreshold(int n)
{
    m_instancingThreshold = qMax(1, n);
}

void MoleculeViewer::refreshVisualization()
{
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size())
        syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true);
}

// ---------------------------------------------------------------------------
// Simulation / picking compatibility hooks
// ---------------------------------------------------------------------------
void MoleculeViewer::setSimulationActive(bool on)
{
    m_simulationActive = on;
    m_grabbedAtom = -1;
    if (m_scene) m_scene->setForceArrows({});
    Qt::CursorShape shape = on ? Qt::SizeAllCursor : Qt::ArrowCursor;
    if (m_quickView) m_quickView->setCursor(shape);
    if (m_container) m_container->setCursor(shape);
}

void MoleculeViewer::setPickingActive(bool /*active*/)
{
    // No-op: Quick3D picking is ray-based and always available.
}

void MoleculeViewer::ensurePickersForGrab()
{
    // No-op: ray picking works for instanced geometry of any size.
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------
void MoleculeViewer::selectAtom(int index, bool append)
{
    if (index < 0)
        return;
    selectAtoms({ index }, append);
}

// Claude Generated 2026 - Bulk select; shared by single-pick, fragment, paste, merge.
void MoleculeViewer::selectAtoms(const QVector<int>& indices, bool append)
{
    if (!append)
        m_selectedAtoms.clear();
    for (int idx : indices)
        if (idx >= 0 && !m_selectedAtoms.contains(idx))
            m_selectedAtoms.append(idx);
    if (m_selectionManager) {
        m_selectionManager->clearSelection();
        for (int a : m_selectedAtoms)
            m_selectionManager->selectAtom(a, true);
    }
    if (m_scene)
        m_scene->setSelection(m_selectedAtoms);
    emit selectionChanged(m_selectedAtoms);
}

// Claude Generated 2026 - Select the whole connected fragment containing seedAtom by
// BFS over the current frame's bond graph (what the user actually sees), reusing the
// force-injection adjacency builder.
void MoleculeViewer::selectFragment(int seedAtom, bool append)
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    if (seedAtom < 0 || seedAtom >= m_trajectoryAtoms[m_currentFrame].size())
        return;
    const QVector<Bond> bonds = (m_currentFrame < m_trajectoryBonds.size())
        ? m_trajectoryBonds[m_currentFrame] : QVector<Bond>{};
    const auto adjacency = forceinjector::buildAdjacency(
        m_trajectoryAtoms[m_currentFrame].size(), bonds);

    QVector<int> fragment;
    QSet<int> seen;
    QVector<int> queue{ seedAtom };
    seen.insert(seedAtom);
    while (!queue.isEmpty()) {
        const int a = queue.takeFirst();
        fragment.append(a);
        for (int nb : adjacency[a])
            if (!seen.contains(nb)) {
                seen.insert(nb);
                queue.append(nb);
            }
    }
    selectAtoms(fragment, append);
}

void MoleculeViewer::clearSelection()
{
    m_selectedAtoms.clear();
    if (m_selectionManager)
        m_selectionManager->clearSelection();
    if (m_scene)
        m_scene->setSelection(m_selectedAtoms);
    updateMeasurement(); // selection now empty -> clears the measurement display
}

void MoleculeViewer::setMeasurementMode(int mode)
{
    m_measurementMode = qBound(0, mode, 3);
    if (m_measurementMode != 0 && m_editMode)
        setEditMode(false);  // mutually exclusive interaction modes
    updateMeasurement(); // refresh/clear the on-screen measurement for the new mode
    emit measurementModeChanged(m_measurementMode);
}

void MoleculeViewer::setBondEditMode(int mode)
{
    m_bondEditMode = qBound(0, mode, 3);
    if (m_bondEditMode != 0 && m_editMode)
        setEditMode(false);  // mutually exclusive interaction modes
    if (m_bondEditor) {
        BondEditor::EditMode editorMode;
        switch (m_bondEditMode) {
        case 1: editorMode = BondEditor::EditMode::AddBondMode; break;
        case 2: editorMode = BondEditor::EditMode::DeleteBondMode; break;
        case 3: editorMode = BondEditor::EditMode::ChangeBondMode; break;
        default: editorMode = BondEditor::EditMode::None;
        }
        m_bondEditor->setEditMode(editorMode);
    }
}

// ===========================================================================
// Structure editing (Explore-mode "Edit" toggle). Claude Generated 2026.
// Direct coordinate editing — select atoms/molecules, move, copy/paste, merge a
// file, with collision feedback. Distinct from the simulation grab-force, which
// injects forces into a running MD/Opt rather than mutating stored geometry.
// ===========================================================================
void MoleculeViewer::setEditMode(bool on)
{
    if (m_editMode == on)
        return;
    m_editMode = on;
    if (on) {
        if (m_measurementMode != 0)
            setMeasurementMode(0);
        if (m_bondEditMode != 0)
            setBondEditMode(0);
        computeCollisions();   // show any pre-existing clashes immediately
        if (m_scene)
            m_scene->setEditHint(tr("Edit  ·  drag: move (Shift = depth)  ·  WASD/QE: rotate"
                                    "  ·  double-click: whole molecule"
                                    "  ·  Ctrl/Shift+drag: box-select  ·  right-click: clear"));
    } else {
        m_movingSelection = false;
        m_emptyPressPending = false;
        m_collisionAtoms.clear();
        if (m_scene) {
            m_scene->setCollisionAtoms({});
            m_scene->setEditHint(QString());
        }
        emit collisionCountChanged(0);
    }
    emit editModeChanged(m_editMode);
}

QVector3D MoleculeViewer::selectionCentroidLocal() const
{
    QVector3D c;
    if (m_selectedAtoms.isEmpty() || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return c;
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    int n = 0;
    for (int idx : m_selectedAtoms)
        if (idx >= 0 && idx < atoms.size()) {
            c += atoms[idx].position;
            ++n;
        }
    if (n > 0)
        c /= float(n);
    return c;
}

// Translate every selected atom by a model-local delta, redraw cheaply (no camera
// jump), and re-check collisions for live red feedback.
void MoleculeViewer::moveSelection(const QVector3D& modelDelta)
{
    if (m_selectedAtoms.isEmpty() || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    for (int idx : m_selectedAtoms)
        if (idx >= 0 && idx < atoms.size())
            atoms[idx].position += modelDelta;
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/false);
    computeCollisions();
}

// Claude Generated 2026 - Apply a single-atom edit coming from the atom table.
// Position-only changes use the cheap updatePositions path; an element change
// needs an atom rebuild (radius/colour), but keepView avoids any camera jump.
void MoleculeViewer::setAtomInCurrentFrame(int index, const QString& element, const QVector3D& position)
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (index < 0 || index >= atoms.size())
        return;
    const bool elementChanged = (atoms[index].element != element);
    atoms[index].element = element;
    atoms[index].position = position;
    syncSceneToController(m_currentFrame, /*resetCamera=*/false,
        /*fullRebuild=*/elementChanged, /*keepView=*/true);
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
}

// Claude Generated 2026 - Replace the whole current-frame geometry from parsed
// atoms (structure text-editor "Apply"). Single-frame only — editing a frame of a
// trajectory would desync the other frames. Re-detects bonds; keepView preserves
// the camera so an Apply does not jump the view.
bool MoleculeViewer::applyStructureFromAtoms(const QVector<Atom>& atoms)
{
    if (!canEditStructure() || atoms.isEmpty())
        return false;
    if (m_trajectoryAtoms.isEmpty()) {
        m_trajectoryAtoms.resize(1);
        m_trajectoryBonds.resize(1);
        m_frameCount = 1;
        m_currentFrame = 0;
    }
    m_selectedAtoms.clear();  // indices may no longer be valid after a count change
    m_trajectoryAtoms[m_currentFrame] = atoms;
    m_trajectoryBonds[m_currentFrame] = detectBonds(atoms);
    syncSceneToController(m_currentFrame, /*resetCamera=*/false,
        /*fullRebuild=*/true, /*keepView=*/true);
    buildForceAdjacency();
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
    return true;
}

// Flag atoms that overlap (centre distance < kClashFactor * (vdw_i + vdw_j)). Bonded
// pairs and pairs entirely inside the moving selection (a rigid body) never clash.
void MoleculeViewer::computeCollisions()
{
    m_collisionAtoms.clear();
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) {
        const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
        QSet<quint64> bonded;
        if (m_currentFrame < m_trajectoryBonds.size())
            for (const Bond& b : m_trajectoryBonds[m_currentFrame])
                bonded.insert(bondPairKey(b.atom1, b.atom2));
        const QSet<int> sel(m_selectedAtoms.begin(), m_selectedAtoms.end());
        QSet<int> clash;
        for (int i = 0; i < atoms.size(); ++i) {
            for (int j = i + 1; j < atoms.size(); ++j) {
                if (sel.contains(i) && sel.contains(j))
                    continue;  // rigid body: intra-selection never self-clashes
                if (bonded.contains(bondPairKey(i, j)))
                    continue;
                const float thr = (elem::vdwRadius(atoms[i].element)
                                   + elem::vdwRadius(atoms[j].element)) * kClashFactor;
                if ((atoms[i].position - atoms[j].position).length() < thr) {
                    clash.insert(i);
                    clash.insert(j);
                }
            }
        }
        m_collisionAtoms = QVector<int>(clash.begin(), clash.end());
    }
    if (m_scene)
        m_scene->setCollisionAtoms(m_collisionAtoms);
    emit collisionCountChanged(m_collisionAtoms.size());
}

// Re-detect connectivity after a move/edit so moved atoms gain/lose bonds, then
// recompute clashes and notify consumers. Keeps the camera fixed.
void MoleculeViewer::finalizeEdit()
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    if (m_currentFrame < m_trajectoryBonds.size()) {
        const QVector<Bond> newBonds = detectBondsHysteresis(
            m_trajectoryAtoms[m_currentFrame], m_trajectoryBonds[m_currentFrame]);
        if (!bondSetEqual(newBonds, m_trajectoryBonds[m_currentFrame])) {
            m_trajectoryBonds[m_currentFrame] = newBonds;
            if (m_scene) {
                QVector<SceneController::BondDatum> sb;
                sb.reserve(newBonds.size());
                for (const Bond& b : newBonds)
                    sb.append({ b.atom1, b.atom2, b.bondOrder });
                m_scene->updateBonds(sb);  // bonds only, no bounds/camera change
            }
            buildForceAdjacency();
        }
    }
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
}

// Iteratively translate the moving selection (rigidly) along the net push-apart
// direction until no atom clashes with the rest, or a step cap is reached.
void MoleculeViewer::resolveClashes()
{
    if (m_selectedAtoms.isEmpty() || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    const QSet<int> sel(m_selectedAtoms.begin(), m_selectedAtoms.end());
    QSet<quint64> bonded;
    if (m_currentFrame < m_trajectoryBonds.size())
        for (const Bond& b : m_trajectoryBonds[m_currentFrame])
            bonded.insert(bondPairKey(b.atom1, b.atom2));

    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    constexpr int kMaxIter = 200;
    constexpr float kStep = 0.15f;  // Angstrom per iteration
    for (int iter = 0; iter < kMaxIter; ++iter) {
        QVector3D push;
        int clashes = 0;
        for (int s : sel) {
            if (s < 0 || s >= atoms.size())
                continue;
            for (int j = 0; j < atoms.size(); ++j) {
                if (sel.contains(j) || bonded.contains(bondPairKey(s, j)))
                    continue;
                const float thr = (elem::vdwRadius(atoms[s].element)
                                   + elem::vdwRadius(atoms[j].element)) * kClashFactor;
                QVector3D d = atoms[s].position - atoms[j].position;
                const float dist = d.length();
                if (dist < thr) {
                    ++clashes;
                    const QVector3D dir = (dist > 1e-4f) ? d / dist : QVector3D(1, 0, 0);
                    push += dir * (thr - dist);  // overlap-weighted
                }
            }
        }
        if (clashes == 0)
            break;
        if (push.lengthSquared() < 1e-8f)
            push = QVector3D(1, 0, 0);  // degenerate (fully enclosed): escape arbitrarily
        push.normalize();
        for (int s : sel)
            if (s >= 0 && s < atoms.size())
                atoms[s].position += push * kStep;
    }
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/false);
    finalizeEdit();
}

// Copy the selection (atoms + bonds internal to it) into the clipboard, re-indexed
// to a 0-based block.
void MoleculeViewer::copySelection()
{
    m_clipboardAtoms.clear();
    m_clipboardBonds.clear();
    if (m_selectedAtoms.isEmpty() || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    QHash<int, int> remap;  // old index -> clipboard index
    for (int idx : m_selectedAtoms) {
        if (idx < 0 || idx >= atoms.size())
            continue;
        remap.insert(idx, m_clipboardAtoms.size());
        m_clipboardAtoms.append(atoms[idx]);
    }
    if (m_currentFrame < m_trajectoryBonds.size())
        for (const Bond& b : m_trajectoryBonds[m_currentFrame])
            if (remap.contains(b.atom1) && remap.contains(b.atom2))
                m_clipboardBonds.append({ remap[b.atom1], remap[b.atom2], b.bondOrder });
}

// Paste the clipboard into the current frame (small offset so it's visible), select
// it, and start placement. Single-frame only.
void MoleculeViewer::pasteClipboard()
{
    if (m_clipboardAtoms.isEmpty() || !canEditStructure())
        return;
    QVector<Atom> add = m_clipboardAtoms;
    const QVector3D offset(1.5f, 1.5f, 0.0f);
    for (Atom& a : add)
        a.position += offset;
    appendMolecule(add, m_clipboardBonds);
}

// Remove the selected atoms and their incident bonds, reindexing the survivors.
// Single-frame only.
void MoleculeViewer::deleteSelection()
{
    if (m_selectedAtoms.isEmpty() || !canEditStructure())
        return;
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    emit editSnapshotRequested(tr("Before delete"));  // pre-delete state for undo
    const QSet<int> del(m_selectedAtoms.begin(), m_selectedAtoms.end());
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    QVector<Atom> kept;
    QVector<int> newIndex(atoms.size(), -1);  // old -> new (or -1 if deleted)
    for (int i = 0; i < atoms.size(); ++i) {
        if (del.contains(i))
            continue;
        newIndex[i] = kept.size();
        kept.append(atoms[i]);
    }
    QVector<Bond> keptBonds;
    if (m_currentFrame < m_trajectoryBonds.size())
        for (const Bond& b : m_trajectoryBonds[m_currentFrame]) {
            if (del.contains(b.atom1) || del.contains(b.atom2))
                continue;
            keptBonds.append({ newIndex[b.atom1], newIndex[b.atom2], b.bondOrder });
        }
    m_trajectoryAtoms[m_currentFrame] = kept;
    if (m_currentFrame < m_trajectoryBonds.size())
        m_trajectoryBonds[m_currentFrame] = keptBonds;

    m_selectedAtoms.clear();
    m_collisionAtoms.clear();
    if (m_bondEditor)
        m_bondEditor->setAtoms(kept);
    if (m_perfOpt)
        m_perfOpt->setAtomCount(kept.size());
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
    buildForceAdjacency();
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
}

// ---------------------------------------------------------------------------
// Atom data accessors
// ---------------------------------------------------------------------------
QVector<QVector3D> MoleculeViewer::getAtomPositions() const
{
    QVector<QVector3D> positions;
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size())
        for (const Atom& atom : m_trajectoryAtoms[m_currentFrame])
            positions.append(atom.position);
    return positions;
}

QVector<QString> MoleculeViewer::getAtomElements() const
{
    QVector<QString> elements;
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size())
        for (const Atom& atom : m_trajectoryAtoms[m_currentFrame])
            elements.append(atom.element);
    return elements;
}

QVector<float> MoleculeViewer::getAtomCharges() const
{
    QVector<float> charges;
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size())
        for (const Atom& atom : m_trajectoryAtoms[m_currentFrame])
            charges.append(atom.charge);
    return charges;
}

QVector<MoleculeViewer::Atom> MoleculeViewer::getCurrentFrameAtoms() const
{
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size())
        return m_trajectoryAtoms[m_currentFrame];
    return {};
}

// ---------------------------------------------------------------------------
// Element data + bond detection (logic preserved from the Qt3D version)
// ---------------------------------------------------------------------------
QColor MoleculeViewer::getAtomColor(const QString& element, float charge)
{
    switch (m_colorScheme) {
    case ColorScheme::Monochrome:
        return QColor(180, 180, 180);
    case ColorScheme::ByCharge:
        if (charge > 0.5f) return QColor(255, 0, 0);
        if (charge < -0.5f) return QColor(0, 0, 255);
        if (charge > 0.1f) return QColor(255, 128, 128);
        if (charge < -0.1f) return QColor(128, 128, 255);
        return QColor(220, 220, 220);
    case ColorScheme::CPK:
    case ColorScheme::Custom:
    default:
        return elem::cpkColor(element);
    }
}

float MoleculeViewer::getAtomRadius(const QString& element) const
{
    const float base = elem::vdwRadius(element);
    if (m_renderingMode == RenderingMode::SpaceFilling)
        return base * 2.0f * m_atomScaleFactor;
    return base * m_atomScaleFactor;
}

float MoleculeViewer::getCovalentRadius(const QString& element)
{
    return elem::covalentRadius(element);
}

QVector<MoleculeViewer::Bond> MoleculeViewer::detectBonds(const QVector<Atom>& atoms)
{
    QVector<Bond> detectedBonds;
    const float BOND_TOLERANCE = 1.25f;
    for (int i = 0; i < atoms.size(); ++i) {
        for (int j = i + 1; j < atoms.size(); ++j) {
            const float distance = (atoms[i].position - atoms[j].position).length();
            const float threshold = (getCovalentRadius(atoms[i].element) + getCovalentRadius(atoms[j].element)) * BOND_TOLERANCE;
            if (distance <= threshold)
                detectedBonds.append({ i, j, 1 });
        }
    }
    return detectedBonds;
}

// Claude Generated 2026 - per-frame bond detection with hysteresis. A currently-bonded pair is
// kept until it stretches past the looser BREAK threshold; an unbonded pair only forms a bond
// within the tighter FORM threshold. The gap between the two suppresses on/off flicker for bonds
// that vibrate near the cutoff at finite temperature.
QVector<MoleculeViewer::Bond> MoleculeViewer::detectBondsHysteresis(
    const QVector<Atom>& atoms, const QVector<Bond>& previous)
{
    QSet<quint64> bonded;
    bonded.reserve(previous.size());
    for (const Bond& b : previous)
        bonded.insert(bondPairKey(b.atom1, b.atom2));

    QVector<Bond> result;
    constexpr float FORM = 1.25f;   // matches detectBonds() used at load
    constexpr float BREAK = 1.45f;  // ~16% looser before an existing bond is dropped
    for (int i = 0; i < atoms.size(); ++i) {
        for (int j = i + 1; j < atoms.size(); ++j) {
            const float distance = (atoms[i].position - atoms[j].position).length();
            const float r = getCovalentRadius(atoms[i].element) + getCovalentRadius(atoms[j].element);
            const float tol = bonded.contains(bondPairKey(i, j)) ? BREAK : FORM;
            if (distance <= r * tol)
                result.append({ i, j, 1 });
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Screenshot
// ---------------------------------------------------------------------------
void MoleculeViewer::saveScreenshot(const QString& filename, int scaleFactor)
{
    if (!m_quickView) {
        qWarning() << "Cannot save screenshot: view not initialized";
        return;
    }
    QImage shot = m_quickView->grabWindow();
    if (scaleFactor > 1)
        shot = shot.scaled(shot.size() * scaleFactor, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!shot.save(filename))
        qWarning() << "Failed to save screenshot to:" << filename;
}

void MoleculeViewer::saveScreenshotDialog()
{
    QString filter = tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;All Files (*)");
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), QString(), filter);
    if (filename.isEmpty())
        return;
    bool ok = false;
    int scaleFactor = QInputDialog::getInt(this, tr("Screenshot Resolution"),
        tr("Resolution multiplier (1x = current size):"), 1, 1, 4, 1, &ok);
    if (!ok)
        return;
    saveScreenshot(filename, scaleFactor);
    QMessageBox::information(this, tr("Screenshot Saved"), tr("Screenshot saved to:\n%1").arg(filename));
}

bool MoleculeViewer::exportImage(const QString& path, int width, int height, int background,
    bool ssaa)
{
    if (!m_scene || width < 1 || height < 1)
        return false;

    // A separate SceneController (deep copy) — the QQuick3DInstancing nodes belong to a
    // single scene graph and cannot be shared with the live viewer.
    SceneController ctrl;
    ctrl.cloneStateFrom(m_scene);
    ctrl.setHighQualityAA(ssaa);
    const bool transparent = (background == 2);
    if (background == 1) {
        ctrl.setTransparentBackground(false);
        ctrl.setBackgroundColor(Qt::white);
    } else {
        ctrl.setTransparentBackground(transparent);
    }

    // Offscreen render via QQuickRenderControl + QRhi. grabWindow() on a hidden window
    // returns blank with the threaded render loop; the render control drives rendering
    // synchronously on this thread into a texture we read back.
    QQuickRenderControl renderControl;
    QQuickWindow quickWindow(&renderControl);
    quickWindow.setColor(transparent ? QColor(Qt::transparent) : ctrl.backgroundColor());
#if QT_CONFIG(vulkan)
    // Vulkan needs a QVulkanInstance; reuse the live view's so we don't create a second.
    if (QQuickWindow::graphicsApi() == QSGRendererInterface::Vulkan && m_quickView
        && m_quickView->vulkanInstance())
        quickWindow.setVulkanInstance(m_quickView->vulkanInstance());
#endif

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &ctrl);
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/src/qml/viewer3d.qml")));
    if (component.isError()) {
        for (const QQmlError& e : component.errors())
            qWarning() << "exportImage qml:" << e.toString();
        return false;
    }
    QObject* rootObj = component.create(engine.rootContext());
    auto* rootItem = qobject_cast<QQuickItem*>(rootObj);
    if (!rootItem) {
        delete rootObj;
        return false;
    }
    rootItem->setParentItem(quickWindow.contentItem());
    rootItem->setSize(QSizeF(width, height));
    quickWindow.setGeometry(0, 0, width, height);

    if (!renderControl.initialize()) {
        qWarning() << "exportImage: QQuickRenderControl::initialize() failed";
        delete rootObj;
        return false;
    }
    QRhi* rhi = renderControl.rhi();
    if (!rhi) {
        delete rootObj;
        return false;
    }

    const QSize pixelSize(width, height);
    QScopedPointer<QRhiTexture> tex(rhi->newTexture(QRhiTexture::RGBA8, pixelSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    QScopedPointer<QRhiRenderBuffer> ds(
        rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, 1));
    if (!tex->create() || !ds->create()) {
        delete rootObj;
        return false;
    }
    QRhiTextureRenderTargetDescription rtDesc(QRhiColorAttachment(tex.data()));
    rtDesc.setDepthStencilBuffer(ds.data());
    QScopedPointer<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc));
    QScopedPointer<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.data());
    if (!rt->create()) {
        delete rootObj;
        return false;
    }

    quickWindow.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(rt.data()));

    renderControl.polishItems();
    renderControl.beginFrame();
    renderControl.sync();
    renderControl.render();

    QImage result;
    QRhiReadbackResult readback;
    readback.completed = [&result, &readback]() {
        const QImage img(reinterpret_cast<const uchar*>(readback.data.constData()),
            readback.pixelSize.width(), readback.pixelSize.height(),
            QImage::Format_RGBA8888_Premultiplied);
        result = img.copy();  // deep copy: readback.data is freed after the callback
    };
    QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription(tex.data()), &readback);
    renderControl.commandBuffer()->resourceUpdate(batch);
    renderControl.endFrame();  // submits + runs the readback callback

    if (rhi->isYUpInFramebuffer())
        result = result.mirrored(false, true);  // OpenGL is bottom-up

    delete rootObj;            // release the QML scene before engine/ctrl go away
    renderControl.invalidate();

    if (result.isNull())
        return false;
    result = transparent ? result.convertToFormat(QImage::Format_ARGB32)
                         : result.convertToFormat(QImage::Format_RGB888);
    return result.save(path);
}

void MoleculeViewer::exportImageDialog(const QString& startDir)
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Export Image"));
    auto* form = new QFormLayout(&dlg);

    // Default to 2x the current viewport size (true hi-res, keeps the on-screen aspect).
    const QSize cur = m_container ? m_container->size() : QSize(1280, 960);
    auto* wSpin = new QSpinBox(&dlg);
    wSpin->setRange(64, 16384);
    wSpin->setValue(qMax(64, cur.width() * 2));
    wSpin->setSuffix(tr(" px"));
    auto* hSpin = new QSpinBox(&dlg);
    hSpin->setRange(64, 16384);
    hSpin->setValue(qMax(64, cur.height() * 2));
    hSpin->setSuffix(tr(" px"));
    form->addRow(tr("Width:"), wSpin);
    form->addRow(tr("Height:"), hSpin);

    auto* bgCombo = new QComboBox(&dlg);
    bgCombo->addItem(tr("Transparent"), 2);
    bgCombo->addItem(tr("White"), 1);
    bgCombo->addItem(tr("Scene background"), 0);
    form->addRow(tr("Background:"), bgCombo);

    auto* ssaaCheck = new QCheckBox(tr("High-quality antialiasing (SSAA)"), &dlg);
    ssaaCheck->setChecked(true);
    form->addRow(QString(), ssaaCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const int bg = bgCombo->currentData().toInt();
    const QString filter = (bg == 2)
        ? tr("PNG Image (*.png)")  // alpha needs PNG
        : tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)");
    // Default to the current workspace directory (suggest a file name there).
    const QString defaultPath = startDir.isEmpty()
        ? QString()
        : QDir(startDir).filePath(QStringLiteral("molecule.png"));
    QString path = QFileDialog::getSaveFileName(this, tr("Export Image"), defaultPath, filter);
    if (path.isEmpty())
        return;
    if (!path.contains(QLatin1Char('.')))
        path += QStringLiteral(".png");

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = exportImage(path, wSpin->value(), hSpin->value(), bg, ssaaCheck->isChecked());
    QApplication::restoreOverrideCursor();
    if (ok)
        QMessageBox::information(this, tr("Export Image"), tr("Image saved to:\n%1").arg(path));
    else
        QMessageBox::warning(this, tr("Export Image"),
            tr("Failed to export the image (the offscreen render returned no content)."));
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
void MoleculeViewer::startAnimation()
{
    if (m_trajectoryAtoms.size() <= 1) {
        qWarning() << "Cannot start animation: no trajectory data";
        return;
    }
    if (m_isAnimating)
        return;
    m_isAnimating = true;
    if (!m_animationTimer) {
        m_animationTimer = new QTimer(this);
        connect(m_animationTimer, &QTimer::timeout, this, &MoleculeViewer::onAnimationTick);
    }
    m_animationTimer->start(qMax(1, 1000 / m_animationFPS));
}

void MoleculeViewer::stopAnimation()
{
    if (m_animationTimer)
        m_animationTimer->stop();
    m_isAnimating = false;
}

void MoleculeViewer::setAnimationFPS(int fps)
{
    m_animationFPS = qBound(1, fps, 60);
    if (m_isAnimating && m_animationTimer)
        m_animationTimer->setInterval(qMax(1, 1000 / m_animationFPS));
}

void MoleculeViewer::onAnimationTick()
{
    if (m_trajectoryAtoms.isEmpty()) {
        stopAnimation();
        return;
    }
    int next = m_currentFrame + 1;
    if (next >= m_trajectoryAtoms.size()) {
        if (m_animationLoop)
            next = 0;
        else {
            stopAnimation();
            return;
        }
    }
    updateFramePositions(next);
}

// ---------------------------------------------------------------------------
// Bond-edit auto-save
// ---------------------------------------------------------------------------
void MoleculeViewer::onStructureChanged()
{
    if (m_autoSaveEnabled && !m_currentFilePath.isEmpty()) {
        m_hasUnsavedChanges = true;
        m_autoSaveTimer->stop();
        m_autoSaveTimer->start();
    }
}

void MoleculeViewer::onAutoSaveTimer()
{
    if (!m_autoSaveEnabled || m_currentFilePath.isEmpty() || m_trajectoryAtoms.isEmpty())
        return;
    if (!m_hasUnsavedChanges)
        return;
    if (!QFile::exists(m_currentFilePath + ".backup"))
        QFile::copy(m_currentFilePath, m_currentFilePath + ".backup");

    XYZParser::XYZFrame xyzFrame;
    if (XYZParser::convertFromMoleculeViewer(m_trajectoryAtoms[m_currentFrame],
            QString("Auto-saved frame %1").arg(m_currentFrame + 1), xyzFrame)) {
        if (XYZParser::writeFile(m_currentFilePath, xyzFrame))
            m_hasUnsavedChanges = false;
        else
            qWarning() << "Auto-save failed for:" << m_currentFilePath;
    }
}

// ---------------------------------------------------------------------------
// Control panel (top bar) — preserved verbatim from the Qt3D version
// ---------------------------------------------------------------------------
QFrame* MoleculeViewer::createSeparator()
{
    QFrame* separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setMaximumWidth(2);
    return separator;
}

void MoleculeViewer::setupControlPanel()
{
    m_controlPanel = new QWidget;
    m_controlPanel->setMaximumHeight(50);
    m_controlPanel->setAutoFillBackground(true);
    QPalette pal = m_controlPanel->palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Base).lighter(105));
    m_controlPanel->setPalette(pal);
    m_controlPanel->setStyleSheet("QWidget { border-bottom: 1px solid palette(mid); }");

    QHBoxLayout* panelLayout = new QHBoxLayout(m_controlPanel);
    panelLayout->setContentsMargins(5, 3, 5, 3);
    panelLayout->setSpacing(5);

    // Frame navigation (hidden when frameCount == 1)
    m_frameControlWidget = new QWidget;
    QHBoxLayout* frameLayout = new QHBoxLayout(m_frameControlWidget);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(3);

    QPushButton* prevButton = new QPushButton("◀");
    prevButton->setMaximumWidth(30);
    prevButton->setToolTip(tr("Previous Frame"));
    connect(prevButton, &QPushButton::clicked, this, &MoleculeViewer::previousFrame);
    frameLayout->addWidget(prevButton);

    QPushButton* nextButton = new QPushButton("▶");
    nextButton->setMaximumWidth(30);
    nextButton->setToolTip(tr("Next Frame"));
    connect(nextButton, &QPushButton::clicked, this, &MoleculeViewer::nextFrame);
    frameLayout->addWidget(nextButton);

    m_frameSlider = new QSlider(Qt::Horizontal);
    m_frameSlider->setMinimum(0);
    m_frameSlider->setMaximum(0);
    m_frameSlider->setToolTip(tr("Navigate through trajectory frames"));
    connect(m_frameSlider, &QSlider::valueChanged, this, &MoleculeViewer::showFrame);
    frameLayout->addWidget(m_frameSlider, 1);

    m_frameLabel = new QLabel("0/0");
    m_frameLabel->setMinimumWidth(50);
    m_frameLabel->setAlignment(Qt::AlignCenter);
    frameLayout->addWidget(m_frameLabel);

    m_frameJumpBox = new QSpinBox;
    m_frameJumpBox->setMinimum(0);
    m_frameJumpBox->setMaximum(0);
    m_frameJumpBox->setMaximumWidth(60);
    m_frameJumpBox->setToolTip(tr("Jump to frame number"));
    connect(m_frameJumpBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        if (value != m_frameSlider->value()) {
            m_frameSlider->blockSignals(true);
            m_frameSlider->setValue(value);
            m_frameSlider->blockSignals(false);
        }
    });
    frameLayout->addWidget(m_frameJumpBox);

    m_frameControlWidget->setVisible(false);
    panelLayout->addWidget(m_frameControlWidget, 1);
    panelLayout->addWidget(createSeparator());

    // Playback — only shown for multi-frame files (hidden for a single structure).
    m_playbackWidget = new QWidget;
    QHBoxLayout* playbackLayout = new QHBoxLayout(m_playbackWidget);
    playbackLayout->setContentsMargins(0, 0, 0, 0);
    playbackLayout->setSpacing(3);

    QPushButton* playButton = new QPushButton;
    playButton->setIcon(QIcon::fromTheme("media-playback-start"));
    playButton->setToolTip(tr("Play Animation"));
    playButton->setMaximumWidth(30);
    connect(playButton, &QPushButton::clicked, this, &MoleculeViewer::startAnimation);
    playbackLayout->addWidget(playButton);

    QPushButton* pauseButton = new QPushButton;
    pauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseButton->setToolTip(tr("Pause Animation"));
    pauseButton->setMaximumWidth(30);
    connect(pauseButton, &QPushButton::clicked, this, &MoleculeViewer::stopAnimation);
    playbackLayout->addWidget(pauseButton);

    QSpinBox* fpsSpinBox = new QSpinBox;
    fpsSpinBox->setRange(1, 60);
    fpsSpinBox->setValue(m_animationFPS);
    fpsSpinBox->setMaximumWidth(50);
    fpsSpinBox->setSuffix(" fps");
    connect(fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        setAnimationFPS(value);
    });
    playbackLayout->addWidget(fpsSpinBox);

    QCheckBox* loopCheckbox = new QCheckBox(tr("Loop"));
    loopCheckbox->setChecked(true);
    connect(loopCheckbox, &QCheckBox::toggled, this, &MoleculeViewer::setAnimationLoop);
    playbackLayout->addWidget(loopCheckbox);

    m_playbackWidget->setVisible(false);
    panelLayout->addWidget(m_playbackWidget);
    panelLayout->addWidget(createSeparator());

    // Measurement toggle — type is auto-detected from the number of picked atoms
    // (2 = distance, 3 = angle, 4 = dihedral). Quick access; rendering style lives in the dock.
    QToolButton* measureBtn = new QToolButton;
    measureBtn->setText(tr("Measure"));
    measureBtn->setCheckable(true);
    measureBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    {
        QIcon ico = QIcon::fromTheme(QStringLiteral("measure"));
        if (ico.isNull())
            ico = QIcon::fromTheme(QStringLiteral("applications-engineering"));
        if (ico.isNull())
            ico = QIcon::fromTheme(QStringLiteral("draw-line"));
        if (!ico.isNull())
            measureBtn->setIcon(ico);
    }
    measureBtn->setToolTip(tr("Click atoms to measure: 2 = distance, 3 = angle, 4 = dihedral. "
                              "Click a marked atom again to deselect; Esc clears."));
    connect(measureBtn, &QToolButton::toggled, this, [this](bool on) { setMeasurementMode(on ? 1 : 0); });
    connect(this, &MoleculeViewer::measurementModeChanged, measureBtn, [measureBtn](int mode) {
        const bool on = (mode != 0);
        if (measureBtn->isChecked() != on) {
            measureBtn->blockSignals(true);
            measureBtn->setChecked(on);
            measureBtn->blockSignals(false);
        }
    });
    panelLayout->addWidget(measureBtn);

    // Edit toggle — structure editing: select/move atoms & molecules, copy/paste, merge,
    // with collision feedback (Claude Generated 2026). Sibling of the Measure toggle.
    QToolButton* editBtn = new QToolButton;
    editBtn->setText(tr("Edit"));
    editBtn->setCheckable(true);
    editBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    {
        QIcon ico = QIcon::fromTheme(QStringLiteral("transform-move"));
        if (ico.isNull())
            ico = QIcon::fromTheme(QStringLiteral("edit-select-all"));
        if (ico.isNull())
            ico = QIcon::fromTheme(QStringLiteral("document-edit"));
        if (!ico.isNull())
            editBtn->setIcon(ico);
    }
    editBtn->setToolTip(tr("Edit mode: click to select an atom, double-click for the whole molecule, "
                           "drag to move (Shift = depth, arrow keys = nudge). Overlapping atoms turn red."));
    connect(editBtn, &QToolButton::toggled, this, [this](bool on) { setEditMode(on); });
    connect(this, &MoleculeViewer::editModeChanged, editBtn, [editBtn](bool on) {
        if (editBtn->isChecked() != on) {
            editBtn->blockSignals(true);
            editBtn->setChecked(on);
            editBtn->blockSignals(false);
        }
    });
    panelLayout->addWidget(editBtn);

    QComboBox* colorCombo = new QComboBox;
    colorCombo->addItem(tr("CPK"), static_cast<int>(ColorScheme::CPK));
    colorCombo->addItem(tr("Monochrome"), static_cast<int>(ColorScheme::Monochrome));
    colorCombo->addItem(tr("By Charge"), static_cast<int>(ColorScheme::ByCharge));
    colorCombo->setMaximumWidth(100);
    connect(colorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, colorCombo](int index) {
        setColorScheme(static_cast<ColorScheme>(colorCombo->itemData(index).toInt()));
    });
    connect(this, &MoleculeViewer::colorSchemeChanged, colorCombo, [colorCombo](ColorScheme s) {
        const int i = colorCombo->findData(static_cast<int>(s));
        if (i >= 0 && i != colorCombo->currentIndex()) {
            colorCombo->blockSignals(true);
            colorCombo->setCurrentIndex(i);
            colorCombo->blockSignals(false);
        }
    });
    panelLayout->addWidget(colorCombo);

    // Clash status + auto-resolve (visible only in Edit mode). Claude Generated 2026.
    QLabel* clashLabel = new QLabel;
    clashLabel->setVisible(false);
    panelLayout->addWidget(clashLabel);

    QPushButton* resolveBtn = new QPushButton(tr("Resolve clashes"));
    resolveBtn->setToolTip(tr("Translate the selection away from the rest until it no longer overlaps."));
    resolveBtn->setVisible(false);
    connect(resolveBtn, &QPushButton::clicked, this, [this] { resolveClashes(); });
    panelLayout->addWidget(resolveBtn);

    connect(this, &MoleculeViewer::collisionCountChanged, this,
        [this, clashLabel, resolveBtn](int n) {
            clashLabel->setVisible(m_editMode);
            resolveBtn->setVisible(m_editMode && n > 0);
            if (n > 0) {
                clashLabel->setText(tr("⚠ %1 clash%2").arg(n).arg(n == 1 ? QString() : tr("es")));
                clashLabel->setStyleSheet(QStringLiteral("QLabel { color: #e63c3c; font-weight: bold; border: none; }"));
            } else {
                clashLabel->setText(tr("✓ no clashes"));
                clashLabel->setStyleSheet(QStringLiteral("QLabel { color: #4caf50; border: none; }"));
            }
        });
    connect(this, &MoleculeViewer::editModeChanged, this,
        [clashLabel, resolveBtn](bool on) {
            clashLabel->setVisible(on);
            if (!on)
                resolveBtn->setVisible(false);
        });

    panelLayout->addStretch();

    // Everything else (material, glow, measure, bond-edit, force, fog, lights,
    // background, …) now lives in the "Display" dock — opened by this button.
    QPushButton* displayBtn = new QPushButton(tr("Display ⚙"));
    displayBtn->setToolTip(tr("Open the Display panel (style, effects, lighting, tools)"));
    connect(displayBtn, &QPushButton::clicked, this, &MoleculeViewer::displayOptionsRequested);
    panelLayout->addWidget(displayBtn);
}
