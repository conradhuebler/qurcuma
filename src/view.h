// viewer.h
#ifndef MOLECULEVIEWER_H
#define MOLECULEVIEWER_H

#include <QWidget>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <QVector3D>
#include <Qt3DExtras/QOrbitCameraController>

class MoleculeViewer : public QWidget
{
    Q_OBJECT

public:
    struct Atom {
        QVector3D position;
        QString element;
    };

    struct Bond {
        int atom1;
        int atom2;
        int bondOrder;
    };

    explicit MoleculeViewer(QWidget *parent = nullptr);
    ~MoleculeViewer();

    void addMolecule(const QVector<Atom>& atoms, const QVector<Bond>& bonds);
    void addMolecule(const QVector<Atom>& atoms) { addMolecule(atoms, {}); }

public slots:
    void resetCamera();
    void resetView();
private:
    Qt3DExtras::Qt3DWindow *m_view;
    QWidget *m_container;
    Qt3DCore::QEntity *m_rootEntity;
    Qt3DRender::QCamera *m_camera;
    Qt3DExtras::QOrbitCameraController *m_cameraController;

    void setupViewer();
    void clearScene();
    Qt3DCore::QEntity* createMoleculeEntity(const QVector<Atom>& atoms, const QVector<Bond>& bonds);
    static QColor getAtomColor(const QString& element);
    static float getAtomRadius(const QString& element);

    static const float DEFAULT_BOND_DISTANCE; // Maximaler Abstand für automatische Bindungserkennung
    QVector<Bond> detectBonds(const QVector<Atom>& atoms);
    void setDefaultView();
    QVector3D m_moleculeCenter;
    float m_moleculeRadius;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // MOLECULEVIEWER_H
