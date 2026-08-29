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
#include "widgets/elementpicker.h"  // Claude Generated 2026 - builder element strip
#include "buildtools.h"  // Claude Generated 2026 - valence accounting + auto-H
#include "fragmentlibrary.h"  // Claude Generated 2026 - built-in builder fragments
#include "settings.h"

#include "src/core/elements.h"
#include "forceinjector.h"
#include "performanceoptimizer.h"
#include "ncianalysis.h"
#include "scenecontroller.h"
#include "selectionmanager.h"
#include "xyzparser.h"

#include <QApplication>
#include <QFileInfo>
#include <algorithm>
#include <QActionGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QMenu>
#include <QCursor>
#include <QDateTime>
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
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
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
#include <QPainter>
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

MoleculeViewer::~MoleculeViewer()
{
    // Claude Generated 2026 - Tear down the QML scene before the SceneController it
    // binds to. Both are children of this widget, but m_scene is parented first, so
    // QObject would otherwise destroy it before the QML view; the "controller"
    // context property then goes null while the bindings are still live, and every
    // controller.<prop> binding in viewer3d.qml logs a harmless "Cannot read
    // property ... of null" TypeError on exit. Unloading the QML root here drops
    // those bindings first.
    if (m_quickView)
        m_quickView->setSource(QUrl());
}

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
                if (editMode() && !m_simulationActive) {
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
                if (buildMode() && !m_simulationActive) {
                    // Claude Generated 2026 - A carried fragment is dropped by the
                    // click (Shift held: drop a copy and keep carrying, Anno-style).
                    // The matching release must then do nothing, or it would count
                    // as a fresh click and place an extra atom on top of the drop.
                    if (m_carryActive) {
                        dropFragmentCarry(me->modifiers() & Qt::ShiftModifier);
                        m_buildPressConsumed = true;
                        return true;
                    }
                    // Build mode: remember the atom under the
                    // press. A drag from it moves it live (or bonds when released on
                    // another atom); a press on empty space stays a rotate-drag, and a
                    // plain click places a new atom on release. Ctrl+drag is pure
                    // navigation (rotate) even when the press hits an atom.
                    m_buildNavDrag = me->modifiers() & Qt::ControlModifier;
                    m_buildDragFrom = m_buildNavDrag ? -1 : pickAtomAtScreenPos(m_lastMousePos);
                    m_buildDragMoved = false;
                    if (m_buildDragFrom >= 0 && m_currentFrame < m_trajectoryAtoms.size()
                        && m_buildDragFrom < m_trajectoryAtoms[m_currentFrame].size())
                        m_buildDragStartPos = m_trajectoryAtoms[m_currentFrame][m_buildDragFrom].position;
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
                // Claude Generated 2026 - Build mode: middle-click on an atom
                // attaches a bonded atom of the current element; middle-click on
                // empty space keeps the usual view reset.
                if (buildMode() && !m_simulationActive) {
                    const int picked = pickAtomAtScreenPos(me->position().toPoint());
                    if (picked >= 0) {
                        buildAttachAtom(picked);
                        return true;
                    }
                }
                resetView();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_leftMousePressed = false;
                if (editMode() && !m_simulationActive) {
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
                if (buildMode() && !m_simulationActive) {
                    // Claude Generated 2026 - Build mode gestures resolve on release.
                    if (m_buildPressConsumed) {
                        m_buildPressConsumed = false;  // the press was a carry drop
                        return true;
                    }
                    const QPoint pos = me->position().toPoint();
                    const int from = m_buildDragFrom;
                    m_buildDragFrom = -1;
                    clearBuildBondPreview();  // release commits via buildBond below
                    if (m_buildNavDrag) {
                        m_buildNavDrag = false;  // Ctrl+drag was pure navigation
                        return true;
                    }
                    if (from >= 0) {
                        if (!m_leftDragged) {
                            // Click on an atom: change it to the current element
                            // (attach = middle-click, delete = right-click).
                            QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
                            if (from < atoms.size() && atoms[from].element != m_buildElement) {
                                requestBuildSnapshot();
                                setAtomInCurrentFrame(from, m_buildElement, atoms[from].position);
                            }
                        } else if (m_currentFrame < m_trajectoryAtoms.size()
                            && from < m_trajectoryAtoms[m_currentFrame].size()) {
                            QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
                            if (m_scene)
                                m_scene->setHoverAtom(-1);
                            // Proximity target = form the previewed bond, atom stays
                            // where it was pulled. Cursor directly on an already
                            // bonded neighbour = cycle that bond's order (a gesture,
                            // not a move: the atom returns to its start).
                            const int proximity = nearestBondableAtom(from);
                            const int picked = pickAtomAtScreenPos(pos, /*excludeIndex=*/from);
                            if (proximity >= 0) {
                                const float d = (atoms[proximity].position
                                    - atoms[from].position)
                                                    .length();
                                buildBond(from, proximity,
                                    build::bondOrderFromDistance(
                                        atoms[from].element, atoms[proximity].element, d));
                            } else if (picked >= 0) {
                                atoms[from].position = m_buildDragStartPos;
                                buildBond(from, picked);  // existing bond -> cycle order
                            } else if (m_buildDragMoved) {
                                // The move already happened live — finish with the
                                // usual notifications. Bonds stay as drawn; the
                                // builder never re-detects topology on a move.
                                syncSceneToController(m_currentFrame, false, false);
                                computeCollisions();
                                onStructureChanged();
                                emit moleculeUpdated(atoms, getCurrentFrameBonds());
                            }
                        }
                        m_buildDragMoved = false;
                    } else if (!m_leftDragged) {
                        placeAtomAtScreen(pos);          // click on empty space: place
                    }
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
                // Claude Generated 2026 - Right-click (no pan-drag): in Build mode
                // on an atom it deletes that atom; otherwise it opens the shared
                // display context menu (deselect lives there and on Esc).
                if (!m_rightDragged) {
                    if (buildMode() && !m_simulationActive && m_carryActive) {
                        cancelFragmentCarry();  // right-click aborts the carry
                        return true;
                    }
                    const int picked = pickAtomAtScreenPos(me->position().toPoint());
                    if (buildMode() && !m_simulationActive && picked >= 0) {
                        selectAtoms({ picked }, /*append=*/false);
                        deleteSelection();
                        return true;
                    }
                    emit contextMenuRequested(me->globalPosition().toPoint(), picked);
                }
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint pos = me->position().toPoint();
            // Claude Generated 2026 - Recover from lost release events (button let
            // go outside the view / over a popup): trust the event's live button
            // state, otherwise the viewer is stuck rotating with no button down.
            if (m_leftMousePressed && !(me->buttons() & Qt::LeftButton)) {
                m_leftMousePressed = false;
                m_movingSelection = false;
                m_rubberBanding = false;
                m_buildNavDrag = false;
                m_buildDragFrom = -1;
                m_buildDragMoved = false;
                clearBuildBondPreview();
                if (m_scene)
                    m_scene->setRubberBand(QRectF(), false);
            }
            if (m_rightMousePressed && !(me->buttons() & Qt::RightButton))
                m_rightMousePressed = false;
            if (editMode() && !m_simulationActive && m_leftMousePressed && m_movingSelection) {
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
            if (editMode() && !m_simulationActive && m_leftMousePressed && m_rubberBanding) {
                m_leftDragged = true;
                if (m_scene)
                    m_scene->setRubberBand(QRectF(m_rubberStart, pos).normalized(), true);
                m_lastMousePos = pos;
                return true;
            }
            if (buildMode() && !m_simulationActive && m_leftMousePressed && m_buildDragFrom >= 0) {
                // Claude Generated 2026 - Live drag feedback: the atom ALWAYS
                // follows the cursor (the mouse never loses it). Bond intent is
                // proximity-based: as soon as the pulled atom comes within forming
                // distance of an atom it is not yet bonded to, that atom lights up
                // and the resulting bond appears as a REAL preview bond; release
                // commits atom position and bond exactly as shown.
                if ((pos - m_leftPressPos).manhattanLength() > kBuildDragThresholdPx)
                    m_leftDragged = true;
                if (m_leftDragged && m_scene && m_quickView
                    && m_currentFrame < m_trajectoryAtoms.size()
                    && m_buildDragFrom < m_trajectoryAtoms[m_currentFrame].size()) {
                    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
                    if (!m_buildDragMoved)
                        requestBuildSnapshot();  // before the first live displacement
                    atoms[m_buildDragFrom].position = m_scene->screenToModelPoint(
                        pos.x(), pos.y(), m_buildDragStartPos,
                        m_quickView->width(), m_quickView->height());
                    const int target = nearestBondableAtom(m_buildDragFrom);
                    if (target >= 0) {
                        m_scene->setHoverAtom(target);
                        // The preview shows the bond order the current distance
                        // implies (pull closer = double/triple), updated live.
                        const float d = (atoms[target].position
                            - atoms[m_buildDragFrom].position)
                                            .length();
                        const int order = build::bondOrderFromDistance(
                            atoms[m_buildDragFrom].element, atoms[target].element, d);
                        if (target != m_buildPreviewB) {
                            clearBuildBondPreview();
                            if (m_currentFrame >= m_trajectoryBonds.size())
                                m_trajectoryBonds.resize(m_currentFrame + 1);
                            m_trajectoryBonds[m_currentFrame].append({ m_buildDragFrom, target, order });
                            m_buildPreviewA = m_buildDragFrom;
                            m_buildPreviewB = target;
                            pushBondsToScene();
                        } else {
                            QVector<Bond>& bonds = m_trajectoryBonds[m_currentFrame];
                            for (int i = bonds.size() - 1; i >= 0; --i)
                                if ((bonds[i].atom1 == m_buildPreviewA && bonds[i].atom2 == m_buildPreviewB)
                                    || (bonds[i].atom1 == m_buildPreviewB && bonds[i].atom2 == m_buildPreviewA)) {
                                    if (bonds[i].bondOrder != order) {
                                        bonds[i].bondOrder = order;
                                        pushBondsToScene();
                                    }
                                    break;
                                }
                        }
                    } else {
                        clearBuildBondPreview();
                        // Cursor directly on a bonded neighbour = order-cycle intent.
                        const int picked = pickAtomAtScreenPos(pos, m_buildDragFrom);
                        m_scene->setHoverAtom(picked);
                    }
                    m_buildDragMoved = true;
                    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/false);
                }
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
                // Claude Generated 2026 - In Build mode a click must survive small
                // hand jitter: larger drag threshold, and no rotation below it (a
                // sub-threshold move would otherwise nudge the view AND the release
                // still needs to count as a click that places an atom).
                const int threshold = buildMode() ? kBuildDragThresholdPx : 3;
                if ((pos - m_leftPressPos).manhattanLength() > threshold)
                    m_leftDragged = true;
                if (!buildMode() || m_leftDragged)
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
            // Claude Generated 2026 - A carried fragment follows the mouse.
            if (buildMode() && !m_simulationActive && m_carryActive) {
                updateFragmentCarry(pos);
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
            if (me->button() == Qt::LeftButton && buildMode() && !m_simulationActive) {
                // Claude Generated 2026 - While carrying, the double-click's second
                // press drops just like a normal press (rapid Shift+click series).
                if (m_carryActive) {
                    dropFragmentCarry(me->modifiers() & Qt::ShiftModifier);
                    m_buildPressConsumed = true;
                    return true;
                }
                // A double-click replaces the second press
                // with this event; treat it like a press so the follow-up release
                // doesn't run with stale state and place a second stacked atom.
                m_leftMousePressed = true;
                m_lastMousePos = me->position().toPoint();
                m_leftPressPos = m_lastMousePos;
                m_leftDragged = false;
                m_buildNavDrag = me->modifiers() & Qt::ControlModifier;
                m_buildDragFrom = m_buildNavDrag ? -1 : pickAtomAtScreenPos(m_lastMousePos);
                m_buildDragMoved = false;
                if (m_buildDragFrom >= 0 && m_currentFrame < m_trajectoryAtoms.size()
                    && m_buildDragFrom < m_trajectoryAtoms[m_currentFrame].size())
                    m_buildDragStartPos = m_trajectoryAtoms[m_currentFrame][m_buildDragFrom].position;
                return true;
            }
            if (me->button() == Qt::LeftButton && editMode() && !m_simulationActive) {
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
    if (nudge && editMode() && !m_selectedAtoms.isEmpty() && m_scene) {
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
        requestCoalescedSnapshot(tr("Before nudge"));  // Claude Generated 2026
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

int MoleculeViewer::pickAtomAtScreenPos(const QPoint& screenPos, int excludeIndex) const
{
    if (!m_scene || !m_quickView)
        return -1;
    return m_scene->pickAtom(screenPos.x(), screenPos.y(),
        m_quickView->width(), m_quickView->height(), excludeIndex);
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

// ---- Non-covalent interaction overlay (Claude Generated 2026) ----
//
// The geometric source is recomputed for every frame: its candidate loops are
// restricted to donor-bound hydrogens and to Cl/Br/I, so a default pass costs less
// than the bond detection the viewer already pays for on each live frame. The two
// calculated sources (GFN-FF parameters, population analysis) are pushed in from
// the analysis worker and are never triggered by a frame change - only their
// geometry is re-fitted, so the contact list stays the one that was calculated.

void MoleculeViewer::setNciSource(int source)
{
    if (m_nciSource == source)
        return;
    m_nciSource = source;
    if (m_nciSource != int(nci::Source::Geometry)) {
        // Leaving the geometric source: the contacts on screen no longer belong to
        // the newly selected source until a calculation delivers them.
        m_nciResult.contacts.clear();
        m_nciResult.summary.clear();
    }
    refreshNciOverlay();
    emit nciSourceChanged(m_nciSource);
}

void MoleculeViewer::setNciOptions(const nci::Options& options)
{
    m_nciOptions = options;
    refreshNciOverlay();
}

void MoleculeViewer::setNciLabelsVisible(bool on)
{
    m_nciLabelsVisible = on;
    if (m_scene)
        m_scene->setNciLabelsVisible(on);
}

void MoleculeViewer::setNciResult(const nci::Result& result)
{
    m_nciResult = result;
    const bool changed = (m_nciSource != int(result.source));
    m_nciSource = int(result.source);
    pushNciToScene();
    emit nciResultChanged(m_nciResult);
    if (changed)
        emit nciSourceChanged(m_nciSource);
}

void MoleculeViewer::setAtomCharges(const QVector<float>& charges)
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (charges.size() != atoms.size())
        return;
    for (int i = 0; i < atoms.size(); ++i)
        atoms[i].charge = charges[i];
    // Full rebuild: the "By Charge" scheme colours atoms AND bonds from this.
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true,
        /*keepView=*/true);
}

// ---- Fragment tinting for host-guest systems (Claude Generated 2026) ----
//
// Fragments are the connected components of the bond graph; the scene controller
// owns them because it owns the structure. The largest fragment stays in its plain
// scheme colour and the others are shifted, which is what makes a guest read as a
// guest inside a host.

QVector<QPair<QString, int>> MoleculeViewer::getFragments() const
{
    QVector<QPair<QString, int>> out;
    if (!m_scene)
        return out;
    for (const SceneController::FragmentInfo& f : m_scene->fragments())
        out.append({ f.formula, f.atomCount });
    return out;
}

void MoleculeViewer::setFragmentTint(bool on, float strength)
{
    m_fragmentTint = on;
    m_fragmentTintStrength = strength;
    if (m_scene)
        m_scene->setFragmentTint(on, strength);
}

QColor MoleculeViewer::getFragmentColor(int fragment) const
{
    return m_scene ? m_scene->fragmentColor(fragment) : QColor();
}

void MoleculeViewer::setFragmentColor(int fragment, const QColor& color)
{
    if (m_scene)
        m_scene->setFragmentColorOverride(fragment, color);
}

bool MoleculeViewer::hasFragmentColorOverride(int fragment) const
{
    return m_scene && m_scene->hasFragmentColorOverride(fragment);
}

void MoleculeViewer::setFragmentScale(float scale)
{
    m_fragmentScale = scale;
    if (m_scene)
        m_scene->setFragmentScale(scale);
}

float MoleculeViewer::getFragmentScaleFor(int fragment) const
{
    return m_scene ? m_scene->fragmentScaleFor(fragment) : 1.0f;
}

void MoleculeViewer::setFragmentScaleOverride(int fragment, float scale)
{
    if (m_scene)
        m_scene->setFragmentScaleOverride(fragment, scale);
}

float MoleculeViewer::getFragmentTintStrengthFor(int fragment) const
{
    return m_scene ? m_scene->fragmentTintStrengthFor(fragment) : 0.0f;
}

void MoleculeViewer::setFragmentTintStrengthOverride(int fragment, float strength)
{
    if (m_scene)
        m_scene->setFragmentTintStrengthOverride(fragment, strength);
}

void MoleculeViewer::resetFragmentOverrides()
{
    if (m_scene)
        m_scene->clearFragmentOverrides();
}

void MoleculeViewer::setNciKindColor(int paletteKey, const QColor& color)
{
    if (color.isValid())
        m_nciPalette.insert(paletteKey, color);
    else
        m_nciPalette.remove(paletteKey);
    pushNciToScene();
    emit nciPaletteChanged();
}

QColor MoleculeViewer::getNciKindColor(int paletteKey) const
{
    const auto it = m_nciPalette.constFind(paletteKey);
    if (it != m_nciPalette.constEnd() && it.value().isValid())
        return it.value();
    // Default colour of that key: the repulsive electrostatic key needs a positive
    // energy to select the repulsive half of the default palette.
    if (paletteKey == nci::ElectrostaticRepulsiveKey)
        return nci::kindColor(nci::Kind::Electrostatic, 1.0);
    return nci::kindColor(static_cast<nci::Kind>(paletteKey), -1.0);
}

void MoleculeViewer::resetNciKindColors()
{
    if (m_nciPalette.isEmpty())
        return;
    m_nciPalette.clear();
    pushNciToScene();
    emit nciPaletteChanged();
}

void MoleculeViewer::setNciPalette(const nci::Palette& palette)
{
    if (m_nciPalette == palette)
        return;
    m_nciPalette = palette;
    pushNciToScene();
    emit nciPaletteChanged();
}

// ---- Coarse-grained bead types (Claude Generated 2026) ----
//
// VTF beads carry a type label instead of an element, and the "By Type" scheme
// derives a stable colour from it. These forward to the scene controller, which
// owns the structure and therefore knows which types are actually present.

QVector<QPair<QString, int>> MoleculeViewer::getBeadTypes() const
{
    return m_scene ? m_scene->beadTypes() : QVector<QPair<QString, int>>();
}

QColor MoleculeViewer::getBeadTypeColor(const QString& type) const
{
    return m_scene ? m_scene->typeColor(type) : QColor();
}

void MoleculeViewer::setBeadTypeColor(const QString& type, const QColor& color)
{
    if (m_scene)
        m_scene->setTypeColorOverride(type, color);
}

void MoleculeViewer::resetBeadTypeColors()
{
    if (m_scene)
        m_scene->clearTypeColorOverrides();
}

QHash<QString, QColor> MoleculeViewer::getBeadTypeColors() const
{
    return m_scene ? m_scene->typeColorOverrides() : QHash<QString, QColor>();
}

void MoleculeViewer::setBeadTypeColors(const QHash<QString, QColor>& colors)
{
    if (m_scene)
        m_scene->setTypeColorOverrides(colors);
}

void MoleculeViewer::refreshNciOverlay()
{
    if (!m_scene)
        return;
    if (m_nciSource == 0) {
        m_scene->setNciVisible(false);
        return;
    }
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size()) {
        m_scene->setNciVisible(false);
        return;
    }

    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (atoms.size() > kNciAtomLimit) {
        m_scene->setNciVisible(false);
        return;
    }

    if (m_nciSource == int(nci::Source::Geometry)) {
        const QVector<Bond> bonds = m_currentFrame < m_trajectoryBonds.size()
            ? m_trajectoryBonds[m_currentFrame]
            : QVector<Bond>();
        if (m_nciOptions.piStacking && !m_nciRingsValid) {
            m_nciRings = nci::findAromaticRings(atoms.size(), bonds);
            m_nciRingsValid = true;
        }
        m_nciResult.source = nci::Source::Geometry;
        m_nciResult.frame = m_currentFrame;
        m_nciResult.contacts = nci::detectGeometric(atoms, bonds, m_nciOptions, &m_nciRings);
        m_nciResult.summary = nci::summarize(m_nciResult.contacts, nci::Source::Geometry);
        if (m_nciResult.contacts.size() >= m_nciOptions.maxContacts)
            m_nciResult.summary += tr(" (list capped at %1)").arg(m_nciOptions.maxContacts);
    } else {
        // Calculated source: keep the pairs, refresh their geometry for this frame.
        nci::refreshGeometry(m_nciResult.contacts, atoms, m_nciOptions);
    }
    if (m_nciLiveContacts) {
        // Live GFN-FF frame: the force field ships its full candidate enumeration,
        // so gate it down to the terms engaged in this frame before drawing.
        nci::applyGeometricGate(m_nciResult.contacts, atoms, m_nciOptions);
        const int dropped = nci::rankAndTruncate(m_nciResult.contacts, m_nciOptions);
        m_nciResult.summary = nci::summarize(m_nciResult.contacts, m_nciResult.source)
            + nci::truncationNote(dropped);
        m_nciLiveContacts = false;
    }

    pushNciToScene();
    emit nciResultChanged(m_nciResult);
}

void MoleculeViewer::pushNciToScene()
{
    if (!m_scene)
        return;
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size()) {
        m_scene->setNciVisible(false);
        return;
    }
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];

    const auto centroid = [&atoms](const QVector<int>& ring) {
        QVector3D c;
        int used = 0;
        for (int idx : ring) {
            if (idx >= 0 && idx < atoms.size()) {
                c += atoms[idx].position;
                ++used;
            }
        }
        return used > 0 ? c / float(used) : QVector3D();
    };

    QVector<SceneController::NciSegment> segs;
    segs.reserve(m_nciResult.contacts.size());
    for (const nci::Contact& c : m_nciResult.contacts) {
        SceneController::NciSegment seg;
        if (c.kind == nci::Kind::PiStacking) {
            if (c.ringA.isEmpty() || c.ringB.isEmpty())
                continue;
            seg.a = centroid(c.ringA);
            seg.b = centroid(c.ringB);   // centroids: nothing to trim against
        } else {
            const int from = c.bridge >= 0 ? c.bridge : c.donor;
            if (from < 0 || from >= atoms.size() || c.acceptor < 0 || c.acceptor >= atoms.size())
                continue;
            seg.a = atoms[from].position;
            seg.b = atoms[c.acceptor].position;
            seg.atomA = from;
            seg.atomB = c.acceptor;
        }

        // Strength is encoded twice - line thickness and alpha - so a weak contact
        // reads as a faint thin line without needing a legend.
        QColor colour = nci::kindColor(c.kind, c.energy, m_nciPalette);
        colour.setAlpha(qBound(60, int(120.0f + 135.0f * c.score), 255));
        seg.color = colour;
        seg.radius = 0.03f + 0.05f * qBound(0.0f, c.score, 1.0f);

        if (c.hasEnergy)
            seg.label = QStringLiteral("%1 kJ/mol").arg(c.energy, 0, 'f', 1);
        else
            seg.label = QStringLiteral("%1 \u00C5").arg(c.distance, 0, 'f', 2);
        segs.append(seg);
    }

    m_scene->setNciLabelsVisible(m_nciLabelsVisible);
    m_scene->setNciContacts(segs);
    m_scene->setNciVisible(!segs.isEmpty());
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
    // Claude Generated 2026 - Determine the actual change first so a no-op click
    // (add on existing bond, delete on missing one) neither snapshots nor rebuilds.
    switch (m_bondEditMode) {
    case 1: // add
        if (found >= 0)
            return;
        emit editSnapshotRequested(tr("Before bond edit"));  // pre-edit state for undo
        bonds.append({ a, b, 1 });
        break;
    case 2: // delete
        if (found < 0)
            return;
        emit editSnapshotRequested(tr("Before bond edit"));
        bonds.remove(found);
        break;
    case 3: // cycle order 1->2->3->1
        if (found < 0)
            return;
        emit editSnapshotRequested(tr("Before bond edit"));
        bonds[found].bondOrder = (bonds[found].bondOrder % 3) + 1;
        break;
    default:
        return;
    }
    // Connectivity changed: same follow-up canon as finalizeEdit() so the NCI
    // overlay, fragment tinting, clash feedback and the atom table stay current.
    refreshVisualization(); // rebuild geometry, keep the camera
    buildForceAdjacency();
    invalidateNciTopology();
    emit fragmentsChanged();
    refreshNciOverlay();
    computeCollisions();
    onStructureChanged();   // trigger XYZ auto-save (if a file is loaded)
    emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
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
            sa.append({ a.position, a.element, a.charge, a.radius, a.type });
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

    // A full rebuild means the atom or bond set itself changed (load, edit, paste,
    // delete), so ring perception has to run again - and the set of bead types may
    // have changed with it.
    if (fullRebuild) {
        invalidateNciTopology();
        emit beadTypesChanged();
        emit fragmentsChanged();
    }

    // The NCI overlay follows every geometry change through this one funnel
    // (frame change, live MD/Opt step, structure edit, refresh). Cheap no-op when
    // the overlay is off, which is the default.
    refreshNciOverlay();
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
void MoleculeViewer::appendMolecule(const QVector<Atom>& newAtoms, const QVector<Bond>& newBonds,
    bool startPlacement)
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
    if (startPlacement && !editMode())
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
        ta.append({ a.position, a.element, a.charge, a.radius, a.type });
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

// Claude Generated 2026 - First/last jump for the playback bar + shortcuts.
void MoleculeViewer::firstFrame()
{
    if (m_trajectoryAtoms.size() > 0)
        showFrame(0);
}

void MoleculeViewer::lastFrame()
{
    if (m_trajectoryAtoms.size() > 0)
        showFrame(m_trajectoryAtoms.size() - 1);
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

    // Claude Generated 2026 - Live interaction overlay: while the GFN-FF source is
    // selected the contacts come from the running force field itself, so what is
    // drawn is what the simulation actually evaluates. Distances, angles and scores
    // are re-fitted against this frame further down (refreshNciOverlay).
    if (m_nciSource == int(nci::Source::GfnffParameters) && !frame->nciContacts.isEmpty()) {
        m_nciResult.source = nci::Source::GfnffParameters;
        m_nciResult.contacts = frame->nciContacts;
        m_nciLiveContacts = true;   // needs the geometric gate in refreshNciOverlay()
    }

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
            invalidateNciTopology();  // rings may have opened/closed
            emit fragmentsChanged();  // a broken bond can split a fragment
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
    emit editSnapshotRequested(tr("Before center at origin"));  // Claude Generated 2026
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

// Claude Generated 2026 - Per-atom overlay labels (element/type/index).
void MoleculeViewer::setAtomLabelMode(AtomLabel mode)
{
    m_atomLabelMode = mode;
    if (m_scene)
        m_scene->setLabelMode(static_cast<int>(mode));
    emit atomLabelModeChanged(static_cast<int>(mode));
}

void MoleculeViewer::setLabelSelectionOnly(bool on)
{
    if (m_scene)
        m_scene->setLabelSelectionOnly(on);
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

// ===========================================================================
// Interaction modes. Claude Generated 2026.
// One exclusive mode (None/Edit/Measure/BondEdit/Build) replaces the former
// parallel bool/int flags with pairwise resets: every mode has its exit and
// entry code in exactly one place, so no mode can leave stale state or stale
// UI behind. The public setEditMode/setMeasurementMode/setBondEditMode remain
// as thin wrappers for the existing callers (bar toggles, Display panel).
// ===========================================================================
void MoleculeViewer::setInteractionMode(InteractionMode mode)
{
    if (m_mode == mode)
        return;
    const bool wasEdit = (m_mode == InteractionMode::Edit);

    // Exit code of the mode we are leaving.
    switch (m_mode) {
    case InteractionMode::Edit:
        m_movingSelection = false;
        m_emptyPressPending = false;
        m_collisionAtoms.clear();
        if (m_scene)
            m_scene->setCollisionAtoms({});
        emit collisionCountChanged(0);
        break;
    case InteractionMode::Measure:
        m_measurementMode = 0;
        updateMeasurement();  // clears the on-screen measurement
        emit measurementModeChanged(0);
        break;
    case InteractionMode::BondEdit:
        m_bondEditMode = 0;
        if (m_bondEditor)
            m_bondEditor->setEditMode(BondEditor::EditMode::None);
        emit bondEditModeChanged(0);
        break;
    case InteractionMode::Build:
        cancelFragmentCarry();  // a carried fragment does not survive the mode
        clearBuildBondPreview();
        m_buildDragFrom = -1;
        m_buildDragMoved = false;
        m_buildNavDrag = false;
        m_collisionAtoms.clear();
        if (m_scene)
            m_scene->setCollisionAtoms({});
        emit collisionCountChanged(0);
        break;
    default:
        break;
    }

    m_mode = mode;
    if (m_scene)
        m_scene->setEditHint(QString());

    // Entry code of the new mode (sub-states like the measurement type or the
    // bond-edit action are set by the wrappers after the switch).
    switch (m_mode) {
    case InteractionMode::Edit:
        computeCollisions();   // show any pre-existing clashes immediately
        if (m_scene)
            m_scene->setEditHint(tr("Edit  ·  drag: move (Shift = depth)  ·  WASD/QE: rotate"
                                    "  ·  double-click: whole molecule"
                                    "  ·  Ctrl/Shift+drag: box-select"));
        break;
    case InteractionMode::Build:
        m_buildSnapshotTimer.invalidate();  // first build edit snapshots immediately
        computeCollisions();
        updateBuildHint();
        break;
    default:
        break;
    }

    if (wasEdit != (m_mode == InteractionMode::Edit))
        emit editModeChanged(m_mode == InteractionMode::Edit);
    emit interactionModeChanged(m_mode);
}

void MoleculeViewer::setMeasurementMode(int mode)
{
    const int m = qBound(0, mode, 3);
    if (m == 0) {
        if (m_mode == InteractionMode::Measure)
            setInteractionMode(InteractionMode::None);  // exit path emits measurementModeChanged(0)
        return;
    }
    setInteractionMode(InteractionMode::Measure);
    if (m_measurementMode != m) {
        m_measurementMode = m;
        updateMeasurement(); // refresh the on-screen measurement for the new type
        emit measurementModeChanged(m_measurementMode);
    }
}

void MoleculeViewer::setBondEditMode(int mode)
{
    const int m = qBound(0, mode, 3);
    if (m == 0) {
        if (m_mode == InteractionMode::BondEdit)
            setInteractionMode(InteractionMode::None);  // exit path emits bondEditModeChanged(0)
        return;
    }
    setInteractionMode(InteractionMode::BondEdit);
    if (m_bondEditMode != m) {
        m_bondEditMode = m;
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
        emit bondEditModeChanged(m_bondEditMode);
    }
}

// Structure editing (Explore-mode "Edit" toggle): direct coordinate editing —
// select atoms/molecules, move, copy/paste, merge a file, with collision
// feedback. Distinct from the simulation grab-force, which injects forces into
// a running MD/Opt rather than mutating stored geometry.
void MoleculeViewer::setEditMode(bool on)
{
    if (on)
        setInteractionMode(InteractionMode::Edit);
    else if (m_mode == InteractionMode::Edit)
        setInteractionMode(InteractionMode::None);
}

// ===========================================================================
// Molecule builder (Build mode). Claude Generated 2026.
// Click empty space = place an atom of the current element at the selection's
// depth; click an atom = attach a bonded atom along its free valence at
// covalent distance; drag atom -> atom = add a bond / cycle its order.
// ===========================================================================
void MoleculeViewer::setBuildMode(bool on)
{
    if (on)
        setInteractionMode(InteractionMode::Build);
    else if (m_mode == InteractionMode::Build)
        setInteractionMode(InteractionMode::None);
}

void MoleculeViewer::setBuildElement(const QString& symbol)
{
    if (symbol.isEmpty() || m_buildElement == symbol)
        return;
    m_buildElement = symbol;
    if (buildMode())
        updateBuildHint();
    emit buildElementChanged(m_buildElement);
}

void MoleculeViewer::updateBuildHint()
{
    if (m_scene)
        m_scene->setEditHint(tr("Build [%1]  ·  click: place / change element"
                                "  ·  drag: move — near an atom it bonds, distance sets the order"
                                "  ·  middle-click: attach  ·  right-click: delete"
                                "  ·  Ctrl+drag: rotate"
                                "  ·  H C N O S P F L(Cl) R(Br): element")
                                 .arg(m_buildElement));
}

// One undo snapshot per 5 s of rapid edits: placing a chain atom-by-atom or
// nudging a selection stays a single Snapshots entry instead of one per input.
void MoleculeViewer::requestCoalescedSnapshot(const QString& label)
{
    if (!m_buildSnapshotTimer.isValid() || m_buildSnapshotTimer.elapsed() > 5000) {
        emit editSnapshotRequested(label);
        m_buildSnapshotTimer.start();
    }
}

void MoleculeViewer::requestBuildSnapshot()
{
    requestCoalescedSnapshot(tr("Before build edits"));
}

// Where a new substituent has the most room: opposite the average of the unit
// vectors to the bonded neighbours. No neighbours -> +X; neighbours that cancel
// (linear/symmetric coordination) -> any direction perpendicular to the first.
QVector3D MoleculeViewer::freeValenceDirection(int atomIndex) const
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return QVector3D(1, 0, 0);
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (atomIndex < 0 || atomIndex >= atoms.size())
        return QVector3D(1, 0, 0);
    QVector3D sum;
    QVector3D firstDir;
    int neighbours = 0;
    if (m_currentFrame < m_trajectoryBonds.size()) {
        for (const Bond& b : m_trajectoryBonds[m_currentFrame]) {
            int other = -1;
            if (b.atom1 == atomIndex)
                other = b.atom2;
            else if (b.atom2 == atomIndex)
                other = b.atom1;
            if (other < 0 || other >= atoms.size())
                continue;
            const QVector3D d = (atoms[other].position - atoms[atomIndex].position).normalized();
            if (neighbours == 0)
                firstDir = d;
            sum += d;
            ++neighbours;
        }
    }
    if (neighbours == 0)
        return QVector3D(1, 0, 0);
    QVector3D dir = -sum;
    if (dir.length() < 1e-3f) {
        QVector3D perp = QVector3D::crossProduct(firstDir, QVector3D(0, 0, 1));
        if (perp.length() < 1e-3f)
            perp = QVector3D::crossProduct(firstDir, QVector3D(0, 1, 0));
        return perp.normalized();
    }
    return dir.normalized();
}

// Append one atom (optionally bonded to an existing one) with the full
// post-mutation canon, so table/text/NCI/fragments/clashes stay current.
int MoleculeViewer::addAtomAt(const QVector3D& modelPos, const QString& element, int bondTo)
{
    Atom atom;
    atom.position = modelPos;
    atom.element = element;

    if (m_trajectoryAtoms.isEmpty()) {
        emit editSnapshotRequested(tr("Before first atom"));  // empty scene, undoable
        // Claude Generated 2026 - Seed the scene WITHOUT the camera reset a normal
        // load performs (addMolecule would recentre), so the first built atom
        // stays under the cursor instead of jumping to the screen centre.
        m_trajectoryAtoms.append(QVector<Atom>{ atom });
        m_trajectoryBonds.append(QVector<Bond>{});
        m_frameCount = 1;
        m_currentFrame = 0;
        m_moleculeCenter = modelPos;
        m_moleculeRadius = 10.0f;
        if (m_frameControlWidget)
            m_frameControlWidget->setVisible(false);
        if (m_playbackWidget)
            m_playbackWidget->setVisible(false);
        if (m_bondEditor)
            m_bondEditor->setAtoms(m_trajectoryAtoms[0]);
        if (m_perfOpt)
            m_perfOpt->setAtomCount(1);
        syncSceneToController(0, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
        selectAtoms({ 0 }, false);
        buildForceAdjacency();
        computeCollisions();
        onStructureChanged();
        emit moleculeUpdated(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
        return 0;
    }
    if (!canEditStructure()) {
        qWarning() << "addAtomAt: only single-frame structures can be edited";
        return -1;
    }
    requestBuildSnapshot();
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    const int index = atoms.size();
    atoms.append(atom);
    if (m_currentFrame >= m_trajectoryBonds.size())
        m_trajectoryBonds.resize(m_currentFrame + 1);
    if (bondTo >= 0 && bondTo < index)
        m_trajectoryBonds[m_currentFrame].append({ bondTo, index, 1 });

    if (m_bondEditor)
        m_bondEditor->setAtoms(atoms);
    if (m_perfOpt)
        m_perfOpt->setAtomCount(atoms.size());
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
    selectAtoms({ index }, /*append=*/false);
    buildForceAdjacency();
    invalidateNciTopology();
    emit fragmentsChanged();
    refreshNciOverlay();
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(atoms, getCurrentFrameBonds());
    return index;
}

void MoleculeViewer::placeAtomAtScreen(const QPoint& pos)
{
    // Depth reference: the selection's centroid keeps consecutive placements in
    // one plane; without a selection, the molecule centre; on an empty scene the
    // origin (the default camera looks at it, so the mapping matches the view).
    QVector3D depthRef(0, 0, 0);
    const bool haveAtoms = !m_trajectoryAtoms.isEmpty()
        && m_currentFrame < m_trajectoryAtoms.size()
        && !m_trajectoryAtoms[m_currentFrame].isEmpty();
    if (haveAtoms)
        depthRef = m_selectedAtoms.isEmpty() ? m_moleculeCenter : selectionCentroidLocal();
    QVector3D p = depthRef;
    if (m_scene && m_quickView)
        p = m_scene->screenToModelPoint(pos.x(), pos.y(), depthRef,
            m_quickView->width(), m_quickView->height());
    addAtomAt(p, m_buildElement);
}

void MoleculeViewer::buildAttachAtom(int atomIndex)
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (atomIndex < 0 || atomIndex >= atoms.size())
        return;
    const QVector3D dir = freeValenceDirection(atomIndex);
    const float dist = elem::covalentRadius(atoms[atomIndex].element)
        + elem::covalentRadius(m_buildElement);
    addAtomAt(atoms[atomIndex].position + dir * dist, m_buildElement, atomIndex);
}

// Claude Generated 2026 - Empty the scene for a fresh build. The camera is left
// alone deliberately: the next placed atom then lands under the cursor.
void MoleculeViewer::newScene()
{
    if (!m_trajectoryAtoms.isEmpty())
        emit editSnapshotRequested(tr("Before new scene"));
    stopAnimation();
    m_trajectoryAtoms.clear();
    m_trajectoryBonds.clear();
    m_frameCount = 0;
    m_currentFrame = 0;
    m_selectedAtoms.clear();
    if (m_selectionManager)
        m_selectionManager->clearSelection();
    m_collisionAtoms.clear();
    m_buildDragFrom = -1;
    m_buildPreviewA = -1;
    m_buildPreviewB = -1;
    m_nciResult.contacts.clear();
    m_nciResult.summary.clear();
    if (m_frameControlWidget)
        m_frameControlWidget->setVisible(false);
    if (m_playbackWidget)
        m_playbackWidget->setVisible(false);
    if (m_bondEditor)
        m_bondEditor->setAtoms({});
    if (m_perfOpt)
        m_perfOpt->setAtomCount(0);
    if (m_scene) {
        m_scene->clear();
        m_scene->setCollisionAtoms({});
        m_scene->setMeasurement({}, QString());
    }
    invalidateNciTopology();
    emit fragmentsChanged();
    emit selectionChanged(m_selectedAtoms);
    emit collisionCountChanged(0);
    emit moleculeUpdated({}, {});
}

// Claude Generated 2026 - Insert a library fragment as its own molecule next to
// the loaded structure; appendMolecule selects it and (outside Build mode)
// starts Edit-mode placement.
void MoleculeViewer::insertFragment(const build::Fragment& fragment)
{
    QVector<Atom> shifted = fragment.atoms;
    if (!m_trajectoryAtoms.isEmpty()
        && m_currentFrame < m_trajectoryAtoms.size()
        && !m_trajectoryAtoms[m_currentFrame].isEmpty()) {
        const QVector3D offset = m_moleculeCenter
            + QVector3D(m_moleculeRadius + 2.5f, 0, 0);
        for (Atom& a : shifted)
            a.position += offset;
    }
    appendMolecule(shifted, fragment.bonds, /*startPlacement=*/!buildMode());
}

// Claude Generated 2026 - Dock a substituent fragment: the fragment's open
// valence (free direction of its attach atom) is rotated to point toward the
// target's free valence; an H on the target that already points that way is
// consumed (as in a condensation drawing), then the connecting bond is added.
// Claude Generated 2026 - Single-atom removal with bond reindexing. No snapshot,
// no notifications: callers batch removals and run the canon themselves.
void MoleculeViewer::removeAtomAt(int index)
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (index < 0 || index >= atoms.size())
        return;
    atoms.remove(index);
    if (m_currentFrame < m_trajectoryBonds.size()) {
        QVector<Bond>& bonds = m_trajectoryBonds[m_currentFrame];
        for (int i = bonds.size() - 1; i >= 0; --i) {
            if (bonds[i].atom1 == index || bonds[i].atom2 == index) {
                bonds.remove(i);
                continue;
            }
            if (bonds[i].atom1 > index)
                --bonds[i].atom1;
            if (bonds[i].atom2 > index)
                --bonds[i].atom2;
        }
    }
}

// Claude Generated 2026 - Docking pose: align the fragment's attachment axis
// (attach -> Xx) onto the target's free valence, then scan the remaining degree
// of freedom (roll about the new bond axis, 30-degree steps) and keep the pose
// whose closest fragment/scene atom pair is farthest apart - the fragment turns
// away from whatever it would otherwise collide with.
QQuaternion MoleculeViewer::dockRotation(const QVector3D& dirF, const QVector3D& dirT,
    const QVector3D& anchor, const QVector<QVector3D>& offsets,
    const QVector<int>& ignoreSceneAtoms) const
{
    const QQuaternion align = QQuaternion::rotationTo(dirF, -dirT);
    if (offsets.isEmpty() || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return align;
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    QQuaternion best = align;
    float bestClearance = -1.0f;
    for (int deg = 0; deg < 360; deg += 30) {
        const QQuaternion q = QQuaternion::fromAxisAndAngle(-dirT, float(deg)) * align;
        float clearance = 1e9f;
        for (const QVector3D& offset : offsets) {
            const QVector3D pos = anchor + q.rotatedVector(offset);
            for (int i = 0; i < atoms.size(); ++i) {
                if (ignoreSceneAtoms.contains(i))
                    continue;
                clearance = qMin(clearance, (atoms[i].position - pos).length());
            }
        }
        if (clearance > bestClearance) {
            bestClearance = clearance;
            best = q;
        }
    }
    return best;
}

void MoleculeViewer::attachFragment(const build::Fragment& fragment, int targetAtom)
{
    if (fragment.attachAtom < 0) {
        insertFragment(fragment);
        return;
    }
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    if (!canEditStructure()) {
        qWarning() << "attachFragment: only single-frame structures can be edited";
        return;
    }
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (targetAtom < 0 || targetAtom >= atoms.size())
        return;
    emit editSnapshotRequested(tr("Before attach fragment"));
    if (m_currentFrame >= m_trajectoryBonds.size())
        m_trajectoryBonds.resize(m_currentFrame + 1);
    QVector<Bond>& bonds = m_trajectoryBonds[m_currentFrame];

    const QVector3D dirT = freeValenceDirection(targetAtom);

    // Sacrificial H: a hydrogen bonded to the target whose direction roughly
    // matches where the fragment will dock.
    int hIndex = -1;
    float bestDot = 0.7f;
    for (const Bond& b : bonds) {
        int other = -1;
        if (b.atom1 == targetAtom)
            other = b.atom2;
        else if (b.atom2 == targetAtom)
            other = b.atom1;
        if (other < 0 || other >= atoms.size()
            || atoms[other].element != QLatin1String("H"))
            continue;
        const QVector3D d = (atoms[other].position - atoms[targetAtom].position).normalized();
        const float dot = QVector3D::dotProduct(d, dirT);
        if (dot > bestDot) {
            bestDot = dot;
            hIndex = other;
        }
    }
    if (hIndex >= 0) {
        removeAtomAt(hIndex);
        if (targetAtom > hIndex)
            --targetAtom;
    }

    // Fixed bonding site: the fragment's "Xx" attachment-point atom (curcuma
    // polymerbuild convention) defines the bond direction; it is consumed by the
    // docking. Fragments without one fall back to the free-valence estimate.
    int xxLocal = -1;
    for (int i = 0; i < fragment.atoms.size(); ++i)
        if (fragment.atoms[i].element == QLatin1String("Xx")) {
            xxLocal = i;
            break;
        }
    const QVector3D attachLocal = fragment.atoms[fragment.attachAtom].position;
    QVector3D dirF(1, 0, 0);
    if (xxLocal >= 0) {
        dirF = (fragment.atoms[xxLocal].position - attachLocal).normalized();
    } else {
        QVector3D sum;
        for (const Bond& b : fragment.bonds) {
            int other = -1;
            if (b.atom1 == fragment.attachAtom)
                other = b.atom2;
            else if (b.atom2 == fragment.attachAtom)
                other = b.atom1;
            if (other >= 0 && other < fragment.atoms.size())
                sum += (fragment.atoms[other].position - attachLocal).normalized();
        }
        if (sum.lengthSquared() > 1e-6f)
            dirF = (-sum).normalized();
    }

    // Put the attach atom at covalent-bond distance along the target's valence
    // and pick the docking pose with the most clearance (dockRotation).
    const float dist = elem::covalentRadius(atoms[targetAtom].element)
        + elem::covalentRadius(fragment.atoms[fragment.attachAtom].element);
    const QVector3D anchor = atoms[targetAtom].position + dirT * dist;
    QVector<QVector3D> offsets;
    for (int i = 0; i < fragment.atoms.size(); ++i)
        if (i != fragment.attachAtom && i != xxLocal)
            offsets.append(fragment.atoms[i].position - attachLocal);
    const QQuaternion rot = dockRotation(dirF, dirT, anchor, offsets, { targetAtom });

    // Append the fragment WITHOUT its Xx; remap the internal bond indices.
    const int base = atoms.size();
    QVector<int> map(fragment.atoms.size(), -1);
    for (int i = 0; i < fragment.atoms.size(); ++i) {
        if (i == xxLocal)
            continue;
        map[i] = int(atoms.size());
        Atom copy = fragment.atoms[i];
        copy.position = anchor + rot.rotatedVector(fragment.atoms[i].position - attachLocal);
        atoms.append(copy);
    }
    for (const Bond& fb : fragment.bonds)
        if (map[fb.atom1] >= 0 && map[fb.atom2] >= 0)
            bonds.append({ map[fb.atom1], map[fb.atom2], fb.bondOrder });
    bonds.append({ targetAtom, map[fragment.attachAtom], 1 });

    if (m_bondEditor)
        m_bondEditor->setAtoms(atoms);
    if (m_perfOpt)
        m_perfOpt->setAtomCount(atoms.size());
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
    QVector<int> added;
    for (int i = base; i < atoms.size(); ++i)
        added.append(i);
    selectAtoms(added, /*append=*/false);
    buildForceAdjacency();
    invalidateNciTopology();
    emit fragmentsChanged();
    refreshNciOverlay();
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(atoms, getCurrentFrameBonds());
}

// Claude Generated 2026 - Live bond preview helpers: while a build drag hovers a
// target atom, the bond that release will create is inserted as a REAL bond so
// the user sees exactly what forms; it is removed when the drag leaves the
// target or ends. Only the bond list is pushed (no bounds/camera change).
void MoleculeViewer::pushBondsToScene()
{
    if (!m_scene || m_currentFrame < 0 || m_currentFrame >= m_trajectoryBonds.size())
        return;
    const QVector<Bond>& bonds = m_trajectoryBonds[m_currentFrame];
    QVector<SceneController::BondDatum> sb;
    sb.reserve(bonds.size());
    for (const Bond& b : bonds)
        sb.append({ b.atom1, b.atom2, b.bondOrder });
    m_scene->updateBonds(sb);
}

// Claude Generated 2026 - Proximity-based bond intent: the nearest atom within
// kBuildBondFormFactor * (rcov_a + rcov_b) that `from` is not yet bonded to.
// The live preview bond itself is ignored, otherwise the current target would
// count as "already bonded" and the intent would flicker off every move.
int MoleculeViewer::nearestBondableAtom(int from, const QVector<int>& exclude) const
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return -1;
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    if (from < 0 || from >= atoms.size())
        return -1;
    const QVector<Bond> bonds = (m_currentFrame < m_trajectoryBonds.size())
        ? m_trajectoryBonds[m_currentFrame]
        : QVector<Bond>();
    const float rFrom = elem::covalentRadius(atoms[from].element);
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < atoms.size(); ++i) {
        if (i == from || exclude.contains(i))
            continue;
        const float d = (atoms[i].position - atoms[from].position).length();
        const float form = (rFrom + elem::covalentRadius(atoms[i].element)) * kBuildBondFormFactor;
        if (d >= form || d >= bestDist)
            continue;
        bool bonded = false;
        for (const Bond& b : bonds) {
            if ((b.atom1 == from && b.atom2 == i) || (b.atom1 == i && b.atom2 == from)) {
                if (from == m_buildPreviewA && i == m_buildPreviewB)
                    continue;  // the preview bond does not count
                bonded = true;
                break;
            }
        }
        if (bonded)
            continue;
        best = i;
        bestDist = d;
    }
    return best;
}

void MoleculeViewer::clearBuildBondPreview()
{
    if (m_buildPreviewA < 0)
        return;
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryBonds.size()) {
        QVector<Bond>& bonds = m_trajectoryBonds[m_currentFrame];
        for (int i = bonds.size() - 1; i >= 0; --i)
            if ((bonds[i].atom1 == m_buildPreviewA && bonds[i].atom2 == m_buildPreviewB)
                || (bonds[i].atom1 == m_buildPreviewB && bonds[i].atom2 == m_buildPreviewA)) {
                bonds.remove(i);
                break;
            }
        pushBondsToScene();
    }
    m_buildPreviewA = -1;
    m_buildPreviewB = -1;
}

// ===========================================================================
// Fragment carry: the fragment hangs on the mouse until it is dropped.
// Claude Generated 2026.
// ===========================================================================
void MoleculeViewer::startFragmentCarry(const build::Fragment& fragment)
{
    cancelFragmentCarry();
    if (!buildMode())
        setBuildMode(true);
    if (!m_trajectoryAtoms.isEmpty() && !canEditStructure())
        return;  // insertFragment would refuse anyway (multi-frame)
    insertFragment(fragment);  // snapshot + append + select
    if (m_selectedAtoms.size() != fragment.atoms.size())
        return;  // insertion did not happen
    m_carryAtoms = m_selectedAtoms;
    m_carryAttach = (fragment.attachAtom >= 0)
        ? m_carryAtoms.value(fragment.attachAtom, -1)
        : -1;
    m_carryFragment = &fragment;
    m_carryActive = true;
    if (m_scene)
        m_scene->setEditHint(tr("Carrying %1  ·  move: position it"
                                "  ·  near an atom: bond preview"
                                "  ·  click: drop  ·  Shift+click: drop a copy & keep carrying"
                                "  ·  right-click/Esc: cancel")
                                 .arg(fragment.name));
    // Claude Generated 2026 - Move it under the cursor immediately (same event,
    // before the next rendered frame) — otherwise the fragment is briefly
    // visible at its insertion position until the first mouse move. A cursor
    // outside the viewport (dropdown menu) is clamped to the viewport edge.
    if (m_container) {
        QPoint pos = m_container->mapFromGlobal(QCursor::pos());
        pos.setX(qBound(0, pos.x(), qMax(1, m_container->width())));
        pos.setY(qBound(0, pos.y(), qMax(1, m_container->height())));
        updateFragmentCarry(pos);
    }
}

// Translate the carried group so its centroid sits under the cursor, and show
// the bond its attach atom would form with the nearest unbonded neighbour.
void MoleculeViewer::updateFragmentCarry(const QPoint& pos)
{
    if (!m_carryActive || !m_scene || !m_quickView
        || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    QVector3D centroid;
    for (int idx : m_carryAtoms) {
        if (idx < 0 || idx >= atoms.size())
            return;  // structure changed under us — safety net
        centroid += atoms[idx].position;
    }
    centroid /= float(m_carryAtoms.size());
    const QVector3D to = m_scene->screenToModelPoint(pos.x(), pos.y(), centroid,
        m_quickView->width(), m_quickView->height());
    const QVector3D delta = to - centroid;
    for (int idx : m_carryAtoms)
        atoms[idx].position += delta;

    const int target = (m_carryAttach >= 0)
        ? nearestBondableAtom(m_carryAttach, m_carryAtoms)
        : -1;
    if (target >= 0) {
        m_scene->setHoverAtom(target);
        const float d = (atoms[target].position - atoms[m_carryAttach].position).length();
        const int order = build::bondOrderFromDistance(
            atoms[m_carryAttach].element, atoms[target].element, d);
        if (target != m_buildPreviewB) {
            clearBuildBondPreview();
            if (m_currentFrame >= m_trajectoryBonds.size())
                m_trajectoryBonds.resize(m_currentFrame + 1);
            m_trajectoryBonds[m_currentFrame].append({ m_carryAttach, target, order });
            m_buildPreviewA = m_carryAttach;
            m_buildPreviewB = target;
            pushBondsToScene();
        }
    } else {
        clearBuildBondPreview();
        m_scene->setHoverAtom(-1);
    }
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/false);
}

void MoleculeViewer::dropFragmentCarry(bool keepCarrying)
{
    if (!m_carryActive)
        return;
    clearBuildBondPreview();
    const int attach = m_carryAttach;
    QVector<int> carried = m_carryAtoms;
    const build::Fragment* fragment = m_carryFragment;
    m_carryActive = false;
    m_carryAtoms.clear();
    m_carryAttach = -1;
    m_carryFragment = nullptr;
    if (m_scene)
        m_scene->setHoverAtom(-1);
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    const int target = (attach >= 0) ? nearestBondableAtom(attach, carried) : -1;
    if (target >= 0 && fragment) {
        // Claude Generated 2026 - Proper docking: the free-floating copy is
        // replaced by attachFragment's placement (Xx axis aligned onto the
        // target's valence, roll chosen for maximum clearance, Xx and a
        // sacrificial target H consumed). The carried atoms are the appended
        // tail, so removing them keeps the target's index stable.
        std::sort(carried.begin(), carried.end(), std::greater<int>());
        for (int idx : carried)
            removeAtomAt(idx);
        attachFragment(*fragment, target);
    } else {
        // Free drop: the Xx attachment point does not survive outside a carry —
        // the open bonding site is tracked by the valence indicator instead.
        int xxGlobal = -1;
        for (int idx : carried)
            if (idx >= 0 && idx < atoms.size() && atoms[idx].element == QLatin1String("Xx"))
                xxGlobal = idx;
        if (xxGlobal >= 0) {
            removeAtomAt(xxGlobal);
            QVector<int> keep;
            for (int idx : carried) {
                if (idx == xxGlobal)
                    continue;
                keep.append(idx > xxGlobal ? idx - 1 : idx);
            }
            selectAtoms(keep, /*append=*/false);
            syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
            buildForceAdjacency();
            invalidateNciTopology();
            emit fragmentsChanged();
            refreshNciOverlay();
        }
        computeCollisions();
        onStructureChanged();
        emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
    }
    if (keepCarrying && fragment) {
        startFragmentCarry(*fragment);  // Shift held: pick up the next copy
        return;
    }
    updateBuildHint();
}

void MoleculeViewer::cancelFragmentCarry()
{
    if (!m_carryActive)
        return;
    clearBuildBondPreview();
    const QVector<int> carried = m_carryAtoms;
    m_carryActive = false;
    m_carryAtoms.clear();
    m_carryAttach = -1;
    m_carryFragment = nullptr;
    if (m_scene)
        m_scene->setHoverAtom(-1);
    selectAtoms(carried, /*append=*/false);
    deleteSelection();  // removes the carried atoms again (own undo snapshot)
    if (buildMode())
        updateBuildHint();
}

int MoleculeViewer::openValenceCount() const
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return 0;
    const QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    const QVector<Bond> bonds = getCurrentFrameBonds();
    int open = 0;
    for (int i = 0; i < atoms.size(); ++i)
        if (elem::isElementSymbol(atoms[i].element))
            open += build::openValence(i, atoms, bonds);
    return open;
}

void MoleculeViewer::addHydrogens(const QVector<int>& targets)
{
    if (m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    if (!canEditStructure()) {
        qWarning() << "addHydrogens: only single-frame structures can be edited";
        return;
    }
    QVector<Atom> newH;
    QVector<Bond> newBonds;
    build::generateHydrogens(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds(),
        targets, newH, newBonds);
    if (newH.isEmpty())
        return;
    emit editSnapshotRequested(tr("Before add hydrogens"));
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    atoms += newH;
    if (m_currentFrame >= m_trajectoryBonds.size())
        m_trajectoryBonds.resize(m_currentFrame + 1);
    m_trajectoryBonds[m_currentFrame] += newBonds;  // indices are already absolute

    if (m_bondEditor)
        m_bondEditor->setAtoms(atoms);
    if (m_perfOpt)
        m_perfOpt->setAtomCount(atoms.size());
    syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
    buildForceAdjacency();
    invalidateNciTopology();
    emit fragmentsChanged();
    refreshNciOverlay();
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(atoms, getCurrentFrameBonds());
}

void MoleculeViewer::buildBond(int a, int b, int newOrder)
{
    if (a == b || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size())
        return;
    if (m_currentFrame >= m_trajectoryBonds.size())
        m_trajectoryBonds.resize(m_currentFrame + 1);
    QVector<Bond>& bonds = m_trajectoryBonds[m_currentFrame];
    int found = -1;
    for (int i = 0; i < bonds.size(); ++i)
        if ((bonds[i].atom1 == a && bonds[i].atom2 == b)
            || (bonds[i].atom1 == b && bonds[i].atom2 == a)) {
            found = i;
            break;
        }
    requestBuildSnapshot();
    if (found < 0)
        bonds.append({ a, b, qBound(1, newOrder, 3) });
    else
        bonds[found].bondOrder = (bonds[found].bondOrder % 3) + 1;  // 1->2->3->1

    // Claude Generated 2026 - Keep hydrogens consistent: raising a bond order can
    // over-saturate an endpoint that was already H-saturated ("build ring, add H,
    // then aromatize") — such endpoints give up one bonded H per excess valence.
    QVector<Atom>& atoms = m_trajectoryAtoms[m_currentFrame];
    QVector<int> doomed = build::excessHydrogens(a, atoms, bonds);
    for (int h : build::excessHydrogens(b, atoms, bonds))
        if (!doomed.contains(h))
            doomed.append(h);
    doomed.removeAll(a);  // never remove the two atoms just bonded
    doomed.removeAll(b);
    if (!doomed.isEmpty()) {
        std::sort(doomed.begin(), doomed.end(), std::greater<int>());
        for (int h : doomed) {
            atoms.remove(h);
            for (int i = bonds.size() - 1; i >= 0; --i) {
                if (bonds[i].atom1 == h || bonds[i].atom2 == h) {
                    bonds.remove(i);
                    continue;
                }
                if (bonds[i].atom1 > h)
                    --bonds[i].atom1;
                if (bonds[i].atom2 > h)
                    --bonds[i].atom2;
            }
        }
        clearSelection();  // indices shifted
        if (m_bondEditor)
            m_bondEditor->setAtoms(atoms);
        if (m_perfOpt)
            m_perfOpt->setAtomCount(atoms.size());
        syncSceneToController(m_currentFrame, /*resetCamera=*/false, /*fullRebuild=*/true, /*keepView=*/true);
    } else {
        refreshVisualization();
    }
    buildForceAdjacency();
    invalidateNciTopology();
    emit fragmentsChanged();
    refreshNciOverlay();
    computeCollisions();
    onStructureChanged();
    emit moleculeUpdated(m_trajectoryAtoms[m_currentFrame], getCurrentFrameBonds());
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
    emit editSnapshotRequested(tr("Before apply structure text"));  // Claude Generated 2026
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
            invalidateNciTopology();
            emit fragmentsChanged();
            refreshNciOverlay();  // the 1-2/1-3 exclusion set just changed
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
    emit editSnapshotRequested(tr("Before resolve clashes"));  // Claude Generated 2026
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
// Claude Generated 2026 - getAtomColor()/getAtomRadius() removed: dead legacy
// helpers superseded by SceneController::schemeColor()/atomDrawRadius(), which
// are what the renderer actually uses.
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
// Claude Generated 2026 - Reproducible view presets (camera + display).
ViewPreset MoleculeViewer::currentViewPreset(ZoomMode zoomMode) const
{
    ViewPreset p;
    p.zoomMode = zoomMode;
    if (m_scene) {
        p.rootRotation = m_scene->rootRotation();
        p.pan = m_scene->pan();
        p.cameraDistance = m_scene->cameraDistance();
        p.fieldOfView = m_scene->fieldOfView();
        p.sceneExtent = m_scene->sceneExtent();
        const float extent = qMax(p.sceneExtent, 1e-3f);
        p.zoomFactor = p.cameraDistance / extent;
    }

    static_cast<DisplaySettings&>(p) = currentDisplaySettings();
    p.fogDistance = m_fogDistance;
    p.backgroundColor = m_backgroundColor;
    for (int i = 0; i < 4; ++i)
        p.cornerLightEnabled[i] = m_cornerLightEnabled[i];

    return p;
}

void MoleculeViewer::applyViewPreset(const ViewPreset& preset, bool applyCamera, bool applyDisplay)
{
    if (applyCamera && m_scene) {
        float distance = preset.cameraDistance;
        if (preset.zoomMode == ZoomMode::Relative) {
            const float extent = qMax(m_scene->sceneExtent(), 1e-3f);
            distance = preset.zoomFactor * extent;
        }
        // Keep m_modelRotation in sync so mouse rotation continues from the
        // preset instead of snapping back to the pre-preset orientation.
        m_modelRotation = preset.rootRotation.normalized();
        if (distance > 0.0f)
            m_scene->setCameraTransform(preset.rootRotation, preset.pan, distance);
        else
            m_scene->resetView(); // no usable zoom -> frame molecule cleanly
        if (m_quickView)
            m_quickView->update();
    }

    if (!applyDisplay) {
        emit viewPresetApplied();
        return;
    }

    setFogDistance(preset.fogDistance);
    setBackgroundColor(preset.backgroundColor);
    for (int i = 0; i < 4; ++i)
        setCornerLightEnabled(i, preset.cornerLightEnabled[i]);
    applyDisplaySettings(preset, /*allowComputedNciSource=*/true);

    emit viewPresetApplied(); // DisplayPanel re-syncs its controls (no dock raise)
}

// Claude Generated 2026 - The viewer's complete live display state. Single source
// of truth: UI panels sync from this struct, persistence saves it verbatim.
DisplaySettings MoleculeViewer::currentDisplaySettings() const
{
    DisplaySettings s;
    s.renderingMode = static_cast<int>(m_renderingMode);
    s.colorScheme = static_cast<int>(m_colorScheme);
    s.atomTransparency = m_atomTransparency;
    s.atomShininess = m_atomShininess;
    s.atomScaleFactor = m_atomScaleFactor;
    s.bondThickness = m_bondThickness;
    s.fogEnabled = m_fogEnabled;
    s.fogIntensity = m_fogIntensity;
    s.ssaoEnabled = m_ssaoEnabled;
    s.ssaoIntensity = m_ssaoIntensity;
    s.ssaoRadius = m_ssaoRadius;
    s.ssaoBias = m_ssaoBias;
    s.bloomEnabled = m_bloomEnabled;
    s.bloomThreshold = m_bloomThreshold;
    s.bloomIntensity = m_bloomIntensity;
    s.hdrEnabled = m_hdrEnabled;
    s.exposure = m_exposure;
    s.rotationMode = static_cast<int>(m_rotationMode);
    s.wallVisible = m_wallVisibleOverride;
    s.wallOpacity = getWallOpacity();
    s.nciSource = m_nciSource;
    s.nciHydrogenBonds = m_nciOptions.hydrogenBonds;
    s.nciHalogenBonds = m_nciOptions.halogenBonds;
    s.nciPiStacking = m_nciOptions.piStacking;
    s.nciCloseContacts = m_nciOptions.closeContacts;
    s.nciElectrostatics = m_nciOptions.electrostatics;
    s.nciDispersion = m_nciOptions.dispersion;
    s.nciHbDistance = m_nciOptions.hbMaxDistance;
    s.nciHbAngle = m_nciOptions.hbMinAngle;
    s.nciLabels = m_nciLabelsVisible;
    s.nciLiveMd = m_nciLiveMd;
    s.fragmentTint = m_fragmentTint;
    s.fragmentTintStrength = m_fragmentTintStrength;
    s.fragmentScale = m_fragmentScale;
    return s;
}

// Claude Generated 2026 - Apply a full display-state struct (startup, Reset,
// presets). Counterpart of currentDisplaySettings(); the field-by-field setter
// lists that used to live in applyViewPreset() and DisplayPanel are gone.
void MoleculeViewer::applyDisplaySettings(const DisplaySettings& s, bool allowComputedNciSource)
{
    setRenderingMode(static_cast<RenderingMode>(s.renderingMode));
    setColorScheme(static_cast<ColorScheme>(s.colorScheme));
    setAtomTransparency(s.atomTransparency);
    setAtomShininess(s.atomShininess);
    setAtomScaleFactor(s.atomScaleFactor);
    setBondThickness(s.bondThickness);
    setFogEnabled(s.fogEnabled);
    setFogIntensity(s.fogIntensity);
    setSSAOEnabled(s.ssaoEnabled);
    setSSAOIntensity(s.ssaoIntensity);
    setSSAORadius(s.ssaoRadius);
    setSSAOBias(s.ssaoBias);
    setBloomEnabled(s.bloomEnabled);
    setBloomThreshold(s.bloomThreshold);
    setBloomIntensity(s.bloomIntensity);
    setHDREnabled(s.hdrEnabled);
    setExposure(s.exposure);
    setRotationMode(s.rotationMode);
    setWallVisibleOverride(s.wallVisible);
    setWallOpacity(s.wallOpacity);
    {
        nci::Options o = m_nciOptions;
        o.hydrogenBonds = s.nciHydrogenBonds;
        o.halogenBonds = s.nciHalogenBonds;
        o.piStacking = s.nciPiStacking;
        o.closeContacts = s.nciCloseContacts;
        o.electrostatics = s.nciElectrostatics;
        o.dispersion = s.nciDispersion;
        o.hbMaxDistance = s.nciHbDistance;
        o.hbMinAngle = s.nciHbAngle;
        m_nciOptions = o;
    }
    setFragmentTint(s.fragmentTint, s.fragmentTintStrength);
    setFragmentScale(s.fragmentScale);
    setNciLabelsVisible(s.nciLabels);
    m_nciLiveMd = s.nciLiveMd;
    // Calculated sources (>= 2) only make sense when analysis results follow.
    const int source = (s.nciSource <= 1 || allowComputedNciSource) ? s.nciSource : 0;
    setNciSource(source);
    refreshNciOverlay();  // setNciSource() no-ops when the source is unchanged

    applyAppearanceToController();

    emit renderingModeChanged(m_renderingMode);
    emit colorSchemeChanged(m_colorScheme);
}

void MoleculeViewer::setCameraOrientation(const QQuaternion& rotation)
{
    if (!m_scene)
        return;
    // Keep m_modelRotation in sync so mouse rotation continues from this
    // orientation instead of snapping back to the previous one.
    m_modelRotation = rotation.normalized();
    m_scene->setRootRotationOnly(rotation);
    if (m_quickView)
        m_quickView->update();
}

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
    bool ssaa, const ImageMetadata& metadata, const QColor& bgColor)
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
    } else if (background == 3 && bgColor.isValid()) {
        ctrl.setTransparentBackground(false);
        ctrl.setBackgroundColor(bgColor);
    } else {
        ctrl.setTransparentBackground(transparent);
    }

    // Offscreen render via QQuickRenderControl + QRhi. grabWindow() on a hidden window
    // returns blank with the threaded render loop; the render control drives rendering
    // synchronously on this thread into a texture we read back.
    QQuickRenderControl renderControl;
    QQuickWindow quickWindow(&renderControl);
    quickWindow.setColor(transparent ? QColor(Qt::transparent) : ctrl.backgroundColor());
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
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
    // Claude Generated 2026 - QScopedPointer so every early return below releases the
    // QML scene automatically (was 6 hand-written `delete rootObj`, one per exit path).
    QScopedPointer<QObject> root(component.create(engine.rootContext()));
    auto* rootItem = qobject_cast<QQuickItem*>(root.data());
    if (!rootItem)
        return false;
    rootItem->setParentItem(quickWindow.contentItem());
    rootItem->setSize(QSizeF(width, height));
    quickWindow.setGeometry(0, 0, width, height);

    if (!renderControl.initialize()) {
        qWarning() << "exportImage: QQuickRenderControl::initialize() failed";
        return false;
    }
    QRhi* rhi = renderControl.rhi();
    if (!rhi)
        return false;

    const QSize pixelSize(width, height);
    QScopedPointer<QRhiTexture> tex(rhi->newTexture(QRhiTexture::RGBA8, pixelSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    QScopedPointer<QRhiRenderBuffer> ds(
        rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, 1));
    if (!tex->create() || !ds->create())
        return false;
    QRhiTextureRenderTargetDescription rtDesc(QRhiColorAttachment(tex.data()));
    rtDesc.setDepthStencilBuffer(ds.data());
    QScopedPointer<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc));
    QScopedPointer<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.data());
    if (!rt->create())
        return false;

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

    if (rhi->isYUpInFramebuffer()) {  // OpenGL is bottom-up
        // Claude Generated 2026 - QImage::flipped() is Qt 6.9+; CI pins 6.8.2, so
        // fall back to the (now-deprecated) mirrored() there. mirrored(false, true)
        // == flipped(Qt::Vertical).
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        result = result.flipped(Qt::Vertical);
#else
        result = result.mirrored(false, true);
#endif
    }

    root.reset();              // release the QML scene before engine/ctrl go away
    renderControl.invalidate();

    if (result.isNull())
        return false;
    result = transparent ? result.convertToFormat(QImage::Format_ARGB32)
                         : result.convertToFormat(QImage::Format_RGB888);

    // Claude Generated 2026 - embed reproducibility/authorship as PNG text chunks.
    if (metadata.embed) {
        auto quat = metadata.cameraRotation;
        result.setText(QStringLiteral("Software"),
                       QStringLiteral("Qurcuma %1").arg(metadata.qurcumaVersion));
        result.setText(QStringLiteral("ExportTimestamp"), metadata.exportTimestamp);
        result.setText(QStringLiteral("ImageWidth"), QString::number(metadata.width));
        result.setText(QStringLiteral("ImageHeight"), QString::number(metadata.height));

        for (int i = 0; i < metadata.sourceFiles.size(); ++i) {
            const QString key = (i == 0) ? QStringLiteral("SourceFile")
                                          : QStringLiteral("SourceFile%1").arg(i + 1);
            result.setText(key, metadata.sourceFiles[i]);
        }

        result.setText(QStringLiteral("CameraRotation"),
                       QStringLiteral("%1,%2,%3,%4").arg(quat.scalar()).arg(quat.x()).arg(quat.y()).arg(quat.z()));
        result.setText(QStringLiteral("CameraDistance"), QString::number(metadata.cameraDistance));
        result.setText(QStringLiteral("CameraPan"),
                       QStringLiteral("%1,%2,%3").arg(metadata.cameraPan.x()).arg(metadata.cameraPan.y()).arg(metadata.cameraPan.z()));
        result.setText(QStringLiteral("ZoomMode"),
                       metadata.zoomMode == ZoomMode::Relative ? QStringLiteral("Relative")
                                                               : QStringLiteral("Absolute"));
        result.setText(QStringLiteral("ZoomFactor"), QString::number(metadata.zoomFactor));

        result.setText(QStringLiteral("RenderingMode"), QString::number(metadata.renderingMode));
        result.setText(QStringLiteral("ColorScheme"), QString::number(metadata.colorScheme));
        result.setText(QStringLiteral("AtomScale"), QString::number(metadata.atomScaleFactor));
        result.setText(QStringLiteral("BondThickness"), QString::number(metadata.bondThickness));
        result.setText(QStringLiteral("AtomTransparency"), QString::number(metadata.atomTransparency));
        result.setText(QStringLiteral("BackgroundColor"),
                       QStringLiteral("%1,%2,%3,%4")
                           .arg(metadata.backgroundColor.red()).arg(metadata.backgroundColor.green())
                           .arg(metadata.backgroundColor.blue()).arg(metadata.backgroundColor.alpha()));
        if (!metadata.effects.isEmpty())
            result.setText(QStringLiteral("Effects"), metadata.effects);

        if (!metadata.authorName.isEmpty())
            result.setText(QStringLiteral("Author"), metadata.authorName);
        if (!metadata.authorOrcid.isEmpty())
            result.setText(QStringLiteral("AuthorORCID"), metadata.authorOrcid);
        if (!metadata.authorInstitution.isEmpty())
            result.setText(QStringLiteral("AuthorInstitution"), metadata.authorInstitution);
        if (!metadata.license.isEmpty())
            result.setText(QStringLiteral("License"), metadata.license);

        if (!metadata.viewPresetName.isEmpty())
            result.setText(QStringLiteral("ViewPreset"), metadata.viewPresetName);
    }

    return result.save(path);
}

