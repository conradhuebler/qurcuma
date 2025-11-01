// Claude Generated - Phase 3B - Performance Optimizer with LOD and Optimization Controls
#ifndef PERFORMANCEOPTIMIZER_H
#define PERFORMANCEOPTIMIZER_H

#include <QObject>
#include <QString>
#include <QTimer>

/**
 * @brief Performance Optimizer for efficient molecular rendering
 *
 * Implements adaptive optimizations for large molecules:
 * - LOD (Level of Detail): Reduce geometry quality for large systems
 * - Frustum culling: Skip rendering off-screen atoms
 * - Quality presets: Fast/Balanced/High-Quality modes
 * - Performance monitoring: FPS tracking and bottleneck detection
 */
class PerformanceOptimizer : public QObject
{
    Q_OBJECT

public:
    enum QualityMode {
        Fast = 0,         // Low quality, high speed (LOD: 8 rings/slices)
        Balanced = 1,     // Medium quality, medium speed (LOD: 16 rings/slices)
        HighQuality = 2   // High quality, lower speed (LOD: 32 rings/slices)
    };

    explicit PerformanceOptimizer(QObject *parent = nullptr);
    ~PerformanceOptimizer();

    // Quality control
    void setQualityMode(QualityMode mode);
    QualityMode getQualityMode() const { return m_qualityMode; }

    // Sphere mesh quality (rings and slices for sphere generation)
    int getSphereRings() const;
    int getSphereSlices() const;

    // Bond geometry quality
    int getBondSlices() const;

    // Optimization features
    void setFrustumCullingEnabled(bool enabled);
    bool isFrustumCullingEnabled() const { return m_frustumCullingEnabled; }

    void setAdaptiveQualityEnabled(bool enabled);
    bool isAdaptiveQualityEnabled() const { return m_adaptiveQualityEnabled; }

    // Performance monitoring
    void startMonitoring();
    void stopMonitoring();
    float getAverageFPS() const { return m_averageFPS; }
    int getFrameCount() const { return m_frameCount; }
    int getAtomCount() const { return m_atomCount; }

    // Recommendations
    QString getPerformanceWarning(int atomCount) const;
    QualityMode recommendQualityMode(int atomCount) const;

signals:
    void qualityModeChanged(int mode);
    void fpsChanged(float fps);
    void optimizationStatusChanged(const QString& status);

public slots:
    void recordFrame();  // Call once per frame to track FPS
    void setAtomCount(int count);

private:
    void updateAdaptiveQuality();

    QualityMode m_qualityMode = Balanced;
    bool m_frustumCullingEnabled = false;
    bool m_adaptiveQualityEnabled = true;

    // Performance monitoring
    QTimer *m_fpsTimer = nullptr;
    float m_averageFPS = 0.0f;
    int m_frameCount = 0;
    int m_framesSinceLastUpdate = 0;
    int m_atomCount = 0;
};

#endif // PERFORMANCEOPTIMIZER_H
