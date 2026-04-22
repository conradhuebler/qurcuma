// viewer.cpp
#include "view.h"
#include "selectionmanager.h"  // Claude Generated - Phase 2A
#include "measurementoverlay.h"  // Claude Generated - Phase 2B
#include "bondeditor.h"  // Claude Generated - Phase 4B
#include "pbrmaterial.h"  // Claude Generated - Phase 4A
#include "performanceoptimizer.h"  // Claude Generated - LOD wire-up
#include "atominstancingsystem.h"  // Claude Generated - Phase 3.1 - GPU instancing
#include "bondinstancingsystem.h"  // Claude Generated - Phase 3.2 - GPU instancing
#include "xyzparser.h"  // Claude Generated - Phase 4B - For auto-save XYZ writing
#include <QElapsedTimer>  // Claude Generated - Simulation frame timing
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QForwardRenderer>  // Claude Generated - dark clear color
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointLight>        // Claude Generated - world-fixed lights
#include <Qt3DRender/QPickEvent>      // Claude Generated - Phase 2A
#include <Qt3DExtras/QOrbitCameraController>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFile>  // Claude Generated - Phase 4B - For auto-save backup
#include <QMessageBox>
#include <QInputDialog>
#include <QImage>
#include <QPixmap>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <cmath>

const float MoleculeViewer::DEFAULT_BOND_DISTANCE = 3.0f; // Å

MoleculeViewer::MoleculeViewer(QWidget *parent)
    : QWidget(parent)
    , m_frameCount(1)
    , m_currentFrame(0)
{
    // Claude Generated - Phase 2A: Initialize SelectionManager
    m_selectionManager = new SelectionManager(this);

    // Claude Generated - Phase 4B: Initialize BondEditor
    m_bondEditor = new BondEditor(this);

    // Claude Generated - LOD: PerformanceOptimizer drives sphere/cylinder tessellation
    m_perfOpt = new PerformanceOptimizer(this);

    // Claude Generated - Phase 4B: Initialize auto-save system with 500ms debouncing
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(500);  // 500ms debounce delay
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MoleculeViewer::onAutoSaveTimer);

    // Connect BondEditor changes to auto-save trigger
    connect(m_bondEditor, &BondEditor::structureChanged, this, &MoleculeViewer::onStructureChanged);

    setupViewer();

    // Claude Generated - Phase 2B: Initialize MeasurementOverlay
    m_measurementOverlay = new MeasurementOverlay(m_rootEntity);
}

MoleculeViewer::~MoleculeViewer()
{
}

void MoleculeViewer::setupViewer()
{
    // 3D Window erstellen
    m_view = new Qt3DExtras::Qt3DWindow();
    m_container = QWidget::createWindowContainer(m_view, this);

    // Claude Generated - User-configurable clear color. Default is a lifted
    // dark slate, not pitch black, so atom silhouettes have contrast without
    // the scene feeling like a black void. Changed via setBackgroundColor().
    if (auto* fg = qobject_cast<Qt3DExtras::QForwardRenderer*>(m_view->defaultFrameGraph())) {
        fg->setClearColor(m_backgroundColor);
    }

    // Layout erstellen
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Claude Generated - Add control panel at top
    setupControlPanel();
    layout->addWidget(m_controlPanel);

    // Add 3D view
    layout->addWidget(m_container);

    // Root Entity erstellen
    m_rootEntity = new Qt3DCore::QEntity();

    // Model entity: rotation pivot between root and molecule.
    // Mouse drag rotates m_modelTransform instead of orbiting the camera.
    m_modelEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_modelEntity->setObjectName(QStringLiteral("__qurcuma_model_root__"));
    m_modelTransform = new Qt3DCore::QTransform(m_modelEntity);
    m_modelEntity->addComponent(m_modelTransform);
    m_modelRotation = QQuaternion();

    // Claude Generated - 4 world-fixed corner lights positioned at the 4
    // corners of the FRONT face of the scene bounding cube (+Z, ±X, ±Y),
    // not the 4 upper cube corners. Earlier +Y-only positions produced a
    // symmetric "always-top-lit" pattern that looked camera-attached under
    // horizontal orbit. Spreading corners across ±X AND ±Y breaks that
    // symmetry, so camera rotation visibly shifts the lit zone.
    //
    // Uses QPointLight at far distance (≈directional) with a QTransform —
    // Qt3D's default Phong shader reads QPointLight position from the
    // entity's world transform. Parented to m_modelEntity so lights rotate
    // with the model — lighting stays fixed relative to the molecule surface.
    m_lightRoot = new Qt3DCore::QEntity(m_modelEntity);
    m_lightRoot->setObjectName(QStringLiteral("__qurcuma_light_root__"));

    static constexpr float kLightDistance = 1000.0f; // far → behaves directional
    static constexpr float kKeyIntensity  = 1.0f;
    const QColor kLightColor(255, 250, 240);
    // Index mapping matches control-panel 2×2 grid (screen corners):
    //   0 ◤ top-left, 1 ◥ top-right, 2 ◣ bottom-left, 3 ◢ bottom-right
    const QVector3D cornerPos[4] = {
        QVector3D(-1.0f,  1.0f, 1.0f), // ◤ TL
        QVector3D( 1.0f,  1.0f, 1.0f), // ◥ TR
        QVector3D(-1.0f, -1.0f, 1.0f), // ◣ BL
        QVector3D( 1.0f, -1.0f, 1.0f), // ◢ BR
    };
    for (int i = 0; i < 4; ++i) {
        auto* lightEntity = new Qt3DCore::QEntity(m_lightRoot);
        m_cornerLights[i] = new Qt3DRender::QPointLight(lightEntity);
        m_cornerLights[i]->setColor(kLightColor);
        m_cornerLights[i]->setIntensity(m_cornerLightEnabled[i] ? kKeyIntensity : 0.0f);
        // Zero distance attenuation — uniform intensity regardless of distance.
        m_cornerLights[i]->setConstantAttenuation(1.0f);
        m_cornerLights[i]->setLinearAttenuation(0.0f);
        m_cornerLights[i]->setQuadraticAttenuation(0.0f);

        auto* lightTransform = new Qt3DCore::QTransform(lightEntity);
        lightTransform->setTranslation(cornerPos[i].normalized() * kLightDistance);

        lightEntity->addComponent(m_cornerLights[i]);
        lightEntity->addComponent(lightTransform);
    }

    // Kamera einrichten
    m_camera = m_view->camera();
    m_camera->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 5000.0f);

    // Initialize with default position - will be updated by setDefaultView()
    m_camera->setPosition(QVector3D(0, 0, 100.0f));
    m_camera->setViewCenter(QVector3D(0, 0, 0));

    // Kamera-Controller erstellen aber NICHT aktivieren
    // Stattdessen verwenden wir custom mouse handling im eventFilter
    m_cameraController = new Qt3DExtras::QOrbitCameraController(m_rootEntity);
    // NICHT aktivieren: m_cameraController->setCamera(m_camera);

    // Claude Generated - Phase 5A: Initialize custom multi-pass frame graph
    // TEMPORARILY DISABLED (Option A): CustomFrameGraph causes rendering failures on some GPUs
    // Using standard Qt3D rendering instead. Will implement proper fallback in Option B.
    // Reason: RGB8_UNorm format and render target creation failing with RHI backend
    /*
    m_frameGraph = new CustomFrameGraph();

    // Use a QTimer to defer initialization until after layout and show() have completed
    QTimer::singleShot(0, this, [this]() {
        if (m_frameGraph && !m_frameGraph->isInitialized()) {
            QSize frameGraphSize = m_container->size();
            if (frameGraphSize.isEmpty()) {
                frameGraphSize = QSize(800, 600);  // Fallback if still not sized
            }
            m_frameGraph->initialize(frameGraphSize, m_camera, m_rootEntity);
            m_view->setActiveFrameGraph(m_frameGraph);
        }
    });
    */

    m_view->setRootEntity(m_rootEntity);

    // Install event filter für custom mouse interactions
    m_view->installEventFilter(this);
}

bool MoleculeViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view) {
        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_leftMousePressed = true;
                    m_lastMousePos = mouseEvent->position().toPoint();
                    return true;
                } else if (mouseEvent->button() == Qt::RightButton) {
                    m_rightMousePressed = true;
                    m_lastMousePos = mouseEvent->position().toPoint();
                    return true;
                } else if (mouseEvent->button() == Qt::MiddleButton) {
                    // Reset View auf Molekülzentrum
                    resetView();
                    return true;
                }
                break;
            }
            case QEvent::MouseButtonRelease: {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_leftMousePressed = false;
                    // Claude Generated 2026 - Phase 5: end grab; tell worker to drop pending force.
                    if (m_grabbedAtom >= 0) {
                        m_grabbedAtom = -1;
                        emit atomGrabReleased();
                        // Reset cursor after grab release
                        if (m_container)
                            m_container->setCursor(Qt::ArrowCursor);
                    }
                    return true;
                } else if (mouseEvent->button() == Qt::RightButton) {
                    m_rightMousePressed = false;
                    return true;
                }
                break;
            }
            case QEvent::MouseMove: {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                QPoint currentPos = mouseEvent->position().toPoint();

                // Claude Generated 2026 - Phase 5: active grab overrides camera
                // rotation. Convert the per-frame pixel delta into a world-space
                // force along the current screen axes and ship it to the worker.
                // Note: Camera controller is disabled during grab to prevent conflicts.
                if (m_grabbedAtom >= 0 && m_leftMousePressed && m_camera) {
                    QPoint delta = currentPos - m_lastMousePos;
                    // Get camera basis vectors for screen-to-world mapping
                    QVector3D view = m_camera->viewVector().normalized();
                    QVector3D up = m_camera->upVector().normalized();
                    // right = up × view (not view × up) - follows right-hand rule
                    QVector3D right = QVector3D::crossProduct(up, view);
                    // Safety: if up and view are parallel, cross product is zero
                    if (right.lengthSquared() < 1e-6f) {
                        // Use a default right vector (X axis) when camera is top-down
                        right = QVector3D(1, 0, 0);
                    } else {
                        right.normalize();
                    }
                    // Screen coords: +X right, +Y down
                    // World coords: need to map screen movement to world movement
                    QVector3D worldForce =
                        (static_cast<float>(delta.x()) * right
                         + static_cast<float>(delta.y()) * up)
                        * static_cast<float>(m_grabStrength);
                    // Transform world-space force to model-local space (atom positions are model-local)
                    QVector3D modelLocalForce = m_modelRotation.inverted().rotatedVector(worldForce);
                    emit atomForceRequested(m_grabbedAtom, modelLocalForce,
                        m_grabAlpha, m_grabMaxShells);
                    m_lastMousePos = currentPos;
                    return true;
                }

                if (m_leftMousePressed) {
                    handleMouseRotation(currentPos);
                    m_lastMousePos = currentPos;
                    return true;
                } else if (m_rightMousePressed) {
                    handleMousePan(currentPos);
                    m_lastMousePos = currentPos;
                    return true;
                }
                break;
            }
            case QEvent::Wheel: {
                QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
                handleMouseZoom(wheelEvent->angleDelta().y());
                return true;
            }
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MoleculeViewer::resizeEvent(QResizeEvent *event)
{
    // Claude Generated - Handle widget resize and update frame graph viewport
    QWidget::resizeEvent(event);

    // Update frame graph viewport size when widget is resized
    // DISABLED (Option A): CustomFrameGraph temporarily disabled
    /*
    if (m_frameGraph && m_container) {
        QSize newSize = m_container->size();
        if (!newSize.isEmpty()) {
            m_frameGraph->updateViewportSize(newSize);
        }
    }
    */
}

void MoleculeViewer::handleMouseRotation(const QPoint& currentPos)
{
    // Model-based rotation: rotate the molecule, not the camera.
    if (!m_camera) return;

    QPoint delta = currentPos - m_lastMousePos;
    const float rotationSensitivity = 0.5f;

    // Use camera basis vectors as rotation reference (screen-space intuition)
    QVector3D viewDir = m_camera->viewVector().normalized();
    QVector3D rightVector = QVector3D::crossProduct(viewDir, m_camera->upVector()).normalized();
    QVector3D upVector = QVector3D::crossProduct(rightVector, viewDir).normalized();

    // Incremental rotation from screen-space deltas
    QQuaternion horizontalRotation = QQuaternion::fromAxisAndAngle(upVector, delta.x() * rotationSensitivity);
    QQuaternion verticalRotation = QQuaternion::fromAxisAndAngle(rightVector, -delta.y() * rotationSensitivity);

    // Pre-multiply: apply incremental in world space
    QQuaternion deltaRotation = horizontalRotation * verticalRotation;
    m_modelRotation = deltaRotation * m_modelRotation;
    m_modelRotation.normalize();

    updateModelTransformFromRotation();
}

void MoleculeViewer::updateModelTransformFromRotation()
{
    // Rotate around molecule center (pivot), not the origin:
    // translate(center) * rotate(quat) * translate(-center)
    QMatrix4x4 mat;
    mat.translate(m_moleculeCenter);
    mat.rotate(m_modelRotation);
    mat.translate(-m_moleculeCenter);
    m_modelTransform->setMatrix(mat);
}

void MoleculeViewer::handleMousePan(const QPoint& currentPos)
{
    // Claude Generated - Pan the view
    if (!m_camera) return;

    QPoint delta = currentPos - m_lastMousePos;

    // Get current camera state
    QVector3D viewCenter = m_camera->viewCenter();
    QVector3D camPos = m_camera->position();
    QVector3D upVector = m_camera->upVector();

    // Calculate pan vector based on camera orientation
    QVector3D viewDir = (viewCenter - camPos).normalized();
    QVector3D rightVector = QVector3D::crossProduct(viewDir, upVector).normalized();
    QVector3D actualUpVector = QVector3D::crossProduct(rightVector, viewDir).normalized();

    // Pan sensitivity - proportional to distance
    float distance = (viewCenter - camPos).length();
    float panSensitivity = distance * 0.005f; // Adjust this value for pan speed

    // Calculate pan offset
    QVector3D panOffset = rightVector * (-delta.x() * panSensitivity) +
                          actualUpVector * (delta.y() * panSensitivity);

    // Move both view center and camera position
    m_camera->setViewCenter(viewCenter + panOffset);
    m_camera->setPosition(camPos + panOffset);

    updateInstancingCameraPosition();
}