void MoleculeViewer::exportImageDialog(const QString& startDir, Settings* settings)
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

    // Claude Generated 2026 - reproducibility metadata + optional view preset.
    auto* embedCheck = new QCheckBox(tr("Embed metadata (PNG)"), &dlg);
    embedCheck->setChecked(true);
    embedCheck->setToolTip(tr("Write source file, camera, display settings and operator "
                              "authorship as PNG text chunks. JPEG/TIFF get no metadata (Qt6 cannot write EXIF)."));
    form->addRow(QString(), embedCheck);

    auto* presetCombo = new QComboBox(&dlg);
    presetCombo->addItem(tr("(none)"), QString());
    if (settings) {
        for (const ViewPreset& p : settings->viewPresets())
            presetCombo->addItem(p.name, p.name);
    }
    presetCombo->setToolTip(tr("Apply a view preset before exporting; its name is stored in the image metadata."));
    form->addRow(tr("View preset:"), presetCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const int bg = bgCombo->currentData().toInt();
    // Apply an optional view preset before rendering so the figure matches it.
    QString presetName;
    if (settings) {
        const QString sel = presetCombo->currentData().toString();
        if (!sel.isEmpty()) {
            for (const ViewPreset& p : settings->viewPresets()) {
                if (p.name == sel) {
                    applyViewPreset(p, true, true);
                    presetName = sel;
                    break;
                }
            }
        }
    }

    // Build the metadata to embed.
    ImageMetadata meta;
    if (embedCheck->isChecked())
        meta = buildImageMetadata(settings, presetName);
    else
        meta.embed = false;

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

    meta.width = wSpin->value();
    meta.height = hSpin->value();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = exportImage(path, wSpin->value(), hSpin->value(), bg, ssaaCheck->isChecked(), meta);
    QApplication::restoreOverrideCursor();
    if (ok) {
        emit imageExported(path);  // let the image-gallery dock pick it up
        QMessageBox::information(this, tr("Export Image"), tr("Image saved to:\n%1").arg(path));
    } else
        QMessageBox::warning(this, tr("Export Image"),
            tr("Failed to export the image (the offscreen render returned no content)."));
}

// Claude Generated 2026 - shared metadata assembly for both export paths so the
// dialog and the quick "Photo" export embed identical provenance.
ImageMetadata MoleculeViewer::buildImageMetadata(Settings* settings, const QString& presetName)
{
    ImageMetadata meta;
    meta.embed = true;
    if (settings) {
        meta.authorName = settings->operatorName();
        meta.authorOrcid = settings->operatorOrcid();
        meta.authorInstitution = settings->operatorInstitution();
        meta.license = settings->operatorLicense();
    }
    if (!m_currentFilePath.isEmpty())
        meta.sourceFiles << m_currentFilePath;

    const ViewPreset cam = currentViewPreset(ZoomMode::Absolute);
    meta.cameraRotation = cam.rootRotation;
    meta.cameraPan = cam.pan;
    meta.cameraDistance = cam.cameraDistance;
    meta.zoomMode = cam.zoomMode;
    meta.zoomFactor = cam.zoomFactor;

    meta.renderingMode = static_cast<int>(m_renderingMode);
    meta.colorScheme = static_cast<int>(m_colorScheme);
    meta.atomScaleFactor = m_atomScaleFactor;
    meta.bondThickness = m_bondThickness;
    meta.atomTransparency = m_atomTransparency;
    meta.backgroundColor = m_backgroundColor;
    meta.effects = QStringLiteral("SSAO=%1,Bloom=%2,HDR=%3,Fog=%4")
                       .arg(m_ssaoEnabled ? 1 : 0).arg(m_bloomEnabled ? 1 : 0)
                       .arg(m_hdrEnabled ? 1 : 0).arg(m_fogEnabled ? 1 : 0);

    meta.qurcumaVersion = QCoreApplication::applicationVersion();
    meta.exportTimestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    meta.viewPresetName = presetName;
    return meta;
}

