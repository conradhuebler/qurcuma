# API Reference - Key Classes

Quick reference for major Qurcuma classes and their public interfaces.

---

## Rendering & Visualization

### MoleculeViewer (view.cpp/h)

```cpp
class MoleculeViewer : public QWidget {
public:
    enum RenderingMode { BallAndStick, Wireframe, SpaceFilling, SticksOnly };
    enum ColorScheme { CPK, Monochrome, ByCharge, Custom };
    enum MaterialMode { Phong, PBR };

    // Molecule management
    void addMolecule(const QVector<Atom>& atoms, const QVector<Bond>& bonds);
    void clearMolecule();
    void updateFrame(int frameIndex);
    int getCurrentFrame() const;
    int getFrameCount() const;

    // Rendering modes
    void setRenderingMode(RenderingMode mode);
    RenderingMode getRenderingMode() const;

    // Colors & materials
    void setColorScheme(ColorScheme scheme);
    void setMaterialMode(MaterialMode mode);

    // Navigation
    void fitAllInView();
    void centerOnAtom(int atomIndex);
    void zoomToSelection();
    void resetView();

    // Selection
    void selectAtom(int index, bool addToSelection = false);
    void clearSelection();
    QVector<int> getSelectedAtoms() const;

signals:
    void atomSelected(int index);
    void frameChanged(int frameIndex);
};

struct Atom {
    QVector3D position;
    QString element;
    QColor color;
    float radius;
};

struct Bond {
    int atom1, atom2;
};
```

### CustomFrameGraph (customframegraph.cpp/h)

```cpp
class CustomFrameGraph : public Qt3DRender::QFrameGraphNode {
public:
    explicit CustomFrameGraph(Qt3DCore::QNode *parent = nullptr);

    // Setup
    void setCamera(Qt3DRender::QCamera *camera);
    void setViewport(QSize size);

    // Effect parameters
    void setSSAOEnabled(bool enabled);
    void setSSAOIntensity(float intensity);
    void setSSAORadius(float radius);
    void setSSAOBias(float bias);

    void setBloomEnabled(bool enabled);
    void setBloomThreshold(float threshold);
    void setBloomIntensity(float intensity);

    void setToneMappingEnabled(bool enabled);
    void setExposure(float exposure);

signals:
    void renderPassCompleted(const QString& passName);
};
```

### AtomInstancingSystem (atominstancingsystem.cpp/h)

```cpp
class AtomInstancingSystem : public QObject {
    Q_OBJECT

public:
    struct AtomInstance {
        QVector3D position;
        float scale;
        QColor color;
        int atomIndex;
    };

    explicit AtomInstancingSystem(Qt3DCore::QEntity *rootEntity,
                                   QObject *parent = nullptr);

    // Atom management
    void setAtoms(const QVector<QVector3D>& positions,
                  const QVector<QString>& elements,
                  const QVector<QColor>& colors,
                  const QVector<float>& scales);

    // Updates
    void updateAtomPositions(const QVector<QVector3D>& positions);
    void updateAtomColors(const QVector<QColor>& colors);
    void updateAtomScales(const QVector<float>& scales);

    // Picking
    int raycastAtom(const QVector3D& rayOrigin,
                   const QVector3D& rayDirection,
                   const QVector<QVector3D>& atomPositions,
                   float pickingRadius = 0.3f) const;

    // Status
    int getAtomCount() const;
    bool isSupported() const;
    QString getLastError() const;

signals:
    void atomCountChanged(int count);
    void renderingModeChanged(bool isInstancing);
};
```

---

## Performance Optimization

### FrustumCuller (frustumculler.cpp/h)