void MoleculeViewer::handleMouseZoom(int delta)
{
    // Claude Generated - Zoom by moving camera along view direction
    if (!m_camera) return;

    // Zoom sensitivity
    const float zoomSensitivity = 1.1f; // Multiplication factor per wheel click

    QVector3D viewCenter = m_camera->viewCenter();
    QVector3D camPos = m_camera->position();

    // Calculate zoom factor
    float zoomFactor = (delta > 0) ? (1.0f / zoomSensitivity) : zoomSensitivity;

    // Calculate distance from view center to camera
    QVector3D radiusVector = camPos - viewCenter;
    float distance = radiusVector.length();

    // Minimum distance to prevent zooming into the structure
    const float minDistance = 0.5f;

    // New distance
    float newDistance = distance * zoomFactor;

    // Clamp to minimum distance
    if (newDistance < minDistance) {
        newDistance = minDistance;
    }

    // Calculate new camera position
    if (distance > 0.01f) {
        QVector3D direction = radiusVector.normalized();
        QVector3D newCamPos = viewCenter + direction * newDistance;
        m_camera->setPosition(newCamPos);
    }

    updateInstancingCameraPosition();
}

// Claude Generated - Push current camera world position into instancing shaders.
// Qt3D automatic uniform injection for custom materials is unreliable; explicit
// QParameter update ensures specular highlight follows camera orbit.
void MoleculeViewer::updateInstancingCameraPosition()
{
    if (!m_camera)
        return;
    const QVector3D camPos = m_camera->position();
    if (m_atomInstancing)
        m_atomInstancing->setCameraPosition(camPos);
    if (m_bondInstancing)
        m_bondInstancing->setCameraPosition(camPos);
}

// Transform model-local position to world space (applies model rotation around pivot).
QVector3D MoleculeViewer::modelToWorld(const QVector3D& localPos) const
{
    return m_modelRotation.rotatedVector(localPos - m_moleculeCenter) + m_moleculeCenter;
}

// Claude Generated - Bulk-toggle all per-atom/per-bond QObjectPickers.
// Qt3D's object-picker service traverses the scene for each mouse event; at ~5000 atoms
// this shows up in profiles. Simulation rarely needs picking, so disable while running.
// Claude Generated - Enable/disable simulation mode and cleanup grab state
void MoleculeViewer::setSimulationActive(bool on)
{
    m_simulationActive = on;
    if (!on) {
        m_grabbedAtom = -1;
        // Reset cursor when sim stops
        if (m_container)
            m_container->setCursor(Qt::ArrowCursor);
    }
}

void MoleculeViewer::setPickingActive(bool active)
{
    for (auto it = m_atomPickerToIndex.constBegin(); it != m_atomPickerToIndex.constEnd(); ++it) {
        if (it.key())
            it.key()->setEnabled(active);
    }
    for (auto it = m_bondPickerToIndex.constBegin(); it != m_bondPickerToIndex.constEnd(); ++it) {
        if (it.key())
            it.key()->setEnabled(active);
    }
}

// Claude Generated - Fix: Create pickers for grab if they don't exist (e.g., large molecules with instancing)
// NOTE: This only works for non-instanced molecules. For instanced molecules (>= 500 atoms),
// grab is not available because individual pickers cannot be created efficiently.
void MoleculeViewer::ensurePickersForGrab()
{
    // If we already have pickers, nothing to do
    if (!m_atomPickerToIndex.isEmpty())
        return;

    // Cannot create pickers for instanced molecules - they use GPU instancing without individual entities
    if (m_atomEntities.isEmpty() || m_atomInstancing) {
        qDebug() << "ensurePickersForGrab: Cannot create pickers for instanced molecule or no entities";
        return;
    }

    // Create pickers for all atom entities
    for (int i = 0; i < m_atomEntities.size(); ++i) {
        Qt3DCore::QEntity *atomEntity = m_atomEntities[i];
        if (!atomEntity)
            continue;

        // Skip if entity has no mesh (can't be picked without geometry)
        bool hasMesh = false;
        auto comps = atomEntity->components();
        for (auto *comp : comps) {
            if (qobject_cast<Qt3DRender::QGeometryRenderer*>(comp) ||
                qobject_cast<Qt3DExtras::QSphereMesh*>(comp)) {
                hasMesh = true;
                break;
            }
        }
        if (!hasMesh)
            continue;

        // Check if entity already has a picker
        bool hasPicker = false;
        auto components = atomEntity->components();
        for (auto *comp : components) {
            if (qobject_cast<Qt3DRender::QObjectPicker*>(comp)) {
                hasPicker = true;
                break;
            }
        }

        if (!hasPicker) {
            Qt3DRender::QObjectPicker *picker = new Qt3DRender::QObjectPicker(atomEntity);
            picker->setHoverEnabled(true);
            atomEntity->addComponent(picker);
            m_atomPickerToIndex[picker] = i;
            connect(picker, &Qt3DRender::QObjectPicker::clicked, this, &MoleculeViewer::onAtomPicked);
            connect(picker, &Qt3DRender::QObjectPicker::pressed, this, &MoleculeViewer::onAtomPressedForGrab);
            connect(picker, &Qt3DRender::QObjectPicker::entered, this, &MoleculeViewer::onAtomHoverEntered);
            connect(picker, &Qt3DRender::QObjectPicker::exited, this, &MoleculeViewer::onAtomHoverExited);
        }
    }

    qDebug() << "ensurePickersForGrab: Created" << m_atomPickerToIndex.size() << "pickers";
}

void MoleculeViewer::clearScene()
{
    // Claude Generated - Phase 2A: Clear picker mappings
    m_atomPickerToIndex.clear();
    m_bondPickerToIndex.clear();

    // Claude Generated - Clear cached transform pointers before deleting entities.
    // Qt3D deletes QTransform components when their parent entity is deleted.
    // Keeping stale pointers in m_atomTransforms/m_bondTransforms causes dangling
    // pointer crashes when updateFramePositions() is called after clearScene().
    m_atomTransforms.clear();
    m_bondTransforms.clear();

    // Claude Generated - Phase 3.1/3.2: Drop instancing systems; their QEntities are
    // parented to a molecule entity that is about to be deleted below.
    if (m_atomInstancing) {
        delete m_atomInstancing;
        m_atomInstancing = nullptr;
    }
    if (m_bondInstancing) {
        delete m_bondInstancing;
        m_bondInstancing = nullptr;
    }

    // Alle Entities löschen
    // Delete children of m_modelEntity (molecule geometry),
    // but skip m_modelTransform (component) and m_lightRoot (persistent lights).
    for (Qt3DCore::QNode *node : m_modelEntity->childNodes()) {
        if (node == m_modelTransform) continue;
        if (node == m_lightRoot) continue;
        delete node;
    }

    // Reset model rotation
    m_modelRotation = QQuaternion();
    m_modelTransform->setRotation(QQuaternion());
    m_modelTransform->setTranslation(QVector3D(0, 0, 0));
    m_modelTransform->setScale(1.0f);
}


    void MoleculeViewer::resetCamera()
{
    // Reset auf gespeichertes Molekülzentrum
    resetViewToMolecule();
}

// Claude Generated - Visual settings setters
void MoleculeViewer::setRenderingMode(RenderingMode mode)
{
    if (m_renderingMode != mode) {
        m_renderingMode = mode;
        // Refresh display if we have trajectory data (without camera reset)
        if (!m_trajectoryAtoms.isEmpty()) {
            refreshVisualization();  // Claude Generated - use refresh instead of showFrame
        }
    }
}

// Claude Generated - Phase 4A: Material mode switching (Phong vs PBR)
void MoleculeViewer::setMaterialMode(MaterialMode mode)
{
    if (m_materialMode != mode) {
        m_materialMode = mode;
        const char* modeNames[] = {"Phong", "PBR (Cook-Torrance)"};
        qDebug() << "Material mode changed to:" << modeNames[static_cast<int>(mode)];

        // Rebuild entire scene with new material mode
        if (!m_trajectoryAtoms.isEmpty()) {
            refreshVisualization();
        }
    }
}

// Claude Generated - Phase 4 Final: Glow intensity for selection highlighting
void MoleculeViewer::setGlowIntensity(float intensity)
{
    float newIntensity = qBound(1.0f, intensity, 2.0f);  // Clamp to 1.0-2.0
    if (!qFuzzyCompare(m_glowIntensity, newIntensity)) {
        m_glowIntensity = newIntensity;
        qDebug() << "Glow intensity set to:" << m_glowIntensity;

        // Update materials to reflect new glow intensity
        if (!m_trajectoryAtoms.isEmpty()) {
            updateMaterials();
        }
    }
}

void MoleculeViewer::setColorScheme(ColorScheme scheme)
{
    if (m_colorScheme != scheme) {
        m_colorScheme = scheme;
        // Claude Generated - Fix 2: Only update materials, not entire scene
        if (!m_trajectoryAtoms.isEmpty()) {
            updateMaterials();
        }
    }
}

// Claude Generated - Background color applied directly to the default
// forward renderer clear color. Safe to call before or after the scene is built.
void MoleculeViewer::setBackgroundColor(const QColor& color)
{
    if (!color.isValid() || m_backgroundColor == color) return;
    m_backgroundColor = color;
    if (m_view) {
        if (auto* fg = qobject_cast<Qt3DExtras::QForwardRenderer*>(m_view->defaultFrameGraph())) {
            fg->setClearColor(m_backgroundColor);
        }
    }
}

// Claude Generated - Toggle one of the 4 corner lights by setting intensity.
// Index 0..3 maps to top-front-left / top-front-right / top-back-left / top-back-right.
void MoleculeViewer::setCornerLightEnabled(int index, bool on)
{
    if (index < 0 || index > 3) return;
    m_cornerLightEnabled[index] = on;
    if (m_cornerLights[index]) {
        m_cornerLights[index]->setIntensity(on ? 0.9f : 0.0f);
    }
}

bool MoleculeViewer::isCornerLightEnabled(int index) const
{
    if (index < 0 || index > 3) return false;
    return m_cornerLightEnabled[index];
}

void MoleculeViewer::setAtomTransparency(float alpha)
{
    float newAlpha = qBound(0.0f, alpha, 1.0f);
    if (m_atomTransparency != newAlpha) {
        m_atomTransparency = newAlpha;
        // Claude Generated - Fix 2: Only update materials, not entire scene
        if (!m_trajectoryAtoms.isEmpty()) {
            updateMaterials();
        }
    }
}

void MoleculeViewer::setAtomShininess(float shininess)
{
    float newShininess = qMax(0.0f, shininess);
    if (m_atomShininess != newShininess) {
        m_atomShininess = newShininess;
        // Claude Generated - Fix 2: Only update materials, not entire scene
        if (!m_trajectoryAtoms.isEmpty()) {
            updateMaterials();
        }
    }
}

void MoleculeViewer::setAtomScaleFactor(float scale)
{
    float newScale = qMax(0.1f, scale);  // Min 0.1x scaling
    if (m_atomScaleFactor != newScale) {
        m_atomScaleFactor = newScale;
        // Claude Generated - Fix 2: Only update atom geometry, not entire scene
        if (!m_trajectoryAtoms.isEmpty()) {
            updateAtomGeometry();
        }
    }
}

void MoleculeViewer::setBondThickness(float thickness)
{
    float newThickness = qMax(0.01f, thickness);  // Min 0.01 radius
    if (m_bondThickness != newThickness) {
        m_bondThickness = newThickness;
        // Claude Generated - Fix 2: Only update bond geometry, not entire scene
        if (!m_trajectoryAtoms.isEmpty()) {
            updateBondGeometry();
        }
    }
}

// Claude Generated - Fix 2 & Phase 4 Final: Incremental update methods for smooth rendering
void MoleculeViewer::updateMaterials()
{
    // Update only the material properties of existing atoms (colors, transparency, shininess)
    // This is much faster than full scene rebuild
    for (int i = 0; i < m_atomMaterials.size(); ++i) {
        Qt3DRender::QMaterial *baseMaterial = m_atomMaterials[i];
        if (!baseMaterial) continue;

        // Get atom for color calculation
        if (m_currentFrame >= m_trajectoryAtoms.size() || i >= m_trajectoryAtoms[m_currentFrame].size()) {
            continue;
        }
        const Atom& atom = m_trajectoryAtoms[m_currentFrame][i];

        // Update color based on current color scheme
        QColor atomColor = getAtomColor(atom.element, atom.charge);

        // Apply transparency
        if (m_atomTransparency < 1.0f) {
            atomColor.setAlphaF(m_atomTransparency);
        }

        // Claude Generated - Phase 2A: Apply selection highlighting with bright colors
        bool isSelected = m_selectionManager && m_selectionManager->isSelected(i);
        QColor updateColor = isSelected ? QColor(255, 200, 50) : atomColor;  // Bright orange-yellow for selection

        // Update material based on type
        if (m_materialMode == MaterialMode::PBR) {
            // Update PBR Material
            PBRMaterial *pbrMat = qobject_cast<PBRMaterial*>(baseMaterial);
            if (pbrMat) {
                pbrMat->setBaseColor(updateColor);
                if (isSelected) {
                    // Claude Generated - Phase 4 Final: Apply glow intensity to PBR material
                    float metallic = 0.2f + (0.3f * (m_glowIntensity - 1.0f));  // 0.2-0.5 range
                    float roughness = 0.3f / m_glowIntensity;  // Smoother with higher glow
                    pbrMat->setMetallic(qBound(0.0f, metallic, 1.0f));
                    pbrMat->setRoughness(qBound(0.0f, roughness, 1.0f));
                }
            }
        } else {
            // Update Phong Material
            Qt3DExtras::QPhongMaterial *phongMat = qobject_cast<Qt3DExtras::QPhongMaterial*>(baseMaterial);
            if (phongMat) {
                phongMat->setAmbient(updateColor.darker());
                phongMat->setDiffuse(updateColor);
                if (isSelected) {
                    // Claude Generated - Phase 4 Final: Apply glow intensity to Phong material
                    float shininess = m_atomShininess + (20.0f * m_glowIntensity);  // Base + glow
                    phongMat->setShininess(qBound(0.0f, shininess, 128.0f));  // Clamp to valid range
                } else {
                    phongMat->setShininess(m_atomShininess);
                }
            }
        }

    }
}

