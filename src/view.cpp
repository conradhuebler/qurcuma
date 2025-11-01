// viewer.cpp
#include "view.h"
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DRender/QCamera>
#include <Qt3DExtras/QOrbitCameraController>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
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
    setupViewer();
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

// Claude Generated - Fix 2: Incremental update methods for smooth rendering
void MoleculeViewer::updateMaterials()
{
    // Update only the material properties of existing atoms (colors, transparency, shininess)
    // This is much faster than full scene rebuild
    for (int i = 0; i < m_atomMaterials.size(); ++i) {
        Qt3DExtras::QPhongMaterial *material = m_atomMaterials[i];
        if (!material) continue;

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

        // Update material colors
        material->setAmbient(atomColor.darker());
        material->setDiffuse(atomColor);
        material->setShininess(m_atomShininess);
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

    // Claude Generated - Render atoms based on rendering mode
    bool renderAtoms = (m_renderingMode == RenderingMode::BallAndStick ||
                        m_renderingMode == RenderingMode::SpaceFilling);

    if (renderAtoms) {
        for (const Atom& atom : atoms) {
            Qt3DCore::QEntity *atomEntity = new Qt3DCore::QEntity(moleculeEntity);

            // Mesh
            Qt3DExtras::QSphereMesh *sphereMesh = new Qt3DExtras::QSphereMesh();
            sphereMesh->setRadius(getAtomRadius(atom.element));
            sphereMesh->setRings(32);    // High quality spheres
            sphereMesh->setSlices(32);

            // Material with transparency support
            Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
            QColor atomColor = getAtomColor(atom.element, atom.charge);

            // Apply transparency
            if (m_atomTransparency < 1.0f) {
                atomColor.setAlphaF(m_atomTransparency);
            }

            material->setAmbient(atomColor.darker());
            material->setDiffuse(atomColor);
            material->setShininess(m_atomShininess);

            // Transform
            Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
            transform->setTranslation(atom.position);

            atomEntity->addComponent(sphereMesh);
            atomEntity->addComponent(transform);
            atomEntity->addComponent(material);
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
        
        emit frameChanged(m_currentFrame);
    }
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
    
    // Emit signals for UI updates
    emit trajectoryLoaded(m_frameCount);
    emit frameChanged(m_currentFrame);
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

// Claude Generated - Trajectory animation functions
void MoleculeViewer::startAnimation()
{
    if (m_trajectoryAtoms.size() <= 1) {
        qWarning() << "Cannot start animation: no trajectory data";
        return;
    }

    if (m_isAnimating) {
        return;  // Already animating
    }

    m_isAnimating = true;

    if (!m_animationTimer) {
        m_animationTimer = new QTimer(this);
        connect(m_animationTimer, &QTimer::timeout, this, &MoleculeViewer::onAnimationTick);
    }

    // Set interval based on FPS (1000ms / FPS = interval in ms)
    int intervalMs = qMax(1, 1000 / m_animationFPS);
    m_animationTimer->start(intervalMs);
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

    showFrame(nextFrame);
}

// Claude Generated - Setup integrated control panel
void MoleculeViewer::setupControlPanel()
{
    m_controlPanel = new QWidget;
    m_controlPanel->setMaximumHeight(45);

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
    panelLayout->setSpacing(8);

    // Mode selector
    QComboBox *modeCombo = new QComboBox;
    modeCombo->addItem(tr("Ball & Stick"), static_cast<int>(RenderingMode::BallAndStick));
    modeCombo->addItem(tr("Space-Filling"), static_cast<int>(RenderingMode::SpaceFilling));
    modeCombo->addItem(tr("Wireframe"), static_cast<int>(RenderingMode::Wireframe));
    modeCombo->addItem(tr("Sticks"), static_cast<int>(RenderingMode::SticksOnly));
    modeCombo->setMaximumWidth(120);
    // Claude Generated - Fixed: Capture combo pointer instead of using sender()
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, modeCombo](int index) {
        setRenderingMode(static_cast<RenderingMode>(modeCombo->itemData(index).toInt()));
    });
    panelLayout->addWidget(new QLabel(tr("Render:")));
    panelLayout->addWidget(modeCombo);

    // Color scheme selector
    QComboBox *colorCombo = new QComboBox;
    colorCombo->addItem(tr("CPK"), static_cast<int>(ColorScheme::CPK));
    colorCombo->addItem(tr("Monochrome"), static_cast<int>(ColorScheme::Monochrome));
    colorCombo->addItem(tr("By Charge"), static_cast<int>(ColorScheme::ByCharge));
    // Claude Generated - Removed "Custom" color scheme (not implemented)
    colorCombo->setMaximumWidth(100);
    // Claude Generated - Fixed: Capture combo pointer instead of using sender()
    connect(colorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, colorCombo](int index) {
        setColorScheme(static_cast<ColorScheme>(colorCombo->itemData(index).toInt()));
    });
    panelLayout->addWidget(new QLabel(tr("Color:")));
    panelLayout->addWidget(colorCombo);

    panelLayout->addSpacing(10);

    // Transparency slider
    QSlider *transSlider = new QSlider(Qt::Horizontal);
    transSlider->setRange(0, 100);
    transSlider->setValue(100);
    transSlider->setMaximumWidth(80);
    connect(transSlider, &QSlider::valueChanged, [this](int value) {
        setAtomTransparency(value / 100.0f);
    });
    panelLayout->addWidget(new QLabel(tr("Alpha:")));
    panelLayout->addWidget(transSlider);

    // Claude Generated - Shininess spinbox (was applied but had no UI control)
    QSpinBox *shineSpinBox = new QSpinBox;
    shineSpinBox->setRange(0, 200);
    shineSpinBox->setValue(80);
    shineSpinBox->setMaximumWidth(60);
    connect(shineSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        setAtomShininess(static_cast<float>(value));
    });
    panelLayout->addWidget(new QLabel(tr("Shine:")));
    panelLayout->addWidget(shineSpinBox);

    panelLayout->addSpacing(10);

    // Atom size slider
    QSlider *sizeSlider = new QSlider(Qt::Horizontal);
    sizeSlider->setRange(10, 300);
    sizeSlider->setValue(100);
    sizeSlider->setMaximumWidth(80);
    connect(sizeSlider, &QSlider::valueChanged, [this](int value) {
        setAtomScaleFactor(value / 100.0f);
    });
    panelLayout->addWidget(new QLabel(tr("Size:")));
    panelLayout->addWidget(sizeSlider);

    // Claude Generated - Bond thickness slider (was applied but had no UI control)
    QSlider *bondSlider = new QSlider(Qt::Horizontal);
    bondSlider->setRange(5, 50);  // 0.05 to 0.50 (× 100)
    bondSlider->setValue(15);     // 0.15 default
    bondSlider->setMaximumWidth(80);
    connect(bondSlider, &QSlider::valueChanged, [this](int value) {
        setBondThickness(value / 100.0f);
    });
    panelLayout->addWidget(new QLabel(tr("Bond:")));
    panelLayout->addWidget(bondSlider);

    panelLayout->addSpacing(10);

    // Animation controls
    QPushButton *playButton = new QPushButton;
    playButton->setIcon(QIcon::fromTheme("media-playback-start"));
    playButton->setToolTip(tr("Play Animation"));
    playButton->setMaximumWidth(30);
    connect(playButton, &QPushButton::clicked, this, &MoleculeViewer::startAnimation);
    panelLayout->addWidget(playButton);

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
    panelLayout->addWidget(new QLabel(tr("FPS:")));
    panelLayout->addWidget(fpsSpinBox);

    // Loop checkbox
    QCheckBox *loopCheckbox = new QCheckBox(tr("Loop"));
    loopCheckbox->setChecked(true);
    connect(loopCheckbox, &QCheckBox::toggled, this, &MoleculeViewer::setAnimationLoop);
    panelLayout->addWidget(loopCheckbox);

    panelLayout->addSpacing(10);

    // Measurement selector
    QComboBox *measureCombo = new QComboBox;
    measureCombo->addItem(tr("No Measurement"), 0);
    measureCombo->addItem(tr("Distance"), 1);
    measureCombo->addItem(tr("Angle"), 2);
    measureCombo->setMaximumWidth(120);
    // Claude Generated - Fixed: Capture combo pointer instead of using sender()
    connect(measureCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, measureCombo](int index) {
        setMeasurementMode(measureCombo->itemData(index).toInt());
    });
    panelLayout->addWidget(new QLabel(tr("Measure:")));
    panelLayout->addWidget(measureCombo);

    // Atom index spinboxes for selection
    QSpinBox *atom1Box = new QSpinBox;
    atom1Box->setRange(0, 9999);
    atom1Box->setMaximumWidth(50);
    connect(atom1Box, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        while (m_selectedAtoms.size() < 1) m_selectedAtoms.append(0);
        m_selectedAtoms[0] = value;
        updateMeasurementDisplay();
    });
    panelLayout->addWidget(new QLabel(tr("Atom1:")));
    panelLayout->addWidget(atom1Box);

    QSpinBox *atom2Box = new QSpinBox;
    atom2Box->setRange(0, 9999);
    atom2Box->setMaximumWidth(50);
    connect(atom2Box, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        while (m_selectedAtoms.size() < 2) m_selectedAtoms.append(0);
        m_selectedAtoms[1] = value;
        updateMeasurementDisplay();
    });
    panelLayout->addWidget(new QLabel(tr("Atom2:")));
    panelLayout->addWidget(atom2Box);

    QSpinBox *atom3Box = new QSpinBox;
    atom3Box->setRange(0, 9999);
    atom3Box->setMaximumWidth(50);
    connect(atom3Box, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        while (m_selectedAtoms.size() < 3) m_selectedAtoms.append(0);
        m_selectedAtoms[2] = value;
        updateMeasurementDisplay();
    });
    panelLayout->addWidget(new QLabel(tr("Atom3:")));
    panelLayout->addWidget(atom3Box);

    // Claude Generated - Fog controls removed (requires custom shaders, not yet implemented)
    // TODO: Implement fog with Qt3D custom shaders in future

    panelLayout->addStretch();
}

// Claude Generated - Selection and measurement functions
void MoleculeViewer::clearSelection()
{
    m_selectedAtoms.clear();
    m_measurementMode = 0;
}

void MoleculeViewer::setMeasurementMode(int mode)
{
    m_measurementMode = qBound(0, mode, 2);  // Clamp 0-2
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