```cpp
class FrustumCuller {
public:
    struct Bond {
        int atom1, atom2;
    };

    explicit FrustumCuller();

    // Update frustum from camera matrix
    void updateFrustum(const QMatrix4x4& viewProjectionMatrix);

    // Visibility tests
    bool isAtomVisible(const QVector3D& position, float radius = 0.5f) const;
    bool isAABBVisible(const QVector3D& min, const QVector3D& max) const;

    // Bulk culling
    QVector<int> cullAtoms(const QVector<QVector3D>& atomPositions,
                          const QVector<float>& atomScales) const;
    QVector<int> cullBonds(const QVector<Bond>& bonds,
                          const QVector<bool>& visibleAtoms) const;

    int getPlaneCount() const { return 6; }
};
```

### FileLoadingWorker (fileloadingworker.cpp/h)

```cpp
class FileLoadingWorker : public QObject {
    Q_OBJECT

public:
    enum FileType { VTF, XYZ, PDB, MOL2 };

    struct MoleculeData {
        QVector<QVector3D> positions;
        QVector<QString> elements;
        QVector<QColor> colors;
        QVector<float> scales;
        QString filename;
        bool success = false;
        QString errorMessage;
    };

public slots:
    // Load file on worker thread
    void loadFile(const QString& filePath, FileType fileType);

    // Cancel loading
    void cancel();

signals:
    void progress(int percent);              // 0-100%
    void finished(const MoleculeData& data);
    void error(const QString& errorMessage);
};

// Usage example:
FileLoadingWorker *worker = new FileLoadingWorker();
QThread *thread = new QThread();
worker->moveToThread(thread);

connect(thread, &QThread::started, worker, [worker]() {
    worker->loadFile("protein.pdb", FileLoadingWorker::PDB);
});
connect(worker, &FileLoadingWorker::finished, this, &MainWindow::onFileLoaded);
connect(worker, &FileLoadingWorker::progress, progressDialog, &QProgressDialog::setValue);

thread->start();
```

### PerformanceOptimizer (performanceoptimizer.cpp/h)

```cpp
class PerformanceOptimizer : public QObject {
    Q_OBJECT

public:
    enum QualityMode { Fast, Balanced, HighQuality };

    explicit PerformanceOptimizer(QObject *parent = nullptr);

    // Quality control
    void setQualityMode(QualityMode mode);
    QualityMode getQualityMode() const;

    // Sphere geometry
    int getSphereRings() const;    // 8, 16, or 32
    int getSphereSlices() const;

    // Culling
    void setFrustumCullingEnabled(bool enabled);
    bool isFrustumCullingEnabled() const;

    // Adaptive quality
    void setAdaptiveQualityEnabled(bool enabled);
    bool isAdaptiveQualityEnabled() const;

    // Performance monitoring
    void startMonitoring();
    void stopMonitoring();
    float getAverageFPS() const;
    int getFrameCount() const;

    // Recommendations
    QString getPerformanceWarning(int atomCount) const;
    QualityMode recommendQualityMode(int atomCount) const;

signals:
    void qualityModeChanged(int mode);
    void fpsChanged(float fps);
    void optimizationStatusChanged(const QString& status);

public slots:
    void recordFrame();
    void setAtomCount(int count);
};
```

---

## File Parsing

### VTFParser (vtfparser.cpp/h)

```cpp
class VTFParser {
public:
    struct VTFAtom {
        int index;
        QString element;
        float radius;
        QString type;
        QString name;
        float x, y, z;
    };

    struct VTFBond {
        int atom1, atom2;
    };

    struct VTFFrame {
        QVector<VTFAtom> atoms;
        QVector<VTFBond> bonds;
        bool hasUnitCell = false;
        float cellA, cellB, cellC;
    };

    // Parsing
    bool parseFile(const QString& filePath, VTFFrame& frame);
    bool parseTrajectory(const QString& filePath);
    int getFrameCount() const;
    bool getFrame(int frameIndex, VTFFrame& frame) const;

    // Conversion & color
    static void convertToMoleculeViewer(const VTFFrame& vtfFrame,
                                        QVector<Atom>& atoms,
                                        QVector<Bond>& bonds);
    static QColor getAtomColor(const QString& type);
    static float getAtomRadius(float vtfRadius);
};
```