void MoleculeViewer::updateAtomGeometry()
{
    // Update only atom scales (transforms) - used when changing atom size
    for (int i = 0; i < m_atomEntities.size(); ++i) {
        Qt3DCore::QEntity *atomEntity = m_atomEntities[i];
        if (!atomEntity) continue;

        // Find transform component
        auto transforms = atomEntity->componentsOfType<Qt3DCore::QTransform>();
        for (auto transform : transforms) {
            // Get current position from atom
            if (m_currentFrame >= m_trajectoryAtoms.size() || i >= m_trajectoryAtoms[m_currentFrame].size()) {
                continue;
            }
            const Atom& atom = m_trajectoryAtoms[m_currentFrame][i];

            // Update mesh scale
            auto meshes = atomEntity->componentsOfType<Qt3DExtras::QSphereMesh>();
            for (auto mesh : meshes) {
                float radius = getAtomRadius(atom.element);
                mesh->setRadius(radius);
            }

            // Keep transform position (don't change translation)
            transform->setTranslation(atom.position);
        }
    }
}

void MoleculeViewer::updateBondGeometry()
{
    // Update only bond thickness (transforms) - used when changing bond thickness
    for (int i = 0; i < m_bondEntities.size(); ++i) {
        Qt3DCore::QEntity *bondEntity = m_bondEntities[i];
        if (!bondEntity) continue;

        // Find transform component
        auto transforms = bondEntity->componentsOfType<Qt3DCore::QTransform>();
        for (auto transform : transforms) {
            // Update mesh radius
            auto meshes = bondEntity->componentsOfType<Qt3DExtras::QCylinderMesh>();
            for (auto mesh : meshes) {
                float bondRadius = m_bondThickness;
                if (m_renderingMode == RenderingMode::Wireframe) {
                    bondRadius *= 0.5f;
                } else if (m_renderingMode == RenderingMode::SticksOnly) {
                    bondRadius *= 1.5f;
                }
                mesh->setRadius(bondRadius);
            }
        }
    }
}

// Claude Generated - getAtomColor now supports multiple color schemes
QColor MoleculeViewer::getAtomColor(const QString& element, float charge)
{
    switch (m_colorScheme) {
        case ColorScheme::CPK: {
            // CPK-Farbschema (häufig verwendete Farben in der Moleküldarstellung)
            static const QMap<QString, QColor> colors = {
                {"H", QColor(255, 255, 255)},   // Weiß
                {"C", QColor(128, 128, 128)},   // Grau
                {"N", QColor(0, 0, 255)},       // Blau
                {"O", QColor(255, 0, 0)},       // Rot
                {"P", QColor(255, 165, 0)},     // Orange
                {"S", QColor(255, 255, 0)},     // Gelb
                {"Cl", QColor(0, 255, 0)},      // Grün
                {"Br", QColor(165, 42, 42)},    // Braun
                {"I", QColor(148, 0, 211)},     // Violett
                {"F", QColor(218, 165, 32)},    // Goldgelb
                {"Na", QColor(0, 0, 170)},      // Dunkelblau
                {"K", QColor(143, 124, 195)},   // Violett
                {"Mg", QColor(0, 255, 0)},      // Grün
                {"Ca", QColor(128, 128, 144)},  // Grau
                {"Fe", QColor(255, 165, 0)},    // Orange
                {"Zn", QColor(165, 165, 165)}   // Grau
            };
            return colors.value(element, QColor(200, 200, 200));
        }

        case ColorScheme::Monochrome:
            return QColor(180, 180, 180);  // Uniform gray

        case ColorScheme::ByCharge: {
            // Red for positive, blue for negative, white for neutral
            if (charge > 0.5f) {
                return QColor(255, 0, 0);  // Red (positive)
            } else if (charge < -0.5f) {
                return QColor(0, 0, 255);  // Blue (negative)
            } else if (charge > 0.1f) {
                return QColor(255, 128, 128);  // Light red
            } else if (charge < -0.1f) {
                return QColor(128, 128, 255);  // Light blue
            } else {
                return QColor(220, 220, 220);  // Nearly white (neutral)
            }
        }

        case ColorScheme::Custom:
            // For now, return CPK colors (can be extended later with custom mapping)
            return QColor(200, 200, 200);

        default:
            return QColor(200, 200, 200);
    }
}

// Claude Generated - getAtomRadius now applies scale factor
float MoleculeViewer::getAtomRadius(const QString& element)
{
    // Van der Waals Radien in Ångström (base values)
    static const QMap<QString, float> radii = {
        {"H", 0.5f},
        {"C", 0.7f},
        {"N", 0.65f},
        {"O", 0.6f},
        {"P", 1.0f},
        {"S", 1.0f},
        {"Cl", 1.0f},
        {"Br", 1.15f},
        {"I", 1.4f},
        {"F", 0.5f},
        {"Na", 1.8f},
        {"K", 2.2f},
        {"Mg", 1.7f},
        {"Ca", 2.0f},
        {"Fe", 1.4f},
        {"Zn", 1.35f}
    };

    float baseRadius = radii.value(element, 0.7f);

    // Apply rendering mode adjustments
    switch (m_renderingMode) {
        case RenderingMode::SpaceFilling:
            // Use full Van der Waals radius for space-filling
            return baseRadius * 2.0f * m_atomScaleFactor;
        case RenderingMode::BallAndStick:
        case RenderingMode::Wireframe:
        case RenderingMode::SticksOnly:
        default:
            // Use standard scaled radius
            return baseRadius * m_atomScaleFactor;
    }
}

float MoleculeViewer::getCovalentRadius(const QString& element)
{
    // Claude Generated - Covalent radii in Ångström for bond detection
    // Source: CRC Handbook of Chemistry and Physics
    static const QMap<QString, float> covalentRadii = {
        {"H", 0.31f},
        {"C", 0.76f},
        {"N", 0.71f},
        {"O", 0.66f},
        {"F", 0.64f},
        {"P", 1.07f},
        {"S", 1.05f},
        {"Cl", 1.02f},
        {"Br", 1.20f},
        {"I", 1.39f},
        {"Na", 1.54f},
        {"K", 1.96f},
        {"Mg", 1.30f},
        {"Ca", 1.76f},
        {"Fe", 1.32f},
        {"Zn", 1.22f}
    };

    return covalentRadii.value(element, 0.76f);  // Default to carbon if unknown
}

