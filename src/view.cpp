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
#include "forceinjector.h"
#include "performanceoptimizer.h"
#include "scenecontroller.h"
#include "selectionmanager.h"
#include "xyzparser.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
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
                        const int need = m_measurementMode + 1;
                        if (m_selectedAtoms.size() >= need) {
                            m_selectedAtoms.clear(); // start a fresh measurement (keep mode)
                            if (m_selectionManager)
                                m_selectionManager->clearSelection();
                        }
                        selectAtom(picked, /*append=*/true);
                        updateMeasurement();
                    } else {
                        selectAtom(picked, append);
                    }
                }
                return true;
            } else if (me->button() == Qt::RightButton) {
                m_rightMousePressed = false;
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint pos = me->position().toPoint();
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
                handleMousePan(pos);
                m_lastMousePos = pos;
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
    if (!m_scene)
        return;
    const QPoint delta = currentPos - m_lastMousePos;
    const float sens = 0.5f;
    // Camera is axis-aligned: right=+X, up=+Y. Rotate the molecule root.
    const QQuaternion horiz = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), delta.x() * sens);
    const QQuaternion vert = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -delta.y() * sens);
    m_modelRotation = (horiz * vert) * m_modelRotation;
    m_modelRotation.normalize();
    m_scene->setRootRotation(m_modelRotation);
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
    const int need = m_measurementMode + 1; // 1->2, 2->3, 3->4
    if (m_measurementMode == 0 || frame < 0 || m_selectedAtoms.size() < need) {
        m_scene->setMeasurement({}, QString());
        return;
    }
    const QVector<Atom>& atoms = m_trajectoryAtoms[frame];
    const QVector<int> idx = m_selectedAtoms.mid(m_selectedAtoms.size() - need, need);
    for (int i : idx)
        if (i < 0 || i >= atoms.size()) {
            m_scene->setMeasurement({}, QString());
            return;
        }

    QVector<QVector3D> wp; // world positions for the on-screen lines
    for (int i : idx)
        wp.append(modelToWorld(atoms[i].position));
    QVector<QPair<QVector3D, QVector3D>> lines;
    for (int i = 0; i + 1 < wp.size(); ++i)
        lines.append({ wp[i], wp[i + 1] });

    // Values from intrinsic positions (rigid → identical to world; clearer intent).
    const QVector3D p0 = atoms[idx[0]].position;
    const QVector3D p1 = atoms[idx[1]].position;
    QString text;
    if (m_measurementMode == 1) {
        text = QStringLiteral("d(%1%2–%3%4) = %5 Å")
                   .arg(atoms[idx[0]].element).arg(idx[0])
                   .arg(atoms[idx[1]].element).arg(idx[1])
                   .arg((p1 - p0).length(), 0, 'f', 3);
    } else if (m_measurementMode == 2) {
        const QVector3D p2 = atoms[idx[2]].position;
        const QVector3D v1 = (p0 - p1).normalized();
        const QVector3D v2 = (p2 - p1).normalized();
        const float ang = qRadiansToDegrees(qAcos(qBound(-1.0f, QVector3D::dotProduct(v1, v2), 1.0f)));
        text = QStringLiteral("∠(%1%2-%3%4-%5%6) = %7°")
                   .arg(atoms[idx[0]].element).arg(idx[0]).arg(atoms[idx[1]].element).arg(idx[1])
                   .arg(atoms[idx[2]].element).arg(idx[2]).arg(ang, 0, 'f', 1);
    } else { // dihedral
        const QVector3D p2 = atoms[idx[2]].position;
        const QVector3D p3 = atoms[idx[3]].position;
        const QVector3D b1 = p1 - p0, b2 = p2 - p1, b3 = p3 - p2;
        const QVector3D n1 = QVector3D::crossProduct(b1, b2);
        const QVector3D n2 = QVector3D::crossProduct(b2, b3);
        const QVector3D m1 = QVector3D::crossProduct(n1, b2.normalized());
        const float dih = qRadiansToDegrees(qAtan2(QVector3D::dotProduct(m1, n2),
            QVector3D::dotProduct(n1, n2)));
        text = QStringLiteral("φ = %1°").arg(dih, 0, 'f', 1);
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
void MoleculeViewer::syncSceneToController(int frameIndex, bool resetCamera, bool fullRebuild)
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
        m_scene->setStructure(sa, sb); // recomputes bounds + frames the camera
        m_scene->setSelection(m_selectedAtoms);
        if (!resetCamera) {
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

    if (m_bondEditor)
        m_bondEditor->setAtoms(atoms);
    if (m_perfOpt)
        m_perfOpt->setAtomCount(atoms.size());

    syncSceneToController(0, /*resetCamera=*/true, /*fullRebuild=*/true);
    buildForceAdjacency();

    m_moleculeDirty = false;
    emit moleculeUpdated(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
}

void MoleculeViewer::showOverlay(const QVector<Atom>& refAtoms, const QVector<Bond>& refBonds,
    const QVector<Atom>& targetAtoms, const QVector<Bond>& targetBonds)
{
    if (refAtoms.isEmpty()) {
        qWarning() << "showOverlay: empty reference structure";
        return;
    }
    // Reference = normal solid structure (CPK). Target = translucent CPK ghost on
    // top, so both keep correct element colours yet stay tellable apart.
    addMolecule(refAtoms, refBonds);
    if (targetAtoms.isEmpty() || !m_scene)
        return;

    const QVector<Bond> tBonds = targetBonds.isEmpty() ? detectBonds(targetAtoms) : targetBonds;
    QVector<SceneController::AtomDatum> ta;
    ta.reserve(targetAtoms.size());
    for (const Atom& a : targetAtoms)
        ta.append({ a.position, a.element, a.charge });
    QVector<SceneController::BondDatum> tb;
    tb.reserve(tBonds.size());
    for (const Bond& b : tBonds)
        tb.append({ b.atom1, b.atom2, b.bondOrder });
    m_scene->setOverlayStructure(ta, tb);
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

    // In-place position update (keep element/charge/bonds).
    auto& refAtoms = m_trajectoryAtoms[0];
    for (int i = 0; i < n; ++i)
        refAtoms[i].position = positions[i];
    syncSceneToController(0, /*resetCamera=*/false, /*fullRebuild=*/false);

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
    if (!append && !m_selectedAtoms.isEmpty())
        clearSelection();

    if (!m_selectedAtoms.contains(index)) {
        m_selectedAtoms.append(index);
        if (m_selectionManager)
            m_selectionManager->selectAtom(index, append);
        if (m_scene)
            m_scene->setSelection(m_selectedAtoms);
        emit selectionChanged(m_selectedAtoms);
    }
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
    updateMeasurement(); // refresh/clear the on-screen measurement for the new mode
}

void MoleculeViewer::setBondEditMode(int mode)
{
    m_bondEditMode = qBound(0, mode, 3);
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

    // Playback
    QPushButton* playButton = new QPushButton;
    playButton->setIcon(QIcon::fromTheme("media-playback-start"));
    playButton->setToolTip(tr("Play Animation"));
    playButton->setMaximumWidth(30);
    connect(playButton, &QPushButton::clicked, this, &MoleculeViewer::startAnimation);
    panelLayout->addWidget(playButton);

    QPushButton* pauseButton = new QPushButton;
    pauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseButton->setToolTip(tr("Pause Animation"));
    pauseButton->setMaximumWidth(30);
    connect(pauseButton, &QPushButton::clicked, this, &MoleculeViewer::stopAnimation);
    panelLayout->addWidget(pauseButton);

    QSpinBox* fpsSpinBox = new QSpinBox;
    fpsSpinBox->setRange(1, 60);
    fpsSpinBox->setValue(m_animationFPS);
    fpsSpinBox->setMaximumWidth(50);
    fpsSpinBox->setSuffix(" fps");
    connect(fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        setAnimationFPS(value);
    });
    panelLayout->addWidget(fpsSpinBox);

    QCheckBox* loopCheckbox = new QCheckBox(tr("Loop"));
    loopCheckbox->setChecked(true);
    connect(loopCheckbox, &QCheckBox::toggled, this, &MoleculeViewer::setAnimationLoop);
    panelLayout->addWidget(loopCheckbox);
    panelLayout->addWidget(createSeparator());

    // Rendering
    QComboBox* modeCombo = new QComboBox;
    modeCombo->addItem(tr("Ball & Stick"), static_cast<int>(RenderingMode::BallAndStick));
    modeCombo->addItem(tr("Space-Filling"), static_cast<int>(RenderingMode::SpaceFilling));
    modeCombo->addItem(tr("Wireframe"), static_cast<int>(RenderingMode::Wireframe));
    modeCombo->addItem(tr("Sticks"), static_cast<int>(RenderingMode::SticksOnly));
    modeCombo->setMaximumWidth(110);
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, modeCombo](int index) {
        setRenderingMode(static_cast<RenderingMode>(modeCombo->itemData(index).toInt()));
    });
    // Stay in sync with the Display dock / shortcuts.
    connect(this, &MoleculeViewer::renderingModeChanged, modeCombo, [modeCombo](RenderingMode m) {
        const int i = modeCombo->findData(static_cast<int>(m));
        if (i >= 0 && i != modeCombo->currentIndex()) {
            modeCombo->blockSignals(true);
            modeCombo->setCurrentIndex(i);
            modeCombo->blockSignals(false);
        }
    });
    panelLayout->addWidget(modeCombo);

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

    panelLayout->addStretch();

    // Everything else (material, glow, measure, bond-edit, force, fog, lights,
    // background, …) now lives in the "Display" dock — opened by this button.
    QPushButton* displayBtn = new QPushButton(tr("Display ⚙"));
    displayBtn->setToolTip(tr("Open the Display panel (style, effects, lighting, tools)"));
    connect(displayBtn, &QPushButton::clicked, this, &MoleculeViewer::displayOptionsRequested);
    panelLayout->addWidget(displayBtn);
}