### PDBParser (pdbparser.cpp/h)

```cpp
class PDBParser {
public:
    struct PDBAtom {
        int serialNumber;
        QString name;
        QString resName;
        char chain;
        int resSeq;
        float x, y, z;
        QString element;
    };

    struct PDBFrame {
        int modelNumber = 1;
        QVector<PDBAtom> atoms;
    };

    struct Bond {
        int atom1, atom2;  // Serial numbers (1-based)
    };

    // Parsing
    bool parseFile(const QString& filePath, PDBFrame& frame);
    QString getLastError() const;
    QVector<Bond> getBonds() const;

    static void convertToMoleculeViewer(const PDBFrame& pdbFrame,
                                        QVector<Atom>& atoms,
                                        QVector<Bond>& bonds,
                                        const QVector<Bond>& explicitBonds);
};
```

### MOL2Parser (mol2parser.cpp/h)

```cpp
class MOL2Parser {
public:
    struct MOL2Atom {
        QString name;
        QString type;  // Sybyl type (C.ar, N.3, etc.)
        float x, y, z;
        QString charge;
    };

    struct MOL2Bond {
        int atom1, atom2;
        int bondType;  // 1=single, 2=double, 3=triple, 4=aromatic
    };

    struct MOL2Molecule {
        QString name;
        QString comment;
        QVector<MOL2Atom> atoms;
        QVector<MOL2Bond> bonds;
        QString type;
    };

    // Parsing
    bool parseFile(const QString& filePath, MOL2Molecule& mol);
    QString getLastError() const;

    static void convertToMoleculeViewer(const MOL2Molecule& mol2,
                                        QVector<Atom>& atoms,
                                        QVector<Bond>& bonds);
};
```

### XYZParser (xyzparser.cpp/h)

```cpp
class XYZParser {
public:
    struct XYZAtom {
        QString element;
        float x, y, z;
    };

    struct XYZFrame {
        int atomCount;
        QString comment;
        QVector<XYZAtom> atoms;
    };

    // Parsing
    bool parseTrajectory(const QString& filePath);
    int getFrameCount() const;
    QVector<QVector3D> getPositions(int frameIndex) const;
    QVector<QString> getElements() const;

    // Writing
    bool writeFile(const QString& filePath, const XYZFrame& frame);
    bool writeTrajectory(const QString& filePath,
                        const QVector<XYZFrame>& frames);
};
```

---

## Selection & Measurement

### SelectionManager (selectionmanager.cpp/h)

```cpp
class SelectionManager : public QObject {
    Q_OBJECT

public:
    explicit SelectionManager(QObject *parent = nullptr);

    // Selection management
    void selectAtom(int index, bool addToSelection = false);
    void deselectAtom(int index);
    void toggleAtom(int index);
    void clearSelection();

    // Query
    QVector<int> getSelectedAtoms() const;
    bool isAtomSelected(int index) const;
    int getSelectionCount() const;

signals:
    void selectionChanged(const QVector<int>& selected);
    void atomSelected(int index);
    void atomDeselected(int index);
};
```

### MeasurementOverlay (measurementoverlay.cpp/h)

```cpp
class MeasurementOverlay : public Qt3DCore::QEntity {
    Q_OBJECT

public:
    enum MeasurementMode { None, Distance, Angle, Dihedral };

    explicit MeasurementOverlay(Qt3DCore::QEntity *rootEntity,
                                 QObject *parent = nullptr);

    void setMeasurementMode(MeasurementMode mode);
    void updateMeasurements(const QVector<int>& selectedAtoms,
                           const QVector<QVector3D>& positions);

    // Results
    float getDistance() const;
    float getAngle() const;      // In degrees
    float getDihedral() const;    // In degrees

signals:
    void measurementUpdated();
};
```

---

## Application Core

### MainWindow (mainwindow.cpp/h)