Qt3DCore::QEntity* MoleculeViewer::createMoleculeEntity(const QVector<Atom>& atoms, const QVector<Bond>& bonds)
{
    Qt3DCore::QEntity *moleculeEntity = new Qt3DCore::QEntity();

    // Claude Generated - Fix 2: Clear entity/material references for incremental updates
    m_atomEntities.clear();
    m_bondEntities.clear();
    m_atomMaterials.clear();
    m_atomTransforms.clear();  // Claude Generated - cached transform pointers
    m_bondTransforms.clear();

    // Claude Generated - Render atoms based on rendering mode
    bool renderAtoms = (m_renderingMode == RenderingMode::BallAndStick ||
                        m_renderingMode == RenderingMode::SpaceFilling);

    // Claude Generated - LOD: pull tessellation from PerformanceOptimizer (Fast=8, Balanced=16, HighQuality=32)
    const int sphereRings = m_perfOpt ? m_perfOpt->getSphereRings() : 16;
    const int sphereSlices = m_perfOpt ? m_perfOpt->getSphereSlices() : 16;
    const int cylinderSlices = m_perfOpt ? m_perfOpt->getBondSlices() : 16;

    // Claude Generated - Phase 3.1: GPU instancing path for large atom counts.
    // Auto-enabled when atom count exceeds threshold for performance.
    const bool useInstancing = renderAtoms && atoms.size() >= kAtomInstancingThreshold;

    if (useInstancing) {
        if (m_atomInstancing) {
            delete m_atomInstancing;
            m_atomInstancing = nullptr;
        }
        m_atomInstancing = new AtomInstancingSystem(moleculeEntity, this);

        QVector<QVector3D> positions;
        QVector<QString> elements;
        QVector<QColor> colors;
        QVector<float> scales;
        positions.reserve(atoms.size());
        elements.reserve(atoms.size());
        colors.reserve(atoms.size());
        scales.reserve(atoms.size());
        for (const Atom& atom : atoms) {
            positions.append(atom.position);
            elements.append(atom.element);
            QColor c = getAtomColor(atom.element, atom.charge);
            if (m_atomTransparency < 1.0f)
                c.setAlphaF(m_atomTransparency);
            colors.append(c);
            scales.append(getAtomRadius(atom.element));
        }
        m_atomInstancing->setAtoms(positions, elements, colors, scales);
    }

    // Claude Generated - Perf: skip per-atom/per-bond QObjectPicker for large scenes.
    // Even disabled pickers add scene-graph traversal cost per mouse event. Picking for
    // large molecules should be handled by a single ray-cast, not N pickers.
    const bool usePickers = atoms.size() < kAtomInstancingThreshold;

    if (renderAtoms && !useInstancing) {
        for (int atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
            const Atom& atom = atoms[atomIndex];
            Qt3DCore::QEntity *atomEntity = new Qt3DCore::QEntity(moleculeEntity);

            // Mesh
            Qt3DExtras::QSphereMesh *sphereMesh = new Qt3DExtras::QSphereMesh();
            sphereMesh->setRadius(getAtomRadius(atom.element));
            sphereMesh->setRings(sphereRings);
            sphereMesh->setSlices(sphereSlices);

            // Claude Generated - Phase 4 Final: Conditional material (PBR vs Phong)
            QColor atomColor = getAtomColor(atom.element, atom.charge);

            // Apply transparency
            if (m_atomTransparency < 1.0f) {
                atomColor.setAlphaF(m_atomTransparency);
            }

            Qt3DRender::QMaterial *material = nullptr;

            if (m_materialMode == MaterialMode::PBR) {
                // PBR Material (Cook-Torrance BRDF)
                PBRMaterial *pbrMat = new PBRMaterial(atomEntity);
                pbrMat->setBaseColor(atomColor);
                pbrMat->setMetallic(0.1f);      // Non-metallic atoms
                pbrMat->setRoughness(0.5f);     // Medium roughness
                pbrMat->setAmbientOcclusion(1.0f);
                material = pbrMat;
            } else {
                // Phong Material (traditional lighting)
                Qt3DExtras::QPhongMaterial *phongMat = new Qt3DExtras::QPhongMaterial(atomEntity);
                phongMat->setAmbient(atomColor.darker());
                phongMat->setDiffuse(atomColor);
                phongMat->setShininess(m_atomShininess);
                material = phongMat;
            }

            // Transform
            Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
            transform->setTranslation(atom.position);

            atomEntity->addComponent(sphereMesh);
            atomEntity->addComponent(transform);
            atomEntity->addComponent(material);

            // Claude Generated - Phase 2A: Add ObjectPicker for 3D atom selection.
            // Skipped for large scenes (>= kAtomInstancingThreshold) to keep scene graph lean.
            if (usePickers) {
                Qt3DRender::QObjectPicker *picker = new Qt3DRender::QObjectPicker(atomEntity);
                picker->setHoverEnabled(true);
                atomEntity->addComponent(picker);
                m_atomPickerToIndex[picker] = atomIndex;
                connect(picker, &Qt3DRender::QObjectPicker::clicked, this, &MoleculeViewer::onAtomPicked);
                // Claude Generated 2026 - Phase 5: press initiates sim-mode grab (no-op otherwise)
                connect(picker, &Qt3DRender::QObjectPicker::pressed, this, &MoleculeViewer::onAtomPressedForGrab);
                // Claude Generated - Visual feedback for interactive grab
                connect(picker, &Qt3DRender::QObjectPicker::entered, this, &MoleculeViewer::onAtomHoverEntered);
                connect(picker, &Qt3DRender::QObjectPicker::exited, this, &MoleculeViewer::onAtomHoverExited);
            }

            // Claude Generated - Fix 2: Store references for incremental updates
            m_atomEntities.append(atomEntity);
            m_atomMaterials.append(material);
            m_atomTransforms.append(transform);  // cache for O(1) access in updateFramePositions
        }
    }

    // Claude Generated - Render bonds based on rendering mode
    bool renderBonds = (m_renderingMode != RenderingMode::SpaceFilling);

    // Claude Generated - Phase 3.2: Bond instancing disabled by default pending diagnosis
    // of rendering failure when combined with atom instancing. Set env var
    // QURCUMA_BOND_INSTANCING=1 to enable. Atom instancing (Phase 3.1) remains active.
    const bool useBondInstancing = useInstancing && renderBonds
        && qEnvironmentVariableIsSet("QURCUMA_BOND_INSTANCING");

    if (useBondInstancing) {
        float bondRadius = m_bondThickness;
        if (m_renderingMode == RenderingMode::Wireframe)
            bondRadius *= 0.5f;
        else if (m_renderingMode == RenderingMode::SticksOnly)
            bondRadius *= 1.5f;

        if (m_bondInstancing) {
            delete m_bondInstancing;
            m_bondInstancing = nullptr;
        }
        m_bondInstancing = new BondInstancingSystem(moleculeEntity, this);

        // Claude Generated - Per-half bond coloring in instanced path.
        // Each bond emits 2 instances (halves colored by adjacent atoms); multi
        // bonds emit 2 halves per replica. Instance stride per bond = bondOrder * 2.
        QVector<BondInstancingSystem::BondInstance> instances;
        instances.reserve(bonds.size() * 2);

        const QVector3D localUp(0, 1, 0);
        for (const Bond& bond : bonds) {
            if (bond.atom1 < 0 || bond.atom1 >= atoms.size()
                || bond.atom2 < 0 || bond.atom2 >= atoms.size())
                continue;

            const QVector3D posA = atoms[bond.atom1].position;
            const QVector3D posB = atoms[bond.atom2].position;
            const QVector3D direction = posB - posA;
            const float length = direction.length();
            if (length < 1e-4f)
                continue;
            const QVector3D normDir = direction / length;

            QQuaternion rotation;
            const QVector3D rotAxis = QVector3D::crossProduct(localUp, normDir);
            if (rotAxis.length() < 0.001f) {
                rotation = normDir.y() > 0 ? QQuaternion()
                                           : QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f);
            } else {
                const float angle = qAcos(QVector3D::dotProduct(localUp, normDir)) * 180.0f / float(M_PI);
                rotation = QQuaternion::fromAxisAndAngle(rotAxis.normalized(), angle);
            }

            QVector3D offsetDir(0, 0, 0);
            if (bond.bondOrder > 1) {
                offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 0, 1)).normalized();
                if (offsetDir.length() < 0.001f)
                    offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 1, 0)).normalized();
            }
            const float multiBondOffset = 0.2f;

            const QColor colorA = getAtomColor(atoms[bond.atom1].element, atoms[bond.atom1].charge);
            const QColor colorB = getAtomColor(atoms[bond.atom2].element, atoms[bond.atom2].charge);

            for (int r = 0; r < bond.bondOrder; ++r) {
                QVector3D repOffset(0, 0, 0);
                if (r > 0) {
                    const float currentOffset = (r % 2 == 0 ? 1 : -1) * multiBondOffset * ((r + 1) / 2);
                    repOffset = offsetDir * currentOffset;
                }
                const QVector3D a = posA + repOffset;
                const QVector3D b = posB + repOffset;
                const QVector3D mid = 0.5f * (a + b);

                BondInstancingSystem::BondInstance halfA;
                halfA.center = 0.5f * (a + mid);
                halfA.halfLength = length * 0.25f;
                halfA.rotation = rotation;
                halfA.color = colorA;
                halfA.radius = bondRadius;
                instances.append(halfA);

                BondInstancingSystem::BondInstance halfB = halfA;
                halfB.center = 0.5f * (mid + b);
                halfB.color = colorB;
                instances.append(halfB);
            }
        }
        m_bondInstancing->setBonds(instances);
    }

    if (renderBonds && !useBondInstancing) {
        // Determine bond thickness based on mode
        float bondRadius = m_bondThickness;
        if (m_renderingMode == RenderingMode::Wireframe) {
            bondRadius *= 0.5f;  // Thinner for wireframe
        } else if (m_renderingMode == RenderingMode::SticksOnly) {
            bondRadius *= 1.5f;  // Thicker for sticks-only
        }

        // Claude Generated - Per-half bond coloring.
        // Each bond becomes two half-cylinders, each colored from its adjacent
        // atom's CPK color. Multi-bond replicas (double/triple) still offset
        // perpendicular to the bond; each replica produces its own pair of
        // halves. Per-bond stride in m_bondTransforms is (bondOrder * 2).
        const float multiBondOffset = 0.2f;

        for (const Bond& bond : bonds) {
            if (bond.atom1 < 0 || bond.atom1 >= atoms.size() ||
                bond.atom2 < 0 || bond.atom2 >= atoms.size())
                continue;

            const QVector3D posA = atoms[bond.atom1].position;
            const QVector3D posB = atoms[bond.atom2].position;
            const QVector3D direction = posB - posA;
            const float length = direction.length();
            if (length < 1e-4f) continue;
            const QVector3D normDir = direction / length;

            QQuaternion rotation;
            {
                QVector3D localUp(0, 1, 0);
                QVector3D rotAxis = QVector3D::crossProduct(localUp, normDir);
                if (rotAxis.length() < 0.001f) {
                    rotation = normDir.y() > 0
                        ? QQuaternion()
                        : QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f);
                } else {
                    float rotAngle = qAcos(QVector3D::dotProduct(localUp, normDir)) * 180.0f / float(M_PI);
                    rotation = QQuaternion::fromAxisAndAngle(rotAxis.normalized(), rotAngle);
                }
            }

            QVector3D offsetDir(0, 0, 0);
            if (bond.bondOrder > 1) {
                offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 0, 1)).normalized();
                if (offsetDir.length() < 0.001f)
                    offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 1, 0)).normalized();
            }

            const QColor colorA = getAtomColor(atoms[bond.atom1].element, atoms[bond.atom1].charge);
            const QColor colorB = getAtomColor(atoms[bond.atom2].element, atoms[bond.atom2].charge);

            auto makeHalf = [&](const QVector3D& center, const QColor& col, bool attachPicker) {
                Qt3DCore::QEntity* halfEntity = new Qt3DCore::QEntity(moleculeEntity);

                Qt3DExtras::QCylinderMesh* mesh = new Qt3DExtras::QCylinderMesh();
                mesh->setRadius(bondRadius);
                mesh->setRings(cylinderSlices);
                mesh->setSlices(cylinderSlices);

                Qt3DExtras::QPhongMaterial* material = new Qt3DExtras::QPhongMaterial();
                material->setAmbient(col.darker(180));
                material->setDiffuse(col);
                material->setShininess(m_atomShininess);

                Qt3DCore::QTransform* transform = new Qt3DCore::QTransform();
                transform->setTranslation(center);
                transform->setScale3D(QVector3D(1.0f, length * 0.5f, 1.0f));
                transform->setRotation(rotation);

                halfEntity->addComponent(mesh);
                halfEntity->addComponent(transform);
                halfEntity->addComponent(material);

                if (attachPicker && usePickers) {
                    int bondIndex = m_bondEntities.size();
                    Qt3DRender::QObjectPicker* picker = new Qt3DRender::QObjectPicker(halfEntity);
                    picker->setHoverEnabled(true);
                    halfEntity->addComponent(picker);
                    m_bondPickerToIndex[picker] = bondIndex;
                    connect(picker, &Qt3DRender::QObjectPicker::clicked, this, &MoleculeViewer::onBondPicked);
                }

                m_bondEntities.append(halfEntity);
                m_bondTransforms.append(transform);
            };

            for (int r = 0; r < bond.bondOrder; ++r) {
                QVector3D repOffset(0, 0, 0);
                if (r > 0) {
                    const float currentOffset = (r % 2 == 0 ? 1 : -1) * multiBondOffset * ((r + 1) / 2);
                    repOffset = offsetDir * currentOffset;
                }
                const QVector3D a = posA + repOffset;
                const QVector3D b = posB + repOffset;
                const QVector3D mid = 0.5f * (a + b);
                makeHalf(0.5f * (a + mid), colorA, r == 0);
                makeHalf(0.5f * (mid + b), colorB, false);
            }
        }
    }
    return moleculeEntity;
}

void MoleculeViewer::setDefaultView()
{
    if (!m_camera) return;

    // Reset model rotation
    m_modelRotation = QQuaternion();
    m_modelTransform->setRotation(QQuaternion());
    m_modelTransform->setTranslation(QVector3D(0, 0, 0));
    m_modelTransform->setScale(1.0f);

    // Berechne optimale Kamera-Distanz basierend auf Molekülgröße
    float effectiveRadius = m_moleculeRadius;
    if (effectiveRadius < 1.0f) {
        effectiveRadius = 10.0f; // Mindestradius für kleine Moleküle
    }

    // Distanz für gute Übersicht - adaptiv zur Molekülgröße
    float distance = effectiveRadius * 3.0f;

    // Position Kamera auf Z-Achse vom Molekülzentrum aus
    QVector3D cameraPos = m_moleculeCenter + QVector3D(0.0f, 0.0f, distance);

    m_camera->setPosition(cameraPos);
    m_camera->setViewCenter(m_moleculeCenter);
    m_camera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));

    // Kamera-Lens erweitern für große Moleküle
    float maxDistance = distance + effectiveRadius * 2.0f;
    m_camera->lens()->setNearPlane(qMax(0.1f, effectiveRadius * 0.1f));
    m_camera->lens()->setFarPlane(qMax(1000.0f, maxDistance));

    updateInstancingCameraPosition();
}

void MoleculeViewer::resetView()
{
    setDefaultView();
}

void MoleculeViewer::resetViewToMolecule()
{
    // Reset auf gespeichertes Molekülzentrum und optimierte Kamera-Position
    if (m_moleculeCenter != QVector3D(0, 0, 0) || m_moleculeRadius > 0) {
        setDefaultView();
    } else {
        // Fallback für Fall wenn noch kein Molekül geladen wurde
        m_camera->setPosition(QVector3D(0, 0, 100.0f));
        m_camera->setViewCenter(QVector3D(0, 0, 0));
    }
}

QVector<MoleculeViewer::Bond> MoleculeViewer::detectBonds(const QVector<Atom>& atoms)
{
    QVector<Bond> detectedBonds;

    // Claude Generated - Improved bond detection using covalent radii and tolerance
    // Berechne Bindungen basierend auf Atomabständen

    // Bond detection tolerance - aim for max ~2.0-2.1Å for typical bonds
    const float BOND_TOLERANCE = 1.25f;  // Allow 25% extra distance for bond detection

    for (int i = 0; i < atoms.size(); ++i) {
        for (int j = i + 1; j < atoms.size(); ++j) {
            QVector3D diff = atoms[i].position - atoms[j].position;
            float distance = diff.length();

            // Use covalent radii for bond detection
            float covalentRadiusSum = getCovalentRadius(atoms[i].element) + getCovalentRadius(atoms[j].element);
            float bondThreshold = covalentRadiusSum * BOND_TOLERANCE;

            // Prüfe ob Atome nah genug für eine Bindung sind
            if (distance <= bondThreshold) {
                Bond bond;
                bond.atom1 = i;
                bond.atom2 = j;
                bond.bondOrder = 1; // Standard: Einfachbindung
                detectedBonds.append(bond);
            }
        }
    }

    return detectedBonds;
}

void MoleculeViewer::addMolecule(const QVector<Atom>& atoms, const QVector<Bond>& bonds)
{
    clearScene();

    // Berechne Molekülzentrum und Radius für die aktuellen Atome
    if (atoms.isEmpty()) {
        qWarning() << "Empty atom list - cannot calculate molecule center";
        return;
    }
    
    QVector3D min = atoms[0].position;
    QVector3D max = atoms[0].position;
    
    for (const Atom& atom : atoms) {
        min = QVector3D(
            qMin(min.x(), atom.position.x()),
            qMin(min.y(), atom.position.y()),
            qMin(min.z(), atom.position.z())
        );
        max = QVector3D(
            qMax(max.x(), atom.position.x()),
            qMax(max.y(), atom.position.y()),
            qMax(max.z(), atom.position.z())
        );
    }
    
    m_moleculeCenter = (min + max) * 0.5f;
    m_moleculeRadius = (max - min).length() * 0.5f;
    
    // Stelle sicher, dass der Radius nicht zu klein ist
    if (m_moleculeRadius < 1.0f) {
        m_moleculeRadius = 10.0f; // Mindestradius für kleine Moleküle
    }

    // Verwende die übergebenen Bindungen oder erkenne sie automatisch
    QVector<Bond> actualBonds = bonds.isEmpty() ? detectBonds(atoms) : bonds;

    // Claude Generated - Store molecule in trajectory arrays (even single molecules)
    // This allows the settings (transparency, rendering mode, etc.) to work correctly
    m_trajectoryAtoms.clear();
    m_trajectoryBonds.clear();
    m_trajectoryAtoms.append(atoms);
    m_trajectoryBonds.append(actualBonds);
    m_frameCount = 1;
    m_currentFrame = 0;

    // Claude Generated - Phase 4B: Initialize BondEditor with current molecule
    if (m_bondEditor) {
        m_bondEditor->setAtoms(atoms);
        // Note: Bonds will be added via setBond methods during editing
        // or populated from loaded trajectory data
    }

    // Claude Generated - LOD: adapt tessellation BEFORE entity creation.
    // setAtomCount triggers updateAdaptiveQuality which picks Fast/Balanced/HighQuality
    // from recommendQualityMode thresholds (>5000 Fast, >1000 Balanced, else HighQuality).
    if (m_perfOpt)
        m_perfOpt->setAtomCount(atoms.size());

    // Render the molecule using trajectory data
    Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
    moleculeEntity->setParent(m_modelEntity);

    // Claude Generated 2026 - Phase 6: notify listeners (sim dock) of the new molecule.
    emit moleculeUpdated(m_trajectoryAtoms[0], m_trajectoryBonds[0]);

    // Kamera-Controller aktivieren (falls noch nicht aktiv)
    if (!m_cameraController) {
        m_cameraController = new Qt3DExtras::QOrbitCameraController(m_rootEntity);
        m_cameraController->setLinearSpeed(50.0f);
        m_cameraController->setLookSpeed(180.0f);
        m_cameraController->setCamera(m_camera);
    }

    // Setze die Standardansicht auf das Molekülzentrum
    setDefaultView();
}

void MoleculeViewer::showFrame(int frameIndex)
{
    if (frameIndex >= 0 && frameIndex < m_trajectoryAtoms.size()) {
        m_currentFrame = frameIndex;

        // Berechne Molekülzentrum für diese Frame
        const auto& atoms = m_trajectoryAtoms[frameIndex];
        if (!atoms.isEmpty()) {
            QVector3D min = atoms[0].position;
            QVector3D max = atoms[0].position;

            for (const Atom& atom : atoms) {
                min = QVector3D(
                    qMin(min.x(), atom.position.x()),
                    qMin(min.y(), atom.position.y()),
                    qMin(min.z(), atom.position.z())
                );
                max = QVector3D(
                    qMax(max.x(), atom.position.x()),
                    qMax(max.y(), atom.position.y()),
                    qMax(max.z(), atom.position.z())
                );
            }

            m_moleculeCenter = (min + max) * 0.5f;
            m_moleculeRadius = (max - min).length() * 0.5f;
        }

        clearScene();
        Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(m_trajectoryAtoms[frameIndex], m_trajectoryBonds[frameIndex]);
        moleculeEntity->setParent(m_modelEntity);

        // Setze Kamera für diese Frame
        setDefaultView();

        // Claude Generated - Synchronize frame controls when frame changes
        if (m_frameSlider && m_frameLabel && m_frameJumpBox) {
            m_frameSlider->blockSignals(true);
            m_frameSlider->setValue(m_currentFrame);
            m_frameSlider->blockSignals(false);

            m_frameJumpBox->blockSignals(true);
            m_frameJumpBox->setValue(m_currentFrame);
            m_frameJumpBox->blockSignals(false);

            m_frameLabel->setText(QString("%1/%2").arg(m_currentFrame + 1).arg(m_frameCount));
        }

        // Claude Generated - Phase 2B: Update measurement visualization for new frame
        if (m_measurementOverlay && m_measurementMode != 0) {
            // Convert atom positions to world space for measurement overlay
            QVector<QVector3D> positions;
            for (const Atom& atom : m_trajectoryAtoms[frameIndex]) {
                positions.append(modelToWorld(atom.position));
            }
            m_measurementOverlay->updateMeasurement(positions);
        }

        emit frameChanged(m_currentFrame);
    }
}

// Claude Generated - Fast position-only update for animation (no scene rebuild)
void MoleculeViewer::updateFramePositions(int frameIndex)
{
    if (frameIndex < 0 || frameIndex >= m_trajectoryAtoms.size()) {
        return;
    }

    m_currentFrame = frameIndex;
    const auto& atoms = m_trajectoryAtoms[frameIndex];

    // Claude Generated - Use cached transform pointers: avoids componentsOfType<QTransform>() scan per atom/bond
    for (int i = 0; i < m_atomTransforms.size() && i < atoms.size(); ++i) {
        if (m_atomTransforms[i])
            m_atomTransforms[i]->setTranslation(atoms[i].position);
    }

    // Update bond transforms using cached pointers
    const QVector<Bond>& bonds = m_trajectoryBonds[frameIndex];
    // Claude Generated - Per-half bond layout: stride per bond = bondOrder * 2.
    int bondIndex = 0;
    for (int i = 0; i < bonds.size(); ++i) {
        if (bondIndex >= m_bondTransforms.size()) break;
        const Bond& bond = bonds[i];
        if (bond.atom1 < 0 || bond.atom1 >= atoms.size() ||
            bond.atom2 < 0 || bond.atom2 >= atoms.size()) {
            bondIndex += bond.bondOrder * 2;
            continue;
        }

        const QVector3D posA = atoms[bond.atom1].position;
        const QVector3D posB = atoms[bond.atom2].position;
        const QVector3D direction = posB - posA;
        const float length = direction.length();
        if (length < 1e-4f) {
            bondIndex += bond.bondOrder * 2;
            continue;
        }
        const QVector3D normDir = direction / length;

        QQuaternion rotation;
        {
            QVector3D localUp(0, 1, 0);
            QVector3D rotAxis = QVector3D::crossProduct(localUp, normDir);
            if (rotAxis.length() < 0.001f) {
                rotation = normDir.y() > 0 ? QQuaternion()
                    : QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f);
            } else {
                float angle = qAcos(QVector3D::dotProduct(localUp, normDir)) * 180.0f / float(M_PI);
                rotation = QQuaternion::fromAxisAndAngle(rotAxis.normalized(), angle);
            }
        }

        QVector3D offsetDir(0, 0, 0);
        if (bond.bondOrder > 1) {
            offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 0, 1)).normalized();
            if (offsetDir.length() < 0.001f)
                offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 1, 0)).normalized();
        }
        const float multiBondOffset = 0.2f;

        auto writeHalf = [&](const QVector3D& center) {
            if (bondIndex >= m_bondTransforms.size()) return;
            if (auto* t = m_bondTransforms[bondIndex]) {
                t->setTranslation(center);
                t->setScale3D(QVector3D(1.0f, length * 0.5f, 1.0f));
                t->setRotation(rotation);
            }
            ++bondIndex;
        };

        for (int r = 0; r < bond.bondOrder; ++r) {
            QVector3D repOffset(0, 0, 0);
            if (r > 0) {
                float currentOffset = (r % 2 == 0 ? 1 : -1) * multiBondOffset * ((r + 1) / 2);
                repOffset = offsetDir * currentOffset;
            }
            const QVector3D a = posA + repOffset;
            const QVector3D b = posB + repOffset;
            const QVector3D mid = 0.5f * (a + b);
            writeHalf(0.5f * (a + mid));
            writeHalf(0.5f * (mid + b));
        }
    }

    // Synchronize UI controls
    if (m_frameSlider && m_frameLabel && m_frameJumpBox) {
        m_frameSlider->blockSignals(true);
        m_frameSlider->setValue(m_currentFrame);
        m_frameSlider->blockSignals(false);

        m_frameJumpBox->blockSignals(true);
        m_frameJumpBox->setValue(m_currentFrame);
        m_frameJumpBox->blockSignals(false);

        m_frameLabel->setText(QString("%1/%2").arg(m_currentFrame + 1).arg(m_frameCount));
    }

    // Claude Generated - Phase 2B: Update measurement visualization for new frame (animation)
    if (m_measurementOverlay && m_measurementMode != 0) {
        QVector<QVector3D> positions;
        for (const Atom& atom : atoms) {
            positions.append(modelToWorld(atom.position));
        }
        m_measurementOverlay->updateMeasurement(positions);
    }

    emit frameChanged(m_currentFrame);
}

// Claude Generated - Fast atoms-only update for simulation: O(n) translate only, skip bond recalculation
// Bond transforms stay constant during MD (topology doesn't change), so we skip rotation math
void MoleculeViewer::updateAtomPositionsOnly(int frameIndex)
{
    if (frameIndex < 0 || frameIndex >= m_trajectoryAtoms.size()) {
        return;
    }

    m_currentFrame = frameIndex;
    const auto& atoms = m_trajectoryAtoms[frameIndex];

    // Claude Generated - Phase 3.1: Instancing path — upload position buffer in one call.
    if (m_atomInstancing && m_atomInstancing->getAtomCount() == atoms.size()) {
        QVector<QVector3D> positions;
        positions.reserve(atoms.size());
        for (const Atom& a : atoms)
            positions.append(a.position);
        m_atomInstancing->updateAtomPositions(positions);
        return;
    }

    // Atoms only: simple translate. No bond rotation, no quaternion math.
    for (int i = 0; i < m_atomTransforms.size() && i < atoms.size(); ++i) {
        if (m_atomTransforms[i])
            m_atomTransforms[i]->setTranslation(atoms[i].position);
    }
}

// Claude Generated - Precompute bond rotations once before simulation
// Call this once when simulation starts. Caches quaternion for each bond direction.
void MoleculeViewer::prepareSimulationBonds()
{
    m_bondRotations.clear();
    if (m_trajectoryAtoms.isEmpty() || m_trajectoryBonds.isEmpty())
        return;

    const auto& atoms = m_trajectoryAtoms[0];
    const QVector<Bond>& bonds = m_trajectoryBonds[0];

    for (const Bond& bond : bonds) {
        if (bond.atom1 >= atoms.size() || bond.atom2 >= atoms.size())
            continue;

        QVector3D direction = atoms[bond.atom2].position - atoms[bond.atom1].position;
        QVector3D normDir = direction.normalized();
        QVector3D localUp(0, 1, 0);
        QVector3D rotAxis = QVector3D::crossProduct(localUp, normDir);

        QQuaternion q;
        if (rotAxis.length() < 0.001f) {
            q = normDir.y() > 0 ? QQuaternion() : QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f);
        } else {
            float angle = qAcos(QVector3D::dotProduct(localUp, normDir)) * 180.0f / float(M_PI);
            q = QQuaternion::fromAxisAndAngle(rotAxis.normalized(), angle);
        }
        m_bondRotations.append(q);

        // Double/triple bonds: same rotation for offset bonds
        for (int j = 1; j < bond.bondOrder; ++j) {
            m_bondRotations.append(q);
        }
    }
    qDebug() << "Prepared" << m_bondRotations.size() << "bond rotations for simulation";
}

// Claude Generated - Hybrid bond update: rotation cached, update center+length only
// Called every simulation frame. Rotation pre-computed once at simulation start.
void MoleculeViewer::updateBondsHybrid(int frameIndex)
{
    if (frameIndex < 0 || frameIndex >= m_trajectoryAtoms.size())
        return;

    const auto& atoms = m_trajectoryAtoms[frameIndex];
    const QVector<Bond>& bonds = m_trajectoryBonds[frameIndex];

    // Claude Generated - Phase 3.2: Instanced bond path.
    // Recompute centers + halfLengths (rotations stay cached from scene build).
    // Multi-bond offsets approximated using cached rotations — visual parity with hybrid path.
    if (m_bondInstancing && m_bondInstancing->bondCount() > 0) {
        // Claude Generated - Per-half instanced bond layout: 2 instances per bond
        // replica. Emit centers/halfLengths in the same order as setBonds.
        QVector<QVector3D> centers;
        QVector<float> halfLengths;
        const int expected = m_bondInstancing->bondCount();
        centers.reserve(expected);
        halfLengths.reserve(expected);

        const float multiBondOffset = 0.2f;
        for (const Bond& bond : bonds) {
            if (bond.atom1 < 0 || bond.atom1 >= atoms.size()
                || bond.atom2 < 0 || bond.atom2 >= atoms.size())
                continue;
            const QVector3D posA = atoms[bond.atom1].position;
            const QVector3D posB = atoms[bond.atom2].position;
            const QVector3D direction = posB - posA;
            const float length = direction.length();
            if (length < 1e-4f)
                continue;
            const QVector3D normDir = direction / length;
            const float quarterLen = length * 0.25f;

            QVector3D offsetDir(0, 0, 0);
            if (bond.bondOrder > 1) {
                offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 0, 1)).normalized();
                if (offsetDir.length() < 0.001f)
                    offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 1, 0)).normalized();
            }

            for (int r = 0; r < bond.bondOrder; ++r) {
                QVector3D repOffset(0, 0, 0);
                if (r > 0) {
                    const float currentOffset = (r % 2 == 0 ? 1 : -1) * multiBondOffset * ((r + 1) / 2);
                    repOffset = offsetDir * currentOffset;
                }
                const QVector3D a = posA + repOffset;
                const QVector3D b = posB + repOffset;
                const QVector3D mid = 0.5f * (a + b);
                centers.append(0.5f * (a + mid));
                halfLengths.append(quarterLen);
                centers.append(0.5f * (mid + b));
                halfLengths.append(quarterLen);
            }
            if (centers.size() >= expected)
                break;
        }
        // Truncate in case of mismatch (shouldn't happen but guards buffer upload).
        if (centers.size() > expected) {
            centers.resize(expected);
            halfLengths.resize(expected);
        }
        if (centers.size() == expected)
            m_bondInstancing->updateTransforms(centers, halfLengths);
        return;
    }

    // Claude Generated - Per-half bond layout: stride per bond = bondOrder * 2.
    int bondIndex = 0;
    for (int i = 0; i < bonds.size() && bondIndex < m_bondTransforms.size(); ++i) {
        const Bond& bond = bonds[i];
        if (bond.atom1 < 0 || bond.atom1 >= atoms.size() ||
            bond.atom2 < 0 || bond.atom2 >= atoms.size()) {
            bondIndex += bond.bondOrder * 2;
            continue;
        }

        const QVector3D posA = atoms[bond.atom1].position;
        const QVector3D posB = atoms[bond.atom2].position;
        const QVector3D direction = posB - posA;
        const float length = direction.length();
        if (length < 1e-4f) {
            bondIndex += bond.bondOrder * 2;
            continue;
        }
        const QVector3D normDir = direction / length;

        QQuaternion rotation;
        {
            QVector3D localUp(0, 1, 0);
            QVector3D rotAxis = QVector3D::crossProduct(localUp, normDir);
            if (rotAxis.length() < 0.001f) {
                rotation = normDir.y() > 0 ? QQuaternion()
                    : QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f);
            } else {
                float angle = qAcos(QVector3D::dotProduct(localUp, normDir)) * 180.0f / float(M_PI);
                rotation = QQuaternion::fromAxisAndAngle(rotAxis.normalized(), angle);
            }
        }

        QVector3D offsetDir(0, 0, 0);
        if (bond.bondOrder > 1) {
            offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 0, 1)).normalized();
            if (offsetDir.length() < 0.001f)
                offsetDir = QVector3D::crossProduct(normDir, QVector3D(0, 1, 0)).normalized();
        }
        const float multiBondOffset = 0.2f;

        auto writeHalf = [&](const QVector3D& center) {
            if (bondIndex >= m_bondTransforms.size()) return;
            if (auto* t = m_bondTransforms[bondIndex]) {
                t->setTranslation(center);
                t->setScale3D(QVector3D(1.0f, length * 0.5f, 1.0f));
                t->setRotation(rotation);
            }
            ++bondIndex;
        };

        for (int r = 0; r < bond.bondOrder; ++r) {
            QVector3D repOffset(0, 0, 0);
            if (r > 0) {
                float currentOffset = (r % 2 == 0 ? 1 : -1) * multiBondOffset * ((r + 1) / 2);
                repOffset = offsetDir * currentOffset;
            }
            const QVector3D a = posA + repOffset;
            const QVector3D b = posB + repOffset;
            const QVector3D mid = 0.5f * (a + b);
            writeHalf(0.5f * (a + mid));
            writeHalf(0.5f * (mid + b));
        }
    }
}