// Claude Generated 2026 - dialog-free export triggered by the viewer-bar "Photo"
// button. Uses the dialog defaults (2× viewport, transparent, SSAA) and an
// auto-generated file name so a figure is one click away.
QString MoleculeViewer::quickExportImage(const QString& startDir, Settings* settings)
{
    if (!m_scene)
        return QString();

    const QSize cur = m_container ? m_container->size() : QSize(1280, 960);
    const int w = qMax(64, cur.width() * 2);
    const int h = qMax(64, cur.height() * 2);

    // Background from the viewer-bar Photo controls: transparent checkbox wins,
    // otherwise the colour preset (invalid = scene colour). See exportImage().
    int bgMode = 2;  // transparent
    QColor bgColor;
    if (!m_photoTransparent) {
        if (m_photoBgColor.isValid()) {
            bgMode = 3;
            bgColor = m_photoBgColor;
        } else {
            bgMode = 0;  // scene colour
        }
    }

    const QString dir = startDir.isEmpty() ? QDir::homePath() : startDir;
    QString stem = QStringLiteral("qurcuma");
    if (!m_currentFilePath.isEmpty())
        stem = QFileInfo(m_currentFilePath).completeBaseName();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString path = QDir(dir).filePath(QStringLiteral("%1_%2.png").arg(stem, stamp));

    ImageMetadata meta = buildImageMetadata(settings, QString());
    meta.width = w;
    meta.height = h;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = exportImage(path, w, h, bgMode, /*ssaa=*/true, meta, bgColor);
    QApplication::restoreOverrideCursor();
    if (!ok)
        return QString();
    emit imageExported(path);
    return path;
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
    emit animationStateChanged(true);
}

