// viewer.cpp
#include "view.h"
#include "selectionmanager.h"  // Claude Generated - Phase 2A
#include "measurementoverlay.h"  // Claude Generated - Phase 2B
#include "bondeditor.h"  // Claude Generated - Phase 4B
#include "pbrmaterial.h"  // Claude Generated - Phase 4A
#include "xyzparser.h"  // Claude Generated - Phase 4B - For auto-save XYZ writing
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DRender/QCamera>
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
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QTimer>
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
    m_frameGraph = new CustomFrameGraph();
    m_frameGraph->initialize(m_container->size(), m_camera, m_rootEntity);
    m_view->setActiveFrameGraph(m_frameGraph);

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

void MoleculeViewer::handleMouseRotation(const QPoint& currentPos)
{
    // Claude Generated - Arcball rotation around view center
    if (!m_camera) return;

    QPoint delta = currentPos - m_lastMousePos;

    // Sensitivity parameters
    const float rotationSensitivity = 0.5f; // degrees per pixel

    // Get current camera state
    QVector3D viewCenter = m_camera->viewCenter();
    QVector3D camPos = m_camera->position();
    QVector3D upVector = m_camera->upVector();

    // Calculate vector from view center to camera
    QVector3D radiusVector = camPos - viewCenter;
    float distance = radiusVector.length();

    if (distance < 0.01f) return; // Prevent division by zero

    // Normalize radius vector
    QVector3D radialDir = radiusVector.normalized();

    // Calculate rotation axes
    // Vertical rotation: around the "right" vector (perpendicular to view direction and up vector)
    QVector3D rightVector = QVector3D::crossProduct(radialDir, upVector).normalized();

    // Horizontal rotation: around the up vector
    QVector3D actualUpVector = QVector3D::crossProduct(rightVector, radialDir).normalized();

    // Apply rotations using quaternions
    QQuaternion verticalRotation = QQuaternion::fromAxisAndAngle(rightVector, -delta.y() * rotationSensitivity);
    QQuaternion horizontalRotation = QQuaternion::fromAxisAndAngle(actualUpVector, delta.x() * rotationSensitivity);

    // Combined rotation
    QQuaternion totalRotation = horizontalRotation * verticalRotation;

    // Rotate the radius vector
    QVector3D newRadiusVector = totalRotation.rotatedVector(radiusVector);

    // Calculate new camera position
    QVector3D newCamPos = viewCenter + newRadiusVector;

    // Update camera position and up vector
    m_camera->setPosition(newCamPos);

    // Rotate up vector
    QVector3D newUpVector = totalRotation.rotatedVector(upVector);
    m_camera->setUpVector(newUpVector.normalized());
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
}