// Apply a simulation frame to the scene. Worker emits are already throttled to the
// target fps in SimulationWorker::runMD/runOptimization, so Qt3D receives events in a
// deterministic cadence — no backpressure or ack needed.
void MoleculeViewer::updateSimulationFrame(SimulationFramePtr frame)
{
    if (!frame)
        return;

    const auto& positions = frame->positions;
    const int n = static_cast<int>(positions.size());

    // Topology-change fallback: rebuild the scene with cached element/charge where possible,
    // else synthesize placeholder atoms (carbon) so at least count matches.
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

    // First frame: pre-compute bond rotations once (if not already done)
    if (m_bondRotations.isEmpty())
        prepareSimulationBonds();

    // In-place position update: keep element/charge untouched (no per-frame copy of those).
    auto& refAtoms = m_trajectoryAtoms[0];
    for (int i = 0; i < n; ++i)
        refAtoms[i].position = positions[i];

#ifdef DEBUG_ON
    // Claude Generated - Per-frame timing: rolling avg every 60 frames to quantify render cost.
    QElapsedTimer frameTimer;
    frameTimer.start();

    QElapsedTimer sub;
    sub.start();
    updateAtomPositionsOnly(0);
    const qint64 atomNs = sub.nsecsElapsed();

    sub.restart();
    updateBondsHybrid(0);
    const qint64 bondNs = sub.nsecsElapsed();

    const qint64 totalNs = frameTimer.nsecsElapsed();

    static qint64 s_accAtom = 0, s_accBond = 0, s_accTotal = 0;
    static int s_samples = 0;
    s_accAtom += atomNs;
    s_accBond += bondNs;
    s_accTotal += totalNs;
    if (++s_samples >= 60) {
        qDebug().nospace()
            << "[sim-render] N=" << n
            << " instancing=" << (m_atomInstancing ? "on" : "off")
            << " avg total=" << (s_accTotal / s_samples / 1000) << "us"
            << " atoms=" << (s_accAtom / s_samples / 1000) << "us"
            << " bonds=" << (s_accBond / s_samples / 1000) << "us";
        s_accAtom = s_accBond = s_accTotal = 0;
        s_samples = 0;
    }
#else
    updateAtomPositionsOnly(0);
    updateBondsHybrid(0);
#endif
}

void MoleculeViewer::nextFrame()
{
    if (m_currentFrame < m_trajectoryAtoms.size() - 1) {
        showFrame(m_currentFrame + 1);
    }
}

void MoleculeViewer::previousFrame()
{
    if (m_currentFrame > 0) {
        showFrame(m_currentFrame - 1);
    }
}

void MoleculeViewer::setTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds)
{
    m_trajectoryAtoms = atoms;
    m_frameCount = atoms.size();
    m_currentFrame = 0;

    // Claude Generated - Auto-detect bonds if empty (for XYZ files)
    m_trajectoryBonds.clear();
    if (bonds.isEmpty() || (bonds.size() == atoms.size() && bonds[0].isEmpty())) {
        // Detect bonds for each frame
        for (int i = 0; i < atoms.size(); ++i) {
            QVector<Bond> detectedBonds = detectBonds(atoms[i]);
            m_trajectoryBonds.append(detectedBonds);
        }
    } else {
        // Use provided bonds
        m_trajectoryBonds = bonds;
    }

    // Claude Generated - Update frame controls based on frame count
    if (m_frameSlider && m_frameLabel && m_frameJumpBox && m_frameControlWidget) {
        m_frameSlider->setMaximum(m_frameCount - 1);
        m_frameJumpBox->setMaximum(m_frameCount - 1);
        m_frameLabel->setText(QString("1/%1").arg(m_frameCount));

        // Show/hide frame controls based on whether we have multiple frames
        bool hasMultipleFrames = m_frameCount > 1;
        m_frameControlWidget->setVisible(hasMultipleFrames);
    }

    // Load and display the first frame
    if (m_frameCount > 0) {
        showFrame(0);
    }

    // Emit signals for UI updates
    emit trajectoryLoaded(m_frameCount);

    // Claude Generated 2026 - Phase 6: advertise first frame to the sim dock.
    if (m_frameCount > 0)
        emit moleculeUpdated(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
}

void MoleculeViewer::showTrajectoryFrame(int frameIndex)
{
    if (frameIndex >= 0 && frameIndex < m_trajectoryAtoms.size()) {
        m_currentFrame = frameIndex;
        clearScene();
        
        // Use stored trajectory data
        Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(m_trajectoryAtoms[frameIndex], m_trajectoryBonds[frameIndex]);
        moleculeEntity->setParent(m_modelEntity);
        setDefaultView();
        
        emit frameChanged(m_currentFrame);
    }
}

void MoleculeViewer::setVTFTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds)
{
    // Same as setTrajectoryData, but specifically for VTF data
    setTrajectoryData(atoms, bonds);
}

void MoleculeViewer::clearScenePublic()
{
    clearScene();
}

// Claude Generated - Screenshot/Export functionality
void MoleculeViewer::saveScreenshot(const QString& filename, int scaleFactor)
{
    if (!m_container) {
        qWarning() << "Cannot save screenshot: Container not initialized";
        return;
    }

    // Get current widget size
    QSize originalSize = m_container->size();
    QSize captureSize = originalSize * scaleFactor;

    // Grab the container widget (which contains the 3D view) as a pixmap
    QPixmap screenshot = m_container->grab();

    // Scale if needed
    if (scaleFactor > 1) {
        // For higher resolution, we scale up the grabbed image
        // Note: This is upscaling after capture; for true high-res rendering,
        // Qt3D would need offscreen rendering at higher resolution
        screenshot = screenshot.scaled(captureSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Save to file
    if (screenshot.save(filename)) {
        qDebug() << "Screenshot saved to:" << filename;
    } else {
        qWarning() << "Failed to save screenshot to:" << filename;
    }
}

void MoleculeViewer::saveScreenshotDialog()
{
    // Create file dialog
    QString filter = tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;All Files (*)");
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), QString(), filter);

    if (filename.isEmpty()) {
        return;  // User cancelled
    }

    // Ask for scale factor
    bool ok;
    int scaleFactor = QInputDialog::getInt(this, tr("Screenshot Resolution"),
                                           tr("Resolution multiplier (1x = current size):"),
                                           1, 1, 4, 1, &ok);

    if (!ok) {
        return;  // User cancelled
    }

    // Save screenshot
    saveScreenshot(filename, scaleFactor);

    // Show confirmation
    QMessageBox::information(this, tr("Screenshot Saved"),
                            tr("Screenshot saved to:\n%1").arg(filename));
}

// Claude Generated - Fog/Depth effect functions
void MoleculeViewer::setFogEnabled(bool enabled)
{
    m_fogEnabled = enabled;
    // Note: Actual fog rendering would require shader modifications
    // For now, this stores the setting for potential future use
}

void MoleculeViewer::setFogIntensity(float intensity)
{
    m_fogIntensity = qBound(0.0f, intensity, 1.0f);
}

// Claude Generated - Phase 5A: SSAO post-processing control
void MoleculeViewer::setSSAOEnabled(bool enabled)
{
    m_ssaoEnabled = enabled;
    if (m_frameGraph) {
        m_frameGraph->setSSAOEnabled(enabled);
    }
}

void MoleculeViewer::setSSAOIntensity(float intensity)
{
    m_ssaoIntensity = qBound(0.0f, intensity, 2.0f);
    if (m_frameGraph) {
        m_frameGraph->setSSAOIntensity(m_ssaoIntensity);
    }
}

void MoleculeViewer::setSSAORadius(float radius)
{
    m_ssaoRadius = qBound(0.01f, radius, 0.2f);
    if (m_frameGraph) {
        m_frameGraph->setSSAORadius(m_ssaoRadius);
    }
}

void MoleculeViewer::setSSAOBias(float bias)
{
    m_ssaoBias = qBound(0.0f, bias, 0.1f);
    if (m_frameGraph) {
        m_frameGraph->setSSAOBias(m_ssaoBias);
    }
}

// Claude Generated - Phase 5B: Bloom and HDR post-processing control
void MoleculeViewer::setBloomEnabled(bool enabled)
{
    m_bloomEnabled = enabled;
    if (m_frameGraph) {
        m_frameGraph->setBloomEnabled(enabled);
    }
}

void MoleculeViewer::setBloomThreshold(float threshold)
{
    m_bloomThreshold = qBound(0.5f, threshold, 1.5f);
    if (m_frameGraph) {
        m_frameGraph->setBloomThreshold(m_bloomThreshold);
    }
}

void MoleculeViewer::setBloomIntensity(float intensity)
{
    m_bloomIntensity = qBound(0.0f, intensity, 2.0f);
    if (m_frameGraph) {
        m_frameGraph->setBloomIntensity(m_bloomIntensity);
    }
}

void MoleculeViewer::setHDREnabled(bool enabled)
{
    m_hdrEnabled = enabled;
    if (m_frameGraph) {
        m_frameGraph->setHDREnabled(enabled);
    }
}

void MoleculeViewer::setExposure(float exposure)
{
    m_exposure = qBound(0.5f, exposure, 3.0f);
    if (m_frameGraph) {
        m_frameGraph->setExposure(m_exposure);
    }
}

// Claude Generated - Trajectory animation functions
void MoleculeViewer::startAnimation()
{
    qDebug() << "▶ startAnimation() called - trajectorySize:" << m_trajectoryAtoms.size()
             << "currentFrame:" << m_currentFrame << "isAnimating:" << m_isAnimating;

    if (m_trajectoryAtoms.size() <= 1) {
        qWarning() << "Cannot start animation: no trajectory data";
        return;
    }

    if (m_isAnimating) {
        qDebug() << "Already animating, returning";
        return;  // Already animating
    }

    m_isAnimating = true;

    if (!m_animationTimer) {
        m_animationTimer = new QTimer(this);
        connect(m_animationTimer, &QTimer::timeout, this, &MoleculeViewer::onAnimationTick);
        qDebug() << "Created new animation timer";
    }

    // Set interval based on FPS (1000ms / FPS = interval in ms)
    int intervalMs = qMax(1, 1000 / m_animationFPS);
    m_animationTimer->start(intervalMs);
    qDebug() << "Timer started with interval:" << intervalMs << "ms (FPS:" << m_animationFPS << ")";
}

void MoleculeViewer::stopAnimation()
{
    if (m_animationTimer) {
        m_animationTimer->stop();
    }
    m_isAnimating = false;
}

void MoleculeViewer::setAnimationFPS(int fps)
{
    m_animationFPS = qBound(1, fps, 60);  // Clamp between 1-60 FPS

    // If animating, update timer interval
    if (m_isAnimating && m_animationTimer) {
        int intervalMs = qMax(1, 1000 / m_animationFPS);
        m_animationTimer->setInterval(intervalMs);
    }
}

void MoleculeViewer::onAnimationTick()
{
    if (m_trajectoryAtoms.isEmpty()) {
        stopAnimation();
        return;
    }

    int nextFrame = m_currentFrame + 1;

    if (nextFrame >= m_trajectoryAtoms.size()) {
        if (m_animationLoop) {
            nextFrame = 0;  // Loop back to beginning
        } else {
            stopAnimation();  // Stop at end
            return;
        }
    }

    // Claude Generated - Use fast position-only update for smooth animation (no scene rebuild)
    updateFramePositions(nextFrame);
}

// Claude Generated - Create a separator widget
QFrame* MoleculeViewer::createSeparator()
{
    QFrame *separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setMaximumWidth(2);
    return separator;
}