void MoleculeViewer::stopAnimation()
{
    if (m_animationTimer)
        m_animationTimer->stop();
    const bool wasAnimating = m_isAnimating;
    m_isAnimating = false;
    if (wasAnimating)
        emit animationStateChanged(false);
}

// Claude Generated 2026 - Focus gate for the playback keys (Space, arrows).
bool MoleculeViewer::viewportHasFocus() const
{
    return m_container && m_container->hasFocus();
}

// Claude Generated 2026 - Play/pause as one action (toggle button, Space).
void MoleculeViewer::toggleAnimation()
{
    if (m_isAnimating)
        stopAnimation();
    else
        startAnimation();
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

namespace {

// Claude Generated 2026 - Drawn icons for the viewer bar. Theme icons vary
// wildly between desktops (and were often missing entirely); these render
// crisp, consistent glyphs from the palette's text colour, so they fit both
// light and dark themes and every button actually has an icon.
QIcon barIcon(const QString& kind, const QColor& color)
{
    constexpr int size = 20;
    constexpr qreal dpr = 2.0;  // crisp on HiDPI
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    if (kind == QLatin1String("measure")) {
        // Diagonal ruler with tick marks.
        p.drawLine(QPointF(4, 16), QPointF(16, 4));
        p.drawLine(QPointF(7, 13), QPointF(9, 15));
        p.drawLine(QPointF(10, 10), QPointF(12, 12));
        p.drawLine(QPointF(13, 7), QPointF(15, 9));
    } else if (kind == QLatin1String("edit")) {
        // Four-direction move cross.
        p.drawLine(QPointF(10, 4), QPointF(10, 16));
        p.drawLine(QPointF(4, 10), QPointF(16, 10));
        p.drawLine(QPointF(8, 6), QPointF(10, 4));
        p.drawLine(QPointF(12, 6), QPointF(10, 4));
        p.drawLine(QPointF(8, 14), QPointF(10, 16));
        p.drawLine(QPointF(12, 14), QPointF(10, 16));
        p.drawLine(QPointF(6, 8), QPointF(4, 10));
        p.drawLine(QPointF(6, 12), QPointF(4, 10));
        p.drawLine(QPointF(14, 8), QPointF(16, 10));
        p.drawLine(QPointF(14, 12), QPointF(16, 10));
    } else if (kind == QLatin1String("build")) {
        // Hexagon (ring) with a plus for the new atom.
        QPolygonF hex;
        for (int i = 0; i < 6; ++i) {
            const qreal a = qDegreesToRadians(60.0 * i - 90.0);
            hex << QPointF(9 + 5.5 * std::cos(a), 11 + 5.5 * std::sin(a));
        }
        p.drawPolygon(hex);
        p.drawLine(QPointF(16, 3), QPointF(16, 7));
        p.drawLine(QPointF(14, 5), QPointF(18, 5));
    } else if (kind == QLatin1String("nci")) {
        // Two atoms with a dashed contact between them.
        p.setBrush(color);
        p.drawEllipse(QPointF(4.5, 10), 2.2, 2.2);
        p.drawEllipse(QPointF(15.5, 10), 2.2, 2.2);
        p.setBrush(Qt::NoBrush);
        QPen dashed(color, 1.6, Qt::DashLine);
        dashed.setDashPattern({ 2.0, 2.0 });
        p.setPen(dashed);
        p.drawLine(QPointF(7.5, 10), QPointF(12.5, 10));
    } else if (kind == QLatin1String("photo")) {
        // Camera body, viewfinder bump and lens.
        p.drawRoundedRect(QRectF(3, 6.5, 14, 10), 2, 2);
        p.drawLine(QPointF(7.5, 6.5), QPointF(8.5, 4.5));
        p.drawLine(QPointF(8.5, 4.5), QPointF(11.5, 4.5));
        p.drawLine(QPointF(11.5, 4.5), QPointF(12.5, 6.5))
            ;
        p.drawEllipse(QPointF(10, 11.5), 3.2, 3.2);
    } else if (kind == QLatin1String("addh")) {
        // Circled H with a small plus.
        p.drawEllipse(QPointF(9, 11), 6, 6);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(8);
        p.setFont(f);
        p.drawText(QRectF(3, 5, 12, 12), Qt::AlignCenter, QStringLiteral("H"));
        p.drawLine(QPointF(16.5, 3), QPointF(16.5, 7));
        p.drawLine(QPointF(14.5, 5), QPointF(18.5, 5));
    } else if (kind == QLatin1String("clean")) {
        // Sparkle: one big and one small four-point star.
        p.drawLine(QPointF(8, 3.5), QPointF(8, 12.5));
        p.drawLine(QPointF(3.5, 8), QPointF(12.5, 8));
        p.drawLine(QPointF(5.5, 5.5), QPointF(10.5, 10.5));
        p.drawLine(QPointF(10.5, 5.5), QPointF(5.5, 10.5));
        p.drawLine(QPointF(15.5, 12), QPointF(15.5, 17));
        p.drawLine(QPointF(13, 14.5), QPointF(18, 14.5));
    } else if (kind == QLatin1String("gear")) {
        // Gear: ring with radial teeth.
        p.drawEllipse(QPointF(10, 10), 4, 4);
        for (int i = 0; i < 8; ++i) {
            const qreal a = qDegreesToRadians(45.0 * i);
            p.drawLine(QPointF(10 + 5 * std::cos(a), 10 + 5 * std::sin(a)),
                QPointF(10 + 7 * std::cos(a), 10 + 7 * std::sin(a)));
        }
    }
    return QIcon(pm);
}

} // namespace

// Claude Generated 2026 - Attach MainWindow's shared NCI source menu to the bar
// button's dropdown, so bar, Display menu and palette use one action set.
void MoleculeViewer::setNciQuickMenu(QMenu* menu)
{
    if (m_nciButton)
        m_nciButton->setMenu(menu);
}

void MoleculeViewer::setupControlPanel()
{
    // Claude Generated 2026 - One palette-derived colour for all drawn bar icons.
    const QColor iconColor = palette().color(QPalette::ButtonText);
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

    // Claude Generated 2026 - first/prev/next/last; Left/Right and Ctrl+Left/Right
    // shortcuts are handled in MainWindow's app event filter.
    QPushButton* firstButton = new QPushButton("⏮");
    firstButton->setMaximumWidth(30);
    firstButton->setToolTip(tr("First Frame (Ctrl+Left)"));
    connect(firstButton, &QPushButton::clicked, this, &MoleculeViewer::firstFrame);
    frameLayout->addWidget(firstButton);

    QPushButton* prevButton = new QPushButton("◀");
    prevButton->setMaximumWidth(30);
    prevButton->setToolTip(tr("Previous Frame (Left)"));
    connect(prevButton, &QPushButton::clicked, this, &MoleculeViewer::previousFrame);
    frameLayout->addWidget(prevButton);

    QPushButton* nextButton = new QPushButton("▶");
    nextButton->setMaximumWidth(30);
    nextButton->setToolTip(tr("Next Frame (Right)"));
    connect(nextButton, &QPushButton::clicked, this, &MoleculeViewer::nextFrame);
    frameLayout->addWidget(nextButton);

    QPushButton* lastButton = new QPushButton("⏭");
    lastButton->setMaximumWidth(30);
    lastButton->setToolTip(tr("Last Frame (Ctrl+Right)"));
    connect(lastButton, &QPushButton::clicked, this, &MoleculeViewer::lastFrame);
    frameLayout->addWidget(lastButton);

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

    // Claude Generated 2026 - One play/pause toggle whose icon shows the state
    // (was two separate buttons with no running indication). Space toggles too.
    QPushButton* playButton = new QPushButton;
    playButton->setIcon(QIcon::fromTheme("media-playback-start"));
    playButton->setToolTip(tr("Play/Pause Animation (Space)"));
    playButton->setMaximumWidth(30);
    connect(playButton, &QPushButton::clicked, this, &MoleculeViewer::toggleAnimation);
    connect(this, &MoleculeViewer::animationStateChanged, playButton, [playButton](bool running) {
        playButton->setIcon(QIcon::fromTheme(running
            ? QStringLiteral("media-playback-pause")
            : QStringLiteral("media-playback-start")));
    });
    playbackLayout->addWidget(playButton);

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
    measureBtn->setIcon(barIcon(QStringLiteral("measure"), iconColor));
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
    editBtn->setIcon(barIcon(QStringLiteral("edit"), iconColor));
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

    // Build toggle — molecule builder (Claude Generated 2026). Sibling of
    // Measure/Edit; the element strip and dropdown arrive with the element picker.
    QToolButton* buildBtn = new QToolButton;
    buildBtn->setText(tr("Build"));
    buildBtn->setCheckable(true);
    buildBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    buildBtn->setIcon(barIcon(QStringLiteral("build"), iconColor));
    buildBtn->setToolTip(tr("Molecule builder: click empty space to place an atom, click an "
                            "atom to change its element, middle-click an atom to attach one, "
                            "right-click an atom to delete it, drag atom to atom to bond, "
                            "drag an atom onto empty space to move it. "
                            "Keys H C N O S P F L(Cl) R(Br) pick the element. "
                            "Arrow: insert a fragment (docks onto a single selected atom)."));
    connect(buildBtn, &QToolButton::toggled, this, [this](bool on) { setBuildMode(on); });
    // Claude Generated 2026 - Fragment dropdown: with exactly one selected atom a
    // substituent docks onto it, otherwise the fragment lands standalone.
    buildBtn->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu* fragmentMenu = new QMenu(buildBtn);
    const auto& library = build::fragmentLibrary();
    for (int i = 0; i < library.size(); ++i) {
        QAction* a = fragmentMenu->addAction(library[i].name);
        connect(a, &QAction::triggered, this, [this, i]() {
            // Claude Generated 2026 - The fragment hangs on the mouse (carry mode):
            // move it into place, click drops it, Shift+click drops a copy.
            startFragmentCarry(build::fragmentLibrary()[i]);
        });
    }
    buildBtn->setMenu(fragmentMenu);
    connect(this, &MoleculeViewer::interactionModeChanged, buildBtn, [buildBtn](InteractionMode m) {
        const bool on = (m == InteractionMode::Build);
        if (buildBtn->isChecked() != on) {
            buildBtn->blockSignals(true);
            buildBtn->setChecked(on);
            buildBtn->blockSignals(false);
        }
    });
    panelLayout->addWidget(buildBtn);

    // Element strip — visible only while Build mode is on (Claude Generated 2026).
    // Two-way sync with the viewer's build element (hotkeys move the highlight).
    auto* elementStrip = new ElementQuickBar;
    elementStrip->setVisible(false);
    elementStrip->setCurrentElement(m_buildElement);
    connect(elementStrip, &ElementQuickBar::elementPicked,
        this, &MoleculeViewer::setBuildElement);
    connect(this, &MoleculeViewer::buildElementChanged,
        elementStrip, &ElementQuickBar::setCurrentElement);
    connect(this, &MoleculeViewer::interactionModeChanged, elementStrip,
        [elementStrip](InteractionMode m) {
            elementStrip->setVisible(m == InteractionMode::Build);
        });
    panelLayout->addWidget(elementStrip);

    // Add-H button + open-valence label, Build mode only (Claude Generated 2026).
    QToolButton* addHBtn = new QToolButton;
    addHBtn->setText(tr("Add H"));
    addHBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    addHBtn->setIcon(barIcon(QStringLiteral("addh"), iconColor));
    addHBtn->setToolTip(tr("Saturate all open valences with hydrogens "
                           "(tetrahedral/trigonal/linear placement)."));
    addHBtn->setVisible(false);
    connect(addHBtn, &QToolButton::clicked, this, [this]() { addHydrogens(); });
    panelLayout->addWidget(addHBtn);

    QToolButton* cleanupBtn = new QToolButton;
    cleanupBtn->setText(tr("Clean up"));
    cleanupBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    cleanupBtn->setIcon(barIcon(QStringLiteral("clean"), iconColor));
    cleanupBtn->setToolTip(tr("Relax the built structure with a short geometry "
                              "optimization (current method, ~50 steps)."));
    cleanupBtn->setVisible(false);
    connect(cleanupBtn, &QToolButton::clicked, this, [this]() { emit cleanupRequested(); });
    panelLayout->addWidget(cleanupBtn);

    QLabel* valenceLabel = new QLabel;
    valenceLabel->setVisible(false);
    panelLayout->addWidget(valenceLabel);
    auto updateValenceLabel = [this, valenceLabel]() {
        if (!buildMode()) {
            valenceLabel->setVisible(false);
            return;
        }
        const int open = openValenceCount();
        valenceLabel->setVisible(true);
        if (open > 0) {
            valenceLabel->setText(tr("%1 open valence%2").arg(open).arg(open == 1 ? "" : "s"));
            valenceLabel->setStyleSheet(QStringLiteral("QLabel { color: #e0a030; border: none; }"));
        } else {
            valenceLabel->setText(tr("✓ saturated"));
            valenceLabel->setStyleSheet(QStringLiteral("QLabel { color: #4caf50; border: none; }"));
        }
    };
    connect(this, &MoleculeViewer::interactionModeChanged, valenceLabel,
        [addHBtn, cleanupBtn, updateValenceLabel](InteractionMode m) {
            addHBtn->setVisible(m == InteractionMode::Build);
            cleanupBtn->setVisible(m == InteractionMode::Build);
            updateValenceLabel();
        });
    connect(this, &MoleculeViewer::moleculeUpdated, valenceLabel,
        [updateValenceLabel](const QVector<MoleculeViewer::Atom>&,
            const QVector<MoleculeViewer::Bond>&) { updateValenceLabel(); });

    // NCI toggle — quick access to the non-covalent interaction overlay (Claude
    // Generated 2026). Click toggles; the dropdown arrow picks the source. The
    // source menu is injected by MainWindow (setNciQuickMenu), which owns the
    // last-source memory and the analysis worker paths.
    m_nciButton = new QToolButton;
    m_nciButton->setText(tr("NCI"));
    m_nciButton->setCheckable(true);
    m_nciButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_nciButton->setIcon(barIcon(QStringLiteral("nci"), iconColor));
    m_nciButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_nciButton->setToolTip(tr("Show non-covalent interactions (hydrogen/halogen bonds, "
                               "pi stacking, contacts). Arrow: choose the source. Shortcut: N"));
    m_nciButton->setChecked(m_nciSource != 0);
    connect(m_nciButton, &QToolButton::clicked, this, [this]() { emit nciToggleRequested(); });
    connect(this, &MoleculeViewer::nciSourceChanged, m_nciButton, [this](int source) {
        const bool on = (source != 0);
        if (m_nciButton->isChecked() != on) {
            m_nciButton->blockSignals(true);
            m_nciButton->setChecked(on);
            m_nciButton->blockSignals(false);
        }
    });
    panelLayout->addWidget(m_nciButton);

    // Photo — one-click image export (no dialog). Sibling of Measure/Edit; the host
    // supplies the working dir + operator settings via quickExportRequested.
    QToolButton* photoBtn = new QToolButton;
    photoBtn->setText(tr("Photo"));
    photoBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    photoBtn->setIcon(barIcon(QStringLiteral("photo"), iconColor));
    photoBtn->setToolTip(tr("Quick export: save a PNG (2× view, metadata embedded) to the "
                            "working folder without a dialog. Arrow: background options. "
                            "Ctrl+Shift+E opens the full dialog."));
    connect(photoBtn, &QToolButton::clicked, this, [this] { emit quickExportRequested(); });
    // Claude Generated 2026 - Background options moved from two permanent bar
    // widgets (checkbox + combo) into the button's dropdown menu.
    photoBtn->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu* photoMenu = new QMenu(photoBtn);
    QAction* photoTranspAct = photoMenu->addAction(tr("Transparent background"));
    photoTranspAct->setCheckable(true);
    photoTranspAct->setChecked(m_photoTransparent);
    photoMenu->addSeparator();
    auto* photoBgGroup = new QActionGroup(photoMenu);
    const QVector<QPair<QString, QColor>> photoBgs = {
        { tr("Scene background"), QColor() },  // invalid = scene colour
        { tr("White"), QColor(Qt::white) },
        { tr("Black"), QColor(Qt::black) },
        { tr("Light grey"), QColor(0xDD, 0xDD, 0xDD) },
        { tr("Dark grey"), QColor(0x28, 0x28, 0x28) },
    };
    for (const auto& bg : photoBgs) {
        QAction* a = photoMenu->addAction(bg.first);
        a->setCheckable(true);
        a->setChecked(!bg.second.isValid());
        a->setEnabled(!m_photoTransparent);
        photoBgGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, color = bg.second]() { m_photoBgColor = color; });
    }
    m_photoBgColor = QColor();
    connect(photoTranspAct, &QAction::toggled, this, [this, photoBgGroup](bool on) {
        m_photoTransparent = on;
        for (QAction* a : photoBgGroup->actions())
            a->setEnabled(!on);
    });
    photoBtn->setMenu(photoMenu);
    panelLayout->addWidget(photoBtn);

    QComboBox* colorCombo = new QComboBox;
    colorCombo->addItem(tr("CPK"), static_cast<int>(ColorScheme::CPK));
    colorCombo->addItem(tr("Monochrome"), static_cast<int>(ColorScheme::Monochrome));
    colorCombo->addItem(tr("By Charge"), static_cast<int>(ColorScheme::ByCharge));
    colorCombo->addItem(tr("By Type"), static_cast<int>(ColorScheme::ByType));
    colorCombo->addItem(tr("Custom"), static_cast<int>(ColorScheme::Custom));
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
            clashLabel->setVisible(editMode());
            resolveBtn->setVisible(editMode() && n > 0);
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
    QPushButton* displayBtn = new QPushButton(tr("Display"));
    displayBtn->setIcon(barIcon(QStringLiteral("gear"), iconColor));
    displayBtn->setToolTip(tr("Open the Display panel (style, effects, lighting, tools)"));
    connect(displayBtn, &QPushButton::clicked, this, &MoleculeViewer::displayOptionsRequested);
    panelLayout->addWidget(displayBtn);
}
