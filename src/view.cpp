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
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QImage>
#include <QPixmap>

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
    layout->addWidget(m_container);
    layout->setContentsMargins(0, 0, 0, 0);

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
        // Refresh display if we have trajectory data
        if (!m_trajectoryAtoms.isEmpty()) {
            showFrame(m_currentFrame);
        }
    }
}

void MoleculeViewer::setColorScheme(ColorScheme scheme)
{
    if (m_colorScheme != scheme) {
        m_colorScheme = scheme;
        // Refresh display if we have trajectory data
        if (!m_trajectoryAtoms.isEmpty()) {
            showFrame(m_currentFrame);
        }
    }
}

void MoleculeViewer::setAtomTransparency(float alpha)
{
    m_atomTransparency = qBound(0.0f, alpha, 1.0f);
    // Refresh display
    if (!m_trajectoryAtoms.isEmpty()) {
        showFrame(m_currentFrame);
    }
}

void MoleculeViewer::setAtomShininess(float shininess)
{
    m_atomShininess = qMax(0.0f, shininess);
    // Refresh display
    if (!m_trajectoryAtoms.isEmpty()) {
        showFrame(m_currentFrame);
    }
}

void MoleculeViewer::setAtomScaleFactor(float scale)
{
    m_atomScaleFactor = qMax(0.1f, scale);  // Min 0.1x scaling
    // Refresh display
    if (!m_trajectoryAtoms.isEmpty()) {
        showFrame(m_currentFrame);
    }
}

void MoleculeViewer::setBondThickness(float thickness)
{
    m_bondThickness = qMax(0.01f, thickness);  // Min 0.01 radius
    // Refresh display
    if (!m_trajectoryAtoms.isEmpty()) {
        showFrame(m_currentFrame);
    }
}

// viewer.cpp
// Füge diese Implementierungen hinzu:

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

    // If we have multiple frames stored (trajectory), show current frame
    if (m_trajectoryAtoms.size() > 1 && m_currentFrame >= 0 && m_currentFrame < m_trajectoryAtoms.size()) {
        Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(m_trajectoryAtoms[m_currentFrame], m_trajectoryBonds[m_currentFrame]);
        moleculeEntity->setParent(m_rootEntity);
    } else {
        // Single frame or first frame
        Qt3DCore::QEntity* moleculeEntity = createMoleculeEntity(atoms, actualBonds);
        moleculeEntity->setParent(m_rootEntity);
    }

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