// Claude Generated - Setup integrated control panel (refactored for frame navigation)
void MoleculeViewer::setupControlPanel()
{
    m_controlPanel = new QWidget;
    m_controlPanel->setMaximumHeight(50);

    // Claude Generated - Adaptive styling for light/dark mode
    m_controlPanel->setAutoFillBackground(true);
    QPalette pal = m_controlPanel->palette();
    // Use system palette for background - automatically adapts to theme
    pal.setColor(QPalette::Window, pal.color(QPalette::Base).lighter(105));
    m_controlPanel->setPalette(pal);
    // Border style - automatically uses appropriate color
    m_controlPanel->setStyleSheet("QWidget { border-bottom: 1px solid palette(mid); }");

    QHBoxLayout *panelLayout = new QHBoxLayout(m_controlPanel);
    panelLayout->setContentsMargins(5, 3, 5, 3);
    panelLayout->setSpacing(5);

    // Claude Generated - FRAME NAVIGATION SECTION (hidden when frameCount == 1)
    m_frameControlWidget = new QWidget;
    QHBoxLayout *frameLayout = new QHBoxLayout(m_frameControlWidget);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(3);

    // Previous frame button
    QPushButton *prevButton = new QPushButton("◀");
    prevButton->setMaximumWidth(30);
    prevButton->setToolTip(tr("Previous Frame"));
    connect(prevButton, &QPushButton::clicked, this, &MoleculeViewer::previousFrame);
    frameLayout->addWidget(prevButton);

    // Next frame button
    QPushButton *nextButton = new QPushButton("▶");
    nextButton->setMaximumWidth(30);
    nextButton->setToolTip(tr("Next Frame"));
    connect(nextButton, &QPushButton::clicked, this, &MoleculeViewer::nextFrame);
    frameLayout->addWidget(nextButton);

    // Frame slider
    m_frameSlider = new QSlider(Qt::Horizontal);
    m_frameSlider->setMinimum(0);
    m_frameSlider->setMaximum(0);
    m_frameSlider->setToolTip(tr("Navigate through trajectory frames"));
    connect(m_frameSlider, &QSlider::valueChanged, this, &MoleculeViewer::showFrame);
    frameLayout->addWidget(m_frameSlider, 1);  // Stretch to fill available space

    // Frame label
    m_frameLabel = new QLabel("0/0");
    m_frameLabel->setMinimumWidth(50);
    m_frameLabel->setAlignment(Qt::AlignCenter);
    frameLayout->addWidget(m_frameLabel);

    // Frame jump spinbox
    m_frameJumpBox = new QSpinBox;
    m_frameJumpBox->setMinimum(0);
    m_frameJumpBox->setMaximum(0);
    m_frameJumpBox->setMaximumWidth(60);
    m_frameJumpBox->setToolTip(tr("Jump to frame number"));
    // Bidirectional sync: spinbox → slider
    connect(m_frameJumpBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        if (value != m_frameSlider->value()) {
            m_frameSlider->blockSignals(true);
            m_frameSlider->setValue(value);
            m_frameSlider->blockSignals(false);
        }
    });
    frameLayout->addWidget(m_frameJumpBox);

    m_frameControlWidget->setVisible(false);  // Hidden by default (shown when frameCount > 1)
    panelLayout->addWidget(m_frameControlWidget, 1);

    // Separator
    panelLayout->addWidget(createSeparator());

    // Claude Generated - PLAYBACK SECTION
    // Play button
    QPushButton *playButton = new QPushButton;
    playButton->setIcon(QIcon::fromTheme("media-playback-start"));
    playButton->setToolTip(tr("Play Animation"));
    playButton->setMaximumWidth(30);
    connect(playButton, &QPushButton::clicked, this, &MoleculeViewer::startAnimation);
    panelLayout->addWidget(playButton);

    // Pause button
    QPushButton *pauseButton = new QPushButton;
    pauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseButton->setToolTip(tr("Pause Animation"));
    pauseButton->setMaximumWidth(30);
    connect(pauseButton, &QPushButton::clicked, this, &MoleculeViewer::stopAnimation);
    panelLayout->addWidget(pauseButton);

    // FPS spinner
    QSpinBox *fpsSpinBox = new QSpinBox;
    fpsSpinBox->setRange(1, 60);
    fpsSpinBox->setValue(m_animationFPS);
    fpsSpinBox->setMaximumWidth(50);
    fpsSpinBox->setSuffix(" fps");
    connect(fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        setAnimationFPS(value);
    });
    panelLayout->addWidget(fpsSpinBox);

    // Loop checkbox
    QCheckBox *loopCheckbox = new QCheckBox(tr("Loop"));
    loopCheckbox->setChecked(true);
    connect(loopCheckbox, &QCheckBox::toggled, this, &MoleculeViewer::setAnimationLoop);
    panelLayout->addWidget(loopCheckbox);

    // Separator
    panelLayout->addWidget(createSeparator());

    // Claude Generated - RENDERING SECTION
    // Mode selector
    QComboBox *modeCombo = new QComboBox;
    modeCombo->addItem(tr("Ball & Stick"), static_cast<int>(RenderingMode::BallAndStick));
    modeCombo->addItem(tr("Space-Filling"), static_cast<int>(RenderingMode::SpaceFilling));
    modeCombo->addItem(tr("Wireframe"), static_cast<int>(RenderingMode::Wireframe));
    modeCombo->addItem(tr("Sticks"), static_cast<int>(RenderingMode::SticksOnly));
    modeCombo->setMaximumWidth(110);
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, modeCombo](int index) {
        setRenderingMode(static_cast<RenderingMode>(modeCombo->itemData(index).toInt()));
    });
    panelLayout->addWidget(modeCombo);

    // Color scheme selector
    QComboBox *colorCombo = new QComboBox;
    colorCombo->addItem(tr("CPK"), static_cast<int>(ColorScheme::CPK));
    colorCombo->addItem(tr("Monochrome"), static_cast<int>(ColorScheme::Monochrome));
    colorCombo->addItem(tr("By Charge"), static_cast<int>(ColorScheme::ByCharge));
    colorCombo->setMaximumWidth(100);
    connect(colorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, colorCombo](int index) {
        setColorScheme(static_cast<ColorScheme>(colorCombo->itemData(index).toInt()));
    });
    panelLayout->addWidget(colorCombo);

    // Claude Generated - Phase 4 Final: Material mode selector (Phong vs PBR)
    QComboBox *materialCombo = new QComboBox;
    materialCombo->addItem(tr("Phong"), static_cast<int>(MaterialMode::Phong));
    materialCombo->addItem(tr("PBR"), static_cast<int>(MaterialMode::PBR));
    materialCombo->setMaximumWidth(80);
    materialCombo->setToolTip(tr("Phong: Traditional lighting\nPBR: Physically-based rendering"));
    connect(materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, materialCombo](int index) {
        setMaterialMode(static_cast<MaterialMode>(materialCombo->itemData(index).toInt()));
    });
    panelLayout->addWidget(materialCombo);

    // Claude Generated - Phase 4 Final: Glow intensity slider for selection highlighting
    QSlider *glowSlider = new QSlider(Qt::Horizontal);
    glowSlider->setMinimum(10);  // 1.0x
    glowSlider->setMaximum(20);  // 2.0x
    glowSlider->setValue(10);
    glowSlider->setMaximumWidth(60);
    glowSlider->setToolTip(tr("Selection glow intensity (1.0-2.0x)"));
    connect(glowSlider, &QSlider::valueChanged, [this, glowSlider](int value) {
        setGlowIntensity(value / 10.0f);  // Convert slider value to 1.0-2.0 range
    });
    panelLayout->addWidget(glowSlider);

    // Separator
    panelLayout->addWidget(createSeparator());

    // Measurement selector - Claude Generated Phase 2B: Added Dihedral measurement
    QComboBox *measureCombo = new QComboBox;
    measureCombo->addItem(tr("No Measurement"), 0);
    measureCombo->addItem(tr("Distance (2 atoms)"), 1);
    measureCombo->addItem(tr("Angle (3 atoms)"), 2);
    measureCombo->addItem(tr("Dihedral (4 atoms)"), 3);
    measureCombo->setMaximumWidth(150);
    connect(measureCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, measureCombo](int index) {
        setMeasurementMode(measureCombo->itemData(index).toInt());
    });
    panelLayout->addWidget(measureCombo);

    // Separator
    panelLayout->addWidget(createSeparator());

    // Claude Generated - Phase 4B: BOND EDITING SECTION
    QComboBox *bondEditCombo = new QComboBox;
    bondEditCombo->addItem(tr("No Bond Edit"), 0);
    bondEditCombo->addItem(tr("Add Bond (B)"), 1);
    bondEditCombo->addItem(tr("Delete Bond (D)"), 2);
    bondEditCombo->addItem(tr("Cycle Order (O)"), 3);
    bondEditCombo->setMaximumWidth(130);
    connect(bondEditCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, bondEditCombo](int index) {
        setBondEditMode(bondEditCombo->itemData(index).toInt());
    });
    panelLayout->addWidget(bondEditCombo);

    // Separator
    panelLayout->addWidget(createSeparator());

    // Claude Generated - 4 corner light toggles. Compact 2×2 grid of
    // QToolButton switches labeled by corner (TL=top-left, TR, BL=back-left,
    // BR=back-right relative to +Z front). Hover tooltip clarifies each.
    {
        QWidget* lightsBox = new QWidget;
        QGridLayout* g = new QGridLayout(lightsBox);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(1);
        struct CornerSpec { const char* label; const char* tip; int row; int col; };
        const CornerSpec specs[4] = {
            {"◤", "Top-front-left light",  0, 0},
            {"◥", "Top-front-right light", 0, 1},
            {"◣", "Top-back-left light",   1, 0},
            {"◢", "Top-back-right light",  1, 1},
        };
        for (int i = 0; i < 4; ++i) {
            QToolButton* btn = new QToolButton;
            btn->setText(tr(specs[i].label));
            btn->setToolTip(tr(specs[i].tip));
            btn->setCheckable(true);
            btn->setChecked(m_cornerLightEnabled[i]);
            btn->setFixedSize(22, 22);
            connect(btn, &QToolButton::toggled, this, [this, i](bool on) {
                setCornerLightEnabled(i, on);
            });
            g->addWidget(btn, specs[i].row, specs[i].col);
        }
        panelLayout->addWidget(lightsBox);
    }

    // Claude Generated - Background color picker.
    QPushButton* bgColorBtn = new QPushButton(tr("BG"));
    bgColorBtn->setToolTip(tr("Change 3D view background color"));
    bgColorBtn->setMaximumWidth(40);
    auto applyBgSwatch = [this, bgColorBtn]() {
        bgColorBtn->setStyleSheet(QString("background-color: %1;").arg(m_backgroundColor.name()));
    };
    applyBgSwatch();
    connect(bgColorBtn, &QPushButton::clicked, this, [this, applyBgSwatch]() {
        QColor c = QColorDialog::getColor(m_backgroundColor, this, tr("Background Color"));
        if (c.isValid()) {
            setBackgroundColor(c);
            applyBgSwatch();
        }
    });
    panelLayout->addWidget(bgColorBtn);

    panelLayout->addStretch();
}


// Claude Generated - Selection and measurement functions
void MoleculeViewer::clearSelection()
{
    m_selectedAtoms.clear();
    m_measurementMode = 0;

    // Claude Generated - Phase 2A: Also clear SelectionManager and update visuals
    if (m_selectionManager) {
        m_selectionManager->clearSelection();
    }
    updateMaterials();
}

void MoleculeViewer::setMeasurementMode(int mode)
{
    m_measurementMode = qBound(0, mode, 3);  // Claude Generated - Phase 2B: Clamp 0-3 (None, Distance, Angle, Dihedral)

    // Handle measurement mode changes
    if (m_measurementMode == 0) {
        // No measurement - clear visualization
        if (m_measurementOverlay) {
            m_measurementOverlay->clearMeasurement();
        }
    } else {
        // Active measurement mode - will be updated when atoms are selected
        // Type is determined by number of selected atoms:
        // 2 atoms = Distance (mode 1)
        // 3 atoms = Angle (mode 2)
        // 4+ atoms = Dihedral (mode 3)
    }
}

// Claude Generated - Phase 4B: Bond edit mode management
void MoleculeViewer::setBondEditMode(int mode)
{
    m_bondEditMode = qBound(0, mode, 3);  // Clamp 0-3 (None, AddBond, DeleteBond, ChangeBondOrder)

    if (m_bondEditor) {
        // Convert MoleculeViewer mode to BondEditor mode
        BondEditor::EditMode editorMode;
        switch (m_bondEditMode) {
            case 0: editorMode = BondEditor::EditMode::None; break;
            case 1: editorMode = BondEditor::EditMode::AddBondMode; break;
            case 2: editorMode = BondEditor::EditMode::DeleteBondMode; break;
            case 3: editorMode = BondEditor::EditMode::ChangeBondMode; break;
            default: editorMode = BondEditor::EditMode::None;
        }
        m_bondEditor->setEditMode(editorMode);

        // Log mode change for debugging
        const char* modeNames[] = {"None", "AddBond", "DeleteBond", "ChangeBondOrder"};
        qDebug() << "Bond edit mode:" << modeNames[m_bondEditMode];
    }
}

void MoleculeViewer::updateMeasurementDisplay()
{
    if (m_selectedAtoms.isEmpty() || m_trajectoryAtoms.isEmpty()) {
        return;
    }

    const auto& atoms = m_trajectoryAtoms[m_currentFrame];

    if (m_selectedAtoms.size() >= 2 && m_measurementMode == 1) {
        // Distance measurement
        int idx0 = m_selectedAtoms[0];
        int idx1 = m_selectedAtoms[1];

        if (idx0 >= 0 && idx0 < atoms.size() && idx1 >= 0 && idx1 < atoms.size()) {
            QVector3D v1 = atoms[idx0].position;
            QVector3D v2 = atoms[idx1].position;
            float distance = (v2 - v1).length();

            qDebug() << QString("Distance: %1-%2 = %3 Å")
                .arg(atoms[idx0].element).arg(atoms[idx1].element).arg(distance, 0, 'f', 3);
        }
    }

    if (m_selectedAtoms.size() >= 3 && m_measurementMode == 2) {
        // Angle measurement
        int idx0 = m_selectedAtoms[0];
        int idx1 = m_selectedAtoms[1];
        int idx2 = m_selectedAtoms[2];

        if (idx0 >= 0 && idx0 < atoms.size() && idx1 >= 0 && idx1 < atoms.size() && idx2 >= 0 && idx2 < atoms.size()) {
            QVector3D v1 = atoms[idx0].position;
            QVector3D v2 = atoms[idx1].position;
            QVector3D v3 = atoms[idx2].position;

            QVector3D vec1 = (v1 - v2).normalized();
            QVector3D vec2 = (v3 - v2).normalized();

            float cosAngle = QVector3D::dotProduct(vec1, vec2);
            cosAngle = qBound(-1.0f, cosAngle, 1.0f);
            float angle = qAcos(cosAngle) * 180.0f / M_PI;

            qDebug() << QString("Angle: %1-%2-%3 = %4°")
                .arg(atoms[idx0].element).arg(atoms[idx1].element).arg(atoms[idx2].element)
                .arg(angle, 0, 'f', 1);
        }
    }
}

// Claude Generated - Refresh visualization without camera reset
void MoleculeViewer::refreshVisualization()
{
    if (m_trajectoryAtoms.isEmpty() || m_currentFrame < 0 || m_currentFrame >= m_trajectoryAtoms.size()) {
        return;
    }

    // Clear and re-render the scene WITHOUT changing camera
    clearScene();
    Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(
        m_trajectoryAtoms[m_currentFrame],
        m_trajectoryBonds[m_currentFrame]
    );
    moleculeEntity->setParent(m_modelEntity);

    // NOTE: Intentionally NO setDefaultView() call - preserve camera position!
}

// Claude Generated - Focus & Zoom Commands
void MoleculeViewer::getSelectedBounds(QVector3D& center, float& radius)
{
    if (m_trajectoryAtoms.isEmpty() || m_currentFrame >= m_trajectoryAtoms.size()) {
        center = m_moleculeCenter;
        radius = m_moleculeRadius;
        return;
    }

    const auto& atoms = m_trajectoryAtoms[m_currentFrame];
    if (atoms.isEmpty()) {
        center = m_moleculeCenter;
        radius = m_moleculeRadius;
        return;
    }

    // Calculate bounding sphere for selected atoms
    if (m_selectedAtoms.isEmpty()) {
        // Use all atoms if nothing selected
        center = m_moleculeCenter;
        radius = m_moleculeRadius;
    } else {
        // Calculate center of selected atoms
        QVector3D sum(0, 0, 0);
        for (int idx : m_selectedAtoms) {
            if (idx < atoms.size()) {
                sum += atoms[idx].position;
            }
        }
        center = sum / m_selectedAtoms.size();

        // Calculate radius as max distance from center
        radius = 0.0f;
        for (int idx : m_selectedAtoms) {
            if (idx < atoms.size()) {
                float dist = (atoms[idx].position - center).length();
                if (dist > radius) radius = dist;
            }
        }
        radius += 2.0f;  // Add padding
    }
}

void MoleculeViewer::centerOnAtom(int atomIndex)
{
    if (m_trajectoryAtoms.isEmpty() || m_currentFrame >= m_trajectoryAtoms.size()) return;
    const auto& atoms = m_trajectoryAtoms[m_currentFrame];
    if (atomIndex < 0 || atomIndex >= atoms.size()) return;

    // Move camera to look at selected atom
    const auto& atom = atoms[atomIndex];
    // Transform model-local position to world space for camera targeting
    QVector3D worldPos = m_modelRotation.rotatedVector(atom.position - m_moleculeCenter) + m_moleculeCenter;
    QVector3D direction = m_camera->position() - worldPos;
    if (direction.length() < 0.001f) {
        direction = QVector3D(0, 0, 1);  // Default if too close
    }
    direction.normalize();

    float distance = 5.0f;  // Default viewing distance
    m_camera->setPosition(worldPos + direction * distance);
    m_camera->setViewCenter(worldPos);

    updateInstancingCameraPosition();
}

void MoleculeViewer::zoomToSelection(const QVector<int>& atomIndices)
{
    if (atomIndices.isEmpty()) {
        fitAllInView();
        return;
    }

    m_selectedAtoms = atomIndices;
    QVector3D center;
    float radius;
    getSelectedBounds(center, radius);

    // Transform model-local center to world space
    QVector3D worldCenter = m_modelRotation.rotatedVector(center - m_moleculeCenter) + m_moleculeCenter;

    // Position camera to view selection
    QVector3D direction = m_camera->position() - worldCenter;
    if (direction.length() < 0.001f) {
        direction = QVector3D(0, 0, 1);
    }
    direction.normalize();

    // Calculate distance to frame the sphere in view
    // For 45 degree FOV, distance = radius / tan(22.5 degrees)
    float distance = radius / 0.4142f;  // ~tan(22.5°)
    if (distance < 1.0f) distance = 1.0f;

    m_camera->setPosition(worldCenter + direction * distance);
    m_camera->setViewCenter(worldCenter);

    updateInstancingCameraPosition();
}

void MoleculeViewer::fitAllInView()
{
    if (m_trajectoryAtoms.isEmpty() || m_currentFrame >= m_trajectoryAtoms.size()) return;

    // Molecule center in world space (may be offset by model rotation)
    QVector3D worldCenter = m_modelRotation.rotatedVector(m_moleculeCenter - m_moleculeCenter) + m_moleculeCenter;
    float radius = m_moleculeRadius;

    // Position camera to view entire molecule
    QVector3D direction = m_camera->position() - worldCenter;
    if (direction.length() < 0.001f) {
        direction = QVector3D(0, 0, 1);
    }
    direction.normalize();

    // Calculate distance to frame the entire molecule
    float distance = radius / 0.4142f;  // ~tan(22.5°) for 45 degree FOV
    if (distance < 1.0f) distance = 5.0f;

    m_camera->setPosition(worldCenter + direction * distance);
    m_camera->setViewCenter(worldCenter);

    updateInstancingCameraPosition();
}

// Claude Generated - Phase 2A: Handle ObjectPicker click events
void MoleculeViewer::onAtomPicked(Qt3DRender::QPickEvent *pickEvent)
{
    // Find which atom was picked
    Qt3DRender::QObjectPicker *picker = qobject_cast<Qt3DRender::QObjectPicker*>(sender());
    if (!picker || !m_atomPickerToIndex.contains(picker)) {
        return;
    }

    int atomIndex = m_atomPickerToIndex[picker];

    // Check modifier keys for selection mode
    bool appendSelection = (pickEvent->modifiers() & Qt::ControlModifier) != 0;
    bool toggleSelection = (pickEvent->modifiers() & Qt::ShiftModifier) != 0;

    if (toggleSelection) {
        m_selectionManager->toggleAtom(atomIndex);
    } else {
        m_selectionManager->selectAtom(atomIndex, appendSelection);
    }

    // Update materials to show selection highlighting
    updateMaterials();

    // Claude Generated - Phase 2B: Update measurement if in measurement mode
    if (m_measurementMode != 0 && m_measurementOverlay) {
        const auto& selectedAtoms = m_selectionManager->selectedAtoms();
        int numSelected = selectedAtoms.size();

        // Determine measurement type based on selection count
        MeasurementOverlay::MeasurementType measurementType = MeasurementOverlay::NoMeasurement;
        if (numSelected == 2 && m_measurementMode >= 1) {
            measurementType = MeasurementOverlay::Distance;
        } else if (numSelected == 3 && m_measurementMode >= 2) {
            measurementType = MeasurementOverlay::Angle;
        } else if (numSelected >= 4 && m_measurementMode >= 3) {
            measurementType = MeasurementOverlay::Dihedral;
        }

        // Set measurement and update visualization
        if (measurementType != MeasurementOverlay::NoMeasurement) {
            m_measurementOverlay->setMeasurement(measurementType, QVector<int>(selectedAtoms));

            // Update with current frame positions (world space)
            if (m_currentFrame < m_trajectoryAtoms.size()) {
                QVector<QVector3D> positions;
                for (const Atom& atom : m_trajectoryAtoms[m_currentFrame]) {
                    positions.append(modelToWorld(atom.position));
                }
                m_measurementOverlay->updateMeasurement(positions);
            }
        }
    }

    // Emit signal for downstream UI updates
    emit selectionChanged(m_selectionManager->selectedAtoms());
}

// Claude Generated 2026 - Phase 5: In sim-mode, a press on an atom enters
// grab mode — subsequent mouse-move deltas generate atomForceRequested() until
// release. Outside sim-mode we ignore the press entirely so the existing
// clicked→onAtomPicked selection path is unaffected.
void MoleculeViewer::onAtomPressedForGrab(Qt3DRender::QPickEvent *pickEvent)
{
    Q_UNUSED(pickEvent);
    if (!m_simulationActive)
        return;
    Qt3DRender::QObjectPicker *picker = qobject_cast<Qt3DRender::QObjectPicker*>(sender());
    if (!picker || !m_atomPickerToIndex.contains(picker))
        return;
    m_grabbedAtom = m_atomPickerToIndex[picker];
    // Change cursor to indicate active grabbing
    if (m_container)
        m_container->setCursor(Qt::ClosedHandCursor);
}

// Claude Generated - Visual feedback when hovering over grab-capable atoms
// Changes cursor to indicate that the atom can be grabbed during simulation
void MoleculeViewer::onAtomHoverEntered()
{
    if (!m_simulationActive)
        return;

    Qt3DRender::QObjectPicker *picker = qobject_cast<Qt3DRender::QObjectPicker*>(sender());
    if (!picker || !m_atomPickerToIndex.contains(picker))
        return;

    int atomIndex = m_atomPickerToIndex[picker];

    // Safety check: ensure trajectory data is valid
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()
        && atomIndex >= 0 && atomIndex < m_trajectoryAtoms[m_currentFrame].size()) {
        const Atom& atom = m_trajectoryAtoms[m_currentFrame][atomIndex];
        // Emit status message with element and index for potential display
        emit grabStatusChanged(tr("Grab atom %1 (%2) - click and drag to apply force")
            .arg(atomIndex + 1).arg(atom.element));
    }

    // Change cursor to "Open Hand" to indicate grab-ability
    if (m_container) {
        m_container->setCursor(Qt::OpenHandCursor);
    }
}

// Claude Generated - Clear visual feedback when leaving atom
void MoleculeViewer::onAtomHoverExited()
{
    if (!m_simulationActive)
        return;

    // Only reset cursor if we're not currently grabbing
    if (m_grabbedAtom < 0 && m_container) {
        m_container->setCursor(Qt::ArrowCursor);
    }
}

// Claude Generated - Phase 4B: Bond picking and editing
void MoleculeViewer::onBondPicked(Qt3DRender::QPickEvent *pickEvent)
{
    // Find which bond was picked
    Qt3DRender::QObjectPicker *picker = qobject_cast<Qt3DRender::QObjectPicker*>(sender());
    if (!picker || !m_bondPickerToIndex.contains(picker)) {
        return;
    }

    int bondIndex = m_bondPickerToIndex[picker];
    if (!m_bondEditor) {
        return;
    }

    // Handle based on current edit mode
    switch (m_bondEditMode) {
        case 0:  // None - just select/deselect
            m_bondEditor->selectBond(bondIndex);
            break;

        case 1: {  // AddBond mode - click 2 atoms
            // Get bond atoms from pickEvent or selectionManager
            const auto& selectedAtoms = m_selectionManager->selectedAtoms();
            if (selectedAtoms.size() >= 2) {
                int atom1 = selectedAtoms[selectedAtoms.size() - 2];
                int atom2 = selectedAtoms[selectedAtoms.size() - 1];

                if (m_bondEditor->addBond(atom1, atom2, 1)) {
                    m_firstSelectedAtomForBond = -1;  // Reset
                    refreshVisualization();  // Rebuild geometry with new bond
                } else {
                    qWarning() << "Failed to add bond";
                }
            }
            break;
        }

        case 2:  // DeleteBond mode - click bond to delete
            if (m_bondEditor->removeBond(bondIndex)) {
                refreshVisualization();  // Rebuild geometry without bond
            } else {
                qWarning() << "Failed to delete bond";
            }
            break;

        case 3:  // ChangeBondOrder mode - click to cycle 1->2->3->1
            {
                const auto& bonds = m_bondEditor->getAllBonds();
                if (bondIndex < bonds.size()) {
                    int currentOrder = bonds[bondIndex].bondOrder;
                    int newOrder = (currentOrder % 3) + 1;  // Cycle: 1->2, 2->3, 3->1

                    if (m_bondEditor->changeBondOrder(bondIndex, newOrder)) {
                        refreshVisualization();  // Rebuild geometry with new bond order
                    } else {
                        qWarning() << "Failed to change bond order";
                    }
                }
            }
            break;

        default:
            break;
    }
}

// Claude Generated - Phase 4B: Auto-save structure changes
void MoleculeViewer::onStructureChanged()
{
    // Signal from BondEditor when structure changes
    if (m_autoSaveEnabled && !m_currentFilePath.isEmpty()) {
        m_hasUnsavedChanges = true;
        qDebug() << "Structure changed - marking for auto-save";

        // Restart debouncing timer
        m_autoSaveTimer->stop();
        m_autoSaveTimer->start();
    }
}

// Claude Generated - Phase 4B: Auto-save timer trigger - write XYZ file
void MoleculeViewer::onAutoSaveTimer()
{
    if (!m_autoSaveEnabled || m_currentFilePath.isEmpty() || m_trajectoryAtoms.isEmpty()) {
        return;
    }

    if (!m_hasUnsavedChanges) {
        return;  // Nothing to save
    }

    // Create backup on first edit
    if (!QFile::exists(m_currentFilePath + ".backup")) {
        QFile::copy(m_currentFilePath, m_currentFilePath + ".backup");
        qDebug() << "Created backup:" << m_currentFilePath + ".backup";
    }

    // Convert current frame to XYZ and write
    XYZParser::XYZFrame xyzFrame;
    if (XYZParser::convertFromMoleculeViewer(m_trajectoryAtoms[m_currentFrame],
                                             QString("Auto-saved frame %1").arg(m_currentFrame + 1),
                                             xyzFrame)) {
        if (XYZParser::writeFile(m_currentFilePath, xyzFrame)) {
            m_hasUnsavedChanges = false;
            qDebug() << "Auto-saved to:" << m_currentFilePath;
        } else {
            qWarning() << "Auto-save failed for:" << m_currentFilePath;
        }
    }
}

// Claude Generated - Phase 2A: Direct selection from internal calls
void MoleculeViewer::selectAtom(int index, bool append)
{
    if (!append && !m_selectedAtoms.isEmpty()) {
        clearSelection();
    }

    if (!m_selectedAtoms.contains(index)) {
        m_selectedAtoms.append(index);
        m_selectionManager->selectAtom(index, append);
        updateMaterials();
        emit selectionChanged(m_selectedAtoms);
    }
}

// Claude Generated - Phase 2C: Atom data accessors for AtomListPanel
QVector<QVector3D> MoleculeViewer::getAtomPositions() const
{
    QVector<QVector3D> positions;
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) {
        for (const Atom& atom : m_trajectoryAtoms[m_currentFrame]) {
            positions.append(atom.position);
        }
    }
    return positions;
}

QVector<QString> MoleculeViewer::getAtomElements() const
{
    QVector<QString> elements;
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) {
        for (const Atom& atom : m_trajectoryAtoms[m_currentFrame]) {
            elements.append(atom.element);
        }
    }
    return elements;
}

QVector<float> MoleculeViewer::getAtomCharges() const
{
    QVector<float> charges;
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) {
        for (const Atom& atom : m_trajectoryAtoms[m_currentFrame]) {
            charges.append(atom.charge);
        }
    }
    return charges;
}

// Claude Generated - Interactive Simulation Integration
QVector<MoleculeViewer::Atom> MoleculeViewer::getCurrentFrameAtoms() const
{
    if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) {
        return m_trajectoryAtoms[m_currentFrame];
    }
    return QVector<Atom>();
}