void MoleculeViewer::clearScene()
{
    // Claude Generated - Phase 2A: Clear picker mappings
    m_atomPickerToIndex.clear();

    // Alle Entities löschen
    for (Qt3DCore::QNode *node : m_rootEntity->childNodes()) {
        delete node;
    }
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

Qt3DCore::QEntity* MoleculeViewer::createMoleculeEntity(const QVector<Atom>& atoms, const QVector<Bond>& bonds)
{
    Qt3DCore::QEntity *moleculeEntity = new Qt3DCore::QEntity();

    // Claude Generated - Fix 2: Clear entity/material references for incremental updates
    m_atomEntities.clear();
    m_bondEntities.clear();
    m_atomMaterials.clear();

    // Claude Generated - Render atoms based on rendering mode
    bool renderAtoms = (m_renderingMode == RenderingMode::BallAndStick ||
                        m_renderingMode == RenderingMode::SpaceFilling);

    if (renderAtoms) {
        for (int atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
            const Atom& atom = atoms[atomIndex];
            Qt3DCore::QEntity *atomEntity = new Qt3DCore::QEntity(moleculeEntity);

            // Mesh
            Qt3DExtras::QSphereMesh *sphereMesh = new Qt3DExtras::QSphereMesh();
            sphereMesh->setRadius(getAtomRadius(atom.element));
            sphereMesh->setRings(32);    // High quality spheres
            sphereMesh->setSlices(32);

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

            // Claude Generated - Phase 2A: Add ObjectPicker for 3D atom selection
            Qt3DRender::QObjectPicker *picker = new Qt3DRender::QObjectPicker(atomEntity);
            picker->setHoverEnabled(true);
            atomEntity->addComponent(picker);

            // Store mapping for picking event handling
            m_atomPickerToIndex[picker] = atomIndex;

            // Connect picker signals
            connect(picker, &Qt3DRender::QObjectPicker::clicked, this, &MoleculeViewer::onAtomPicked);

            // Claude Generated - Fix 2: Store references for incremental updates
            m_atomEntities.append(atomEntity);
            m_atomMaterials.append(material);
        }
    }

    // Claude Generated - Render bonds based on rendering mode
    bool renderBonds = (m_renderingMode != RenderingMode::SpaceFilling);

    if (renderBonds) {
        // Determine bond thickness based on mode
        float bondRadius = m_bondThickness;
        if (m_renderingMode == RenderingMode::Wireframe) {
            bondRadius *= 0.5f;  // Thinner for wireframe
        } else if (m_renderingMode == RenderingMode::SticksOnly) {
            bondRadius *= 1.5f;  // Thicker for sticks-only
        }

        for (const Bond& bond : bonds) {
            Qt3DCore::QEntity *bondEntity = new Qt3DCore::QEntity(moleculeEntity);

            // Mesh
            Qt3DExtras::QCylinderMesh *cylinderMesh = new Qt3DExtras::QCylinderMesh();
            cylinderMesh->setRadius(bondRadius);
            cylinderMesh->setRings(16);      // Good quality
            cylinderMesh->setSlices(16);

            // Material
            Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
            material->setAmbient(QColor(180, 180, 180));
            material->setDiffuse(QColor(200, 200, 200));
            material->setShininess(m_atomShininess);
        
        // Transform
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
        
        // Position und Rotation berechnen
        QVector3D start = atoms[bond.atom1].position;
        QVector3D end = atoms[bond.atom2].position;
        QVector3D direction = end - start;
        float length = direction.length();
        
        transform->setTranslation(start + direction * 0.5f);
        transform->setScale3D(QVector3D(1.0f, length, 1.0f));
        
        // Rotation zur Bindungsachse
        QVector3D localUp(0, 1, 0);
        QVector3D normalizedDirection = direction.normalized();
        QVector3D rotationAxis = QVector3D::crossProduct(localUp, normalizedDirection);
        
        if (rotationAxis.length() < 0.001f) {
            // Wenn die Richtung parallel zur Y-Achse ist
            if (normalizedDirection.y() > 0) {
                transform->setRotation(QQuaternion());  // Keine Rotation nötig
            } else {
                transform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f));
            }
        } else {
            float rotationAngle = qAcos(QVector3D::dotProduct(localUp, normalizedDirection)) * 180.0f / M_PI;
            transform->setRotation(QQuaternion::fromAxisAndAngle(rotationAxis.normalized(), rotationAngle));
        }
        
        bondEntity->addComponent(cylinderMesh);
        bondEntity->addComponent(transform);
        bondEntity->addComponent(material);

        // Claude Generated - Phase 4B: Add ObjectPicker for bond selection
        int bondIndex = m_bondEntities.size();  // Current bond index
        Qt3DRender::QObjectPicker *bondPicker = new Qt3DRender::QObjectPicker(bondEntity);
        bondPicker->setHoverEnabled(true);
        bondEntity->addComponent(bondPicker);

        // Store mapping for picking event handling
        m_bondPickerToIndex[bondPicker] = bondIndex;

        // Connect picker signals
        connect(bondPicker, &Qt3DRender::QObjectPicker::clicked, this, &MoleculeViewer::onBondPicked);

        // Claude Generated - Fix 2: Store bond reference for incremental updates
        m_bondEntities.append(bondEntity);

            // Für Mehrfachbindungen
            if (bond.bondOrder > 1) {
                // Offset für zusätzliche Bindungen
                float offset = 0.2f;
                for (int i = 1; i < bond.bondOrder; ++i) {
                    Qt3DCore::QEntity *additionalBondEntity = new Qt3DCore::QEntity(moleculeEntity);

                    Qt3DExtras::QCylinderMesh *addCylinderMesh = new Qt3DExtras::QCylinderMesh();
                    addCylinderMesh->setRadius(bondRadius);  // Claude Generated - use variable bond radius
                    addCylinderMesh->setRings(16);
                    addCylinderMesh->setSlices(16);
            
            Qt3DCore::QTransform *addTransform = new Qt3DCore::QTransform();
            
            // Position und Rotation neu berechnen
            QVector3D start = atoms[bond.atom1].position;
            QVector3D end = atoms[bond.atom2].position;
            QVector3D direction = end - start;
            float length = direction.length();
            
            // Berechne Offset-Richtung senkrecht zur Bindung
            QVector3D offsetDir = QVector3D::crossProduct(direction.normalized(), QVector3D(0, 0, 1)).normalized();
            if (offsetDir.length() < 0.001f) {
                offsetDir = QVector3D::crossProduct(direction.normalized(), QVector3D(0, 1, 0)).normalized();
            }
            
            // Alterniere die Seite für gerade/ungerade Bindungen
            float currentOffset = (i % 2 == 0 ? 1 : -1) * offset * ((i + 1) / 2);
            QVector3D offsetStart = start + offsetDir * currentOffset;
            QVector3D offsetEnd = end + offsetDir * currentOffset;
            QVector3D offsetCenter = (offsetStart + offsetEnd) * 0.5f;
            
            addTransform->setTranslation(offsetCenter);
            addTransform->setScale3D(QVector3D(1.0f, length, 1.0f));
            
            // Rotation zur Bindungsachse
            QVector3D localUp(0, 1, 0);
            QVector3D normalizedDirection = direction.normalized();
            QVector3D rotationAxis = QVector3D::crossProduct(localUp, normalizedDirection);
            
            if (rotationAxis.length() < 0.001f) {
                if (normalizedDirection.y() > 0) {
                    addTransform->setRotation(QQuaternion());
                } else {
                    addTransform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f));
                }
            } else {
                float rotationAngle = qAcos(QVector3D::dotProduct(localUp, normalizedDirection)) * 180.0f / M_PI;
                addTransform->setRotation(QQuaternion::fromAxisAndAngle(rotationAxis.normalized(), rotationAngle));
            }
            
                    additionalBondEntity->addComponent(addCylinderMesh);
                    additionalBondEntity->addComponent(addTransform);
                    additionalBondEntity->addComponent(material);

                    // Claude Generated - Fix 2: Store additional bond reference
                    m_bondEntities.append(additionalBondEntity);
                }
            }
        }
    }
    return moleculeEntity;
}