```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // File management
    void openFile(const QString& filePath);
    void switchWorkingDirectory(const QString& dirPath);
    void saveWorkspace(const QString& workspaceName);

    // Visualization
    void setVisualizationSettings(const VisualizationSettings& settings);
    VisualizationSettings getVisualizationSettings() const;

public slots:
    void onFileLoaded(const FileLoadingWorker::MoleculeData& data);
};
```

### Settings (settings.cpp/h)

```cpp
class Settings : public QObject {
    Q_OBJECT

public:
    struct VisualizationSettings {
        // Rendering
        int renderingMode = 0;
        int colorScheme = 0;
        float atomScale = 1.0f;
        float bondThickness = 1.0f;

        // Effects
        bool ssaoEnabled = true;
        float ssaoIntensity = 1.0f;
        float ssaoRadius = 0.1f;
        float ssaoBias = 0.025f;

        bool bloomEnabled = true;
        float bloomThreshold = 1.0f;
        float bloomIntensity = 0.5f;

        bool toneMappingEnabled = true;
        float exposure = 1.0f;

        bool fogEnabled = false;
        float fogIntensity = 1.0f;

        // Phase 5D (TODO)
        // bool gpuInstancingEnabled = true;
        // int instancingThreshold = 1000;
        // bool frustumCullingEnabled = true;
        // bool asyncLoadingEnabled = true;
    };

    explicit Settings(QObject *parent = nullptr);

    // Get/Set
    VisualizationSettings visualizationSettings() const;
    void setVisualizationSettings(const VisualizationSettings& settings);

    QString workingDirectory() const;
    void setWorkingDirectory(const QString& path);

    // Persistence
    void saveSettings();
    void loadSettings();
};
```

---

## Utility Classes

### WorkspaceManager (workspacemanager.cpp/h)

```cpp
class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    struct Workspace {
        QString name;
        QString workingDirectory;
        QByteArray windowGeometry;
        QByteArray splitterState;
        QDateTime created;
        QDateTime lastUsed;
    };

    explicit WorkspaceManager(QObject *parent = nullptr);

    // Workspace operations
    bool saveWorkspace(const Workspace& workspace);
    bool loadWorkspace(const QString& name, Workspace& workspace);
    bool deleteWorkspace(const QString& name);
    QStringList listWorkspaces() const;

signals:
    void workspaceLoaded(const Workspace& workspace);
};
```

### BondEditor (bondeditor.cpp/h)

```cpp
class BondEditor : public Qt3DCore::QEntity {
    Q_OBJECT

public:
    enum Mode { Add, Delete, Cycle };

    explicit BondEditor(Qt3DCore::QEntity *rootEntity,
                       QObject *parent = nullptr);

    void setMode(Mode mode);
    void updateBonds(const QVector<Bond>& bonds);

    // Bond operations
    bool canAddBond(int atom1, int atom2,
                   const QVector<QVector3D>& positions) const;
    bool canRemoveBond(int atom1, int atom2) const;

signals:
    void bondAdded(int atom1, int atom2);
    void bondRemoved(int atom1, int atom2);
};
```

---

## Building & Linking

Include headers in your code:

```cpp
#include "view.h"                        // MoleculeViewer
#include "customframegraph.h"            // CustomFrameGraph
#include "atominstancingsystem.h"        // GPU Instancing
#include "frustumculler.h"               // Frustum Culling
#include "fileloadingworker.h"           // Async Loading
#include "vtfparser.h"                   // VTF Parser
#include "pdbparser.h"                   // PDB Parser
#include "selectionmanager.h"            // Selection
#include "measurementoverlay.h"          // Measurements
#include "settings.h"                    // Settings
```

Link in CMakeLists.txt:
```cmake
target_link_libraries(your_app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::3DCore
    Qt6::3DRender
    Qt6::3DInput
    Qt6::3DExtras
)
```

---

**Last Updated:** November 2025 | **Phase:** 5D
