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

const float MoleculeViewer::DEFAULT_BOND_DISTANCE = 3.0f; // Å

MoleculeViewer::MoleculeViewer(QWidget *parent)
    : QWidget(parent)
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
    m_camera->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    m_camera->setPosition(QVector3D(0, 0, 40.0f));
    m_camera->setViewCenter(QVector3D(0, 0, 0));

    // Kamera-Controller
    m_cameraController = nullptr;

    m_view->setRootEntity(m_rootEntity);

    m_view->installEventFilter(this);
}

bool MoleculeViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::MiddleButton) {
                resetView();
                return true; // Event wurde behandelt
            }
        }
    }
    return QWidget::eventFilter(watched, event);
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
    m_camera->setPosition(QVector3D(0, 0, 40.0f));
    m_camera->setViewCenter(QVector3D(0, 0, 0));
}

// viewer.cpp
// Füge diese Implementierungen hinzu:

QColor MoleculeViewer::getAtomColor(const QString& element)
{
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

    return colors.value(element, QColor(200, 200, 200)); // Standardfarbe für unbekannte Elemente
}

float MoleculeViewer::getAtomRadius(const QString& element)
{
    // Van der Waals Radien in Ångström (skaliert)
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

    return radii.value(element, 0.7f); // Standardradius für unbekannte Elemente
}

Qt3DCore::QEntity* MoleculeViewer::createMoleculeEntity(const QVector<Atom>& atoms, const QVector<Bond>& bonds)
{
    Qt3DCore::QEntity *moleculeEntity = new Qt3DCore::QEntity();

    // Atome erstellen
    for (const Atom& atom : atoms) {
        Qt3DCore::QEntity *atomEntity = new Qt3DCore::QEntity(moleculeEntity);
        
        // Mesh
        Qt3DExtras::QSphereMesh *sphereMesh = new Qt3DExtras::QSphereMesh();
        sphereMesh->setRadius(getAtomRadius(atom.element));
        sphereMesh->setRings(32);    // Erhöhe die Qualität der Kugeln
        sphereMesh->setSlices(32);
        
        // Material
        Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
        QColor atomColor = getAtomColor(atom.element);
        material->setAmbient(atomColor.darker());
        material->setDiffuse(atomColor);
        material->setShininess(80.0f);
        
        // Transform
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
        transform->setTranslation(atom.position);
        
        atomEntity->addComponent(sphereMesh);
        atomEntity->addComponent(transform);
        atomEntity->addComponent(material);
    }

    // Bindungen erstellen
    for (const Bond& bond : bonds) {
        Qt3DCore::QEntity *bondEntity = new Qt3DCore::QEntity(moleculeEntity);
        
        // Mesh
        Qt3DExtras::QCylinderMesh *cylinderMesh = new Qt3DExtras::QCylinderMesh();
        cylinderMesh->setRadius(0.15f);  // Dünnere Bindungen
        cylinderMesh->setRings(16);      // Verbesserte Qualität
        cylinderMesh->setSlices(16);
        
        // Material
        Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
        material->setAmbient(QColor(180, 180, 180));
        material->setDiffuse(QColor(200, 200, 200));
        material->setShininess(80.0f);
        
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
            addCylinderMesh->setRadius(0.15f);
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
    return moleculeEntity;
}

void MoleculeViewer::setDefaultView()
{
    if (!m_camera) return;
    
    // Setze die Kamera auf eine Position, die das gesamte Molekül zeigt
    float distance = m_moleculeRadius * 2.5f; // Faktor für guten Überblick
    m_camera->setPosition(m_moleculeCenter + QVector3D(0.0f, 0.0f, distance));
    m_camera->setViewCenter(m_moleculeCenter);
    m_camera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
}

void MoleculeViewer::resetView()
{
    setDefaultView();
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

    // Berechne Molekülzentrum und Radius
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

    // Verwende die übergebenen Bindungen oder erkenne sie automatisch
    QVector<Bond> actualBonds = bonds.isEmpty() ? detectBonds(atoms) : bonds;

    Qt3DCore::QEntity *moleculeEntity = createMoleculeEntity(atoms, actualBonds);
    
    // Kamera-Controller und Entity-Setup
    m_cameraController = new Qt3DExtras::QOrbitCameraController(moleculeEntity);
    m_cameraController->setLinearSpeed(50.0f);
    m_cameraController->setLookSpeed(180.0f);
    m_cameraController->setCamera(m_camera);
    
    moleculeEntity->setParent(m_rootEntity);

    // Setze die Standardansicht
    setDefaultView();
}