void MoleculeViewer::setDefaultView()
{
    if (!m_camera) return;
    
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
    
    // Berechne Bindungen basierend auf Atomabständen
    for (int i = 0; i < atoms.size(); ++i) {
        for (int j = i + 1; j < atoms.size(); ++j) {
            QVector3D diff = atoms[i].position - atoms[j].position;
            float distance = diff.length();
            float covalentRadiusSum = getAtomRadius(atoms[i].element) + getAtomRadius(atoms[j].element);
            // Prüfe ob Atome nah genug für eine Bindung sind
            // Hier könnte man auch die Van-der-Waals-Radien der spezifischen Elemente berücksichtigen
            if (distance <= covalentRadiusSum * DEFAULT_BOND_DISTANCE * 0.5) {
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

    // Render the molecule using trajectory data
    Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(m_trajectoryAtoms[0], m_trajectoryBonds[0]);
    moleculeEntity->setParent(m_rootEntity);

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
        moleculeEntity->setParent(m_rootEntity);

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
            // Convert atom positions to QVector for measurement overlay
            QVector<QVector3D> positions;
            for (const Atom& atom : m_trajectoryAtoms[frameIndex]) {
                positions.append(atom.position);
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

    // Update atom positions only (no scene rebuild, no camera reset)
    for (int i = 0; i < m_atomEntities.size() && i < atoms.size(); ++i) {
        Qt3DCore::QEntity *atomEntity = m_atomEntities[i];
        if (!atomEntity) continue;

        // Find and update the transform component
        auto transforms = atomEntity->componentsOfType<Qt3DCore::QTransform>();
        for (auto transform : transforms) {
            transform->setTranslation(atoms[i].position);
        }
    }

    // Update bond positions too
    int bondIndex = 0;
    for (int i = 0; i < m_trajectoryBonds[frameIndex].size() && bondIndex < m_bondEntities.size(); ++i) {
        const Bond& bond = m_trajectoryBonds[frameIndex][i];
        Qt3DCore::QEntity *bondEntity = m_bondEntities[bondIndex];
        if (!bondEntity) {
            bondIndex++;
            continue;
        }

        // Update bond position and scale
        QVector3D start = atoms[bond.atom1].position;
        QVector3D end = atoms[bond.atom2].position;
        QVector3D direction = end - start;
        float length = direction.length();

        auto transforms = bondEntity->componentsOfType<Qt3DCore::QTransform>();
        for (auto transform : transforms) {
            transform->setTranslation(start + direction * 0.5f);
            transform->setScale3D(QVector3D(1.0f, length, 1.0f));
        }

        bondIndex++;
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
            positions.append(atom.position);
        }
        m_measurementOverlay->updateMeasurement(positions);
    }

    emit frameChanged(m_currentFrame);
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
    m_trajectoryBonds = bonds;
    m_frameCount = atoms.size();
    m_currentFrame = 0;

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
}

void MoleculeViewer::showTrajectoryFrame(int frameIndex)
{
    if (frameIndex >= 0 && frameIndex < m_trajectoryAtoms.size()) {
        m_currentFrame = frameIndex;
        clearScene();
        
        // Use stored trajectory data
        Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(m_trajectoryAtoms[frameIndex], m_trajectoryBonds[frameIndex]);
        moleculeEntity->setParent(m_rootEntity);
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
    moleculeEntity->setParent(m_rootEntity);

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
    QVector3D direction = m_camera->position() - atom.position;
    if (direction.length() < 0.001f) {
        direction = QVector3D(0, 0, 1);  // Default if too close
    }
    direction.normalize();

    float distance = 5.0f;  // Default viewing distance
    m_camera->setPosition(atom.position + direction * distance);
    m_camera->setViewCenter(atom.position);
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

    // Position camera to view selection
    QVector3D direction = m_camera->position() - center;
    if (direction.length() < 0.001f) {
        direction = QVector3D(0, 0, 1);
    }
    direction.normalize();

    // Calculate distance to frame the sphere in view
    // For 45 degree FOV, distance = radius / tan(22.5 degrees)
    float distance = radius / 0.4142f;  // ~tan(22.5°)
    if (distance < 1.0f) distance = 1.0f;

    m_camera->setPosition(center + direction * distance);
    m_camera->setViewCenter(center);
}

void MoleculeViewer::fitAllInView()
{
    if (m_trajectoryAtoms.isEmpty() || m_currentFrame >= m_trajectoryAtoms.size()) return;

    QVector3D center = m_moleculeCenter;
    float radius = m_moleculeRadius;

    // Position camera to view entire molecule
    QVector3D direction = m_camera->position() - center;
    if (direction.length() < 0.001f) {
        direction = QVector3D(0, 0, 1);
    }
    direction.normalize();

    // Calculate distance to frame the entire molecule
    float distance = radius / 0.4142f;  // ~tan(22.5°) for 45 degree FOV
    if (distance < 1.0f) distance = 5.0f;

    m_camera->setPosition(center + direction * distance);
    m_camera->setViewCenter(center);
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

            // Update with current frame positions
            if (m_currentFrame < m_trajectoryAtoms.size()) {
                QVector<QVector3D> positions;
                for (const Atom& atom : m_trajectoryAtoms[m_currentFrame]) {
                    positions.append(atom.position);
                }
                m_measurementOverlay->updateMeasurement(positions);
            }
        }
    }

    // Emit signal for downstream UI updates
    emit selectionChanged(m_selectionManager->selectedAtoms());
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
