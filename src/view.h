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
    // Claude Generated - Rendering modes for molecular visualization
    enum class RenderingMode {
        BallAndStick,    // Default: Atoms as spheres, bonds as cylinders
        Wireframe,       // Only bonds (thin cylinders)
        SpaceFilling,    // Van der Waals spheres only
        SticksOnly       // Thin cylinders for bonds, no atom spheres
    };

    // Claude Generated - Color schemes for atom coloring
    enum class ColorScheme {
        CPK,            // Standard CPK colors (element-based)
        Monochrome,     // Single color (uniform gray)
        ByCharge,       // Color by atomic charge (if available)
        Custom          // User-defined colors
    };

    struct Atom {
        QVector3D position;
        QString element;
        float charge = 0.0f;  // Claude Generated - for charge-based coloring
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

    // Claude Generated - Visual settings setters
    void setRenderingMode(RenderingMode mode);
    RenderingMode getRenderingMode() const { return m_renderingMode; }

    void setColorScheme(ColorScheme scheme);
    ColorScheme getColorScheme() const { return m_colorScheme; }

    void setAtomTransparency(float alpha);  // 0.0 (transparent) to 1.0 (opaque)
    float getAtomTransparency() const { return m_atomTransparency; }

    void setAtomShininess(float shininess);
    float getAtomShininess() const { return m_atomShininess; }

    void setAtomScaleFactor(float scale);  // Global atom size multiplier
    float getAtomScaleFactor() const { return m_atomScaleFactor; }

    void setBondThickness(float thickness);
    float getBondThickness() const { return m_bondThickness; }

    // Frame navigation support for trajectories
    void setFrameCount(int frameCount) { m_frameCount = frameCount; }
    int getFrameCount() const { return m_frameCount; }
    void setCurrentFrame(int frameIndex) { m_currentFrame = frameIndex; }
    int getCurrentFrame() const { return m_currentFrame; }
    
    // XYZ trajectory support
    void setTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds);
    void showTrajectoryFrame(int frameIndex);
    
    // VTF trajectory support (existing)
    void setVTFTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds);

public slots:
    void resetCamera();
    void resetView();
    void resetViewToMolecule();  // Reset auf Molekülzentrum
    void showFrame(int frameIndex);  // Show specific frame
    void nextFrame();               // Show next frame
    void previousFrame();           // Show previous frame

    // Claude Generated - Screenshot/Export functionality
    void saveScreenshot(const QString& filename, int scaleFactor = 1);
    void saveScreenshotDialog();

signals:
    void frameChanged(int frameIndex);
    void trajectoryLoaded(int frameCount);

public:
    void clearScenePublic();  // Public wrapper for file loading

private:
    Qt3DExtras::Qt3DWindow *m_view;
    QWidget *m_container;
    Qt3DCore::QEntity *m_rootEntity;
    Qt3DRender::QCamera *m_camera;
    Qt3DExtras::QOrbitCameraController *m_cameraController;

    void setupViewer();
    void clearScene();  // Private implementation
    Qt3DCore::QEntity* createMoleculeEntity(const QVector<Atom>& atoms, const QVector<Bond>& bonds);
    QColor getAtomColor(const QString& element, float charge = 0.0f);  // Claude Generated - changed to non-static for ColorScheme support
    float getAtomRadius(const QString& element);  // Claude Generated - changed to non-static for scaling support

    static const float DEFAULT_BOND_DISTANCE; // Maximaler Abstand für automatische Bindungserkennung
    QVector<Bond> detectBonds(const QVector<Atom>& atoms);
    void setDefaultView();
    QVector3D m_moleculeCenter;
    float m_moleculeRadius;

    // Frame navigation state
    int m_frameCount = 1;
    int m_currentFrame = 0;
    QVector<QVector<Atom>> m_trajectoryAtoms;  // Store all frames
    QVector<QVector<Bond>> m_trajectoryBonds;  // Store all frames

    // Claude Generated - Visual settings state
    RenderingMode m_renderingMode = RenderingMode::BallAndStick;
    ColorScheme m_colorScheme = ColorScheme::CPK;
    float m_atomTransparency = 1.0f;      // Fully opaque by default
    float m_atomShininess = 80.0f;        // Default Phong shininess
    float m_atomScaleFactor = 1.0f;       // Default: no scaling
    float m_bondThickness = 0.15f;        // Default bond radius

    // Mouse interaction - Claude Generated
    bool m_leftMousePressed = false;
    bool m_rightMousePressed = false;
    QPoint m_lastMousePos;

    void handleMouseRotation(const QPoint& currentPos);
    void handleMousePan(const QPoint& currentPos);
    void handleMouseZoom(int delta);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // MOLECULEVIEWER_H