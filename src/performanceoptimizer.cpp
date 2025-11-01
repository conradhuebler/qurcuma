// Claude Generated - Phase 3B - Performance Optimizer Implementation
#include "performanceoptimizer.h"
#include <QDebug>
#include <QDateTime>

PerformanceOptimizer::PerformanceOptimizer(QObject *parent)
    : QObject(parent)
    , m_qualityMode(Balanced)
{
    // Setup FPS monitoring timer
    m_fpsTimer = new QTimer(this);
    connect(m_fpsTimer, &QTimer::timeout, this, [this]() {
        if (m_framesSinceLastUpdate > 0) {
            // Calculate FPS (assuming 1 second timer)
            m_averageFPS = m_averageFPS * 0.7f + m_framesSinceLastUpdate * 0.3f;
            emit fpsChanged(m_averageFPS);
            m_framesSinceLastUpdate = 0;
        }
    });
    m_fpsTimer->setInterval(1000);  // Update every second
}

PerformanceOptimizer::~PerformanceOptimizer()
{
    stopMonitoring();
}

void PerformanceOptimizer::setQualityMode(QualityMode mode)
{
    if (m_qualityMode != mode) {
        m_qualityMode = mode;

        QString modeName;
        switch (mode) {
            case Fast: modeName = "Fast (LOD reduced)"; break;
            case Balanced: modeName = "Balanced"; break;
            case HighQuality: modeName = "High Quality"; break;
        }

        emit qualityModeChanged(mode);
        emit optimizationStatusChanged(QString("Quality mode: %1").arg(modeName));
    }
}

int PerformanceOptimizer::getSphereRings() const
{
    switch (m_qualityMode) {
        case Fast: return 8;
        case Balanced: return 16;
        case HighQuality: return 32;
    }
    return 16;
}

int PerformanceOptimizer::getSphereSlices() const
{
    switch (m_qualityMode) {
        case Fast: return 8;
        case Balanced: return 16;
        case HighQuality: return 32;
    }
    return 16;
}

int PerformanceOptimizer::getBondSlices() const
{
    switch (m_qualityMode) {
        case Fast: return 8;
        case Balanced: return 16;
        case HighQuality: return 16;
    }
    return 16;
}

void PerformanceOptimizer::setFrustumCullingEnabled(bool enabled)
{
    if (m_frustumCullingEnabled != enabled) {
        m_frustumCullingEnabled = enabled;
        emit optimizationStatusChanged(
            QString("Frustum culling: %1").arg(enabled ? "enabled" : "disabled")
        );
    }
}

void PerformanceOptimizer::setAdaptiveQualityEnabled(bool enabled)
{
    if (m_adaptiveQualityEnabled != enabled) {
        m_adaptiveQualityEnabled = enabled;
        if (enabled) {
            updateAdaptiveQuality();
        }
        emit optimizationStatusChanged(
            QString("Adaptive quality: %1").arg(enabled ? "enabled" : "disabled")
        );
    }
}

void PerformanceOptimizer::startMonitoring()
{
    if (!m_fpsTimer->isActive()) {
        m_fpsTimer->start();
        m_framesSinceLastUpdate = 0;
        m_averageFPS = 0.0f;
        emit optimizationStatusChanged("Performance monitoring: started");
    }
}

void PerformanceOptimizer::stopMonitoring()
{
    if (m_fpsTimer->isActive()) {
        m_fpsTimer->stop();
        emit optimizationStatusChanged("Performance monitoring: stopped");
    }
}

void PerformanceOptimizer::recordFrame()
{
    m_framesSinceLastUpdate++;
    m_frameCount++;

    // Adaptive quality adjustment
    if (m_adaptiveQualityEnabled && m_frameCount % 60 == 0) {
        updateAdaptiveQuality();
    }
}

void PerformanceOptimizer::setAtomCount(int count)
{
    if (m_atomCount != count) {
        m_atomCount = count;

        if (m_adaptiveQualityEnabled) {
            updateAdaptiveQuality();
        }
    }
}

void PerformanceOptimizer::updateAdaptiveQuality()
{
    // Claude Generated - Phase 3B: Adaptive quality based on atom count
    QualityMode recommendedMode = recommendQualityMode(m_atomCount);

    if (recommendedMode != m_qualityMode) {
        QString warning = getPerformanceWarning(m_atomCount);
        if (!warning.isEmpty()) {
            qDebug() << "Performance warning:" << warning;
            emit optimizationStatusChanged(warning);
        }

        setQualityMode(recommendedMode);
    }
}

QString PerformanceOptimizer::getPerformanceWarning(int atomCount) const
{
    if (atomCount > 5000) {
        return "Large molecule detected (>5000 atoms). Performance may be reduced. Consider using Fast mode.";
    } else if (atomCount > 2000) {
        return "Medium-large molecule (>2000 atoms). Performance optimizations recommended.";
    } else if (atomCount > 1000) {
        return "Large molecule (>1000 atoms). LOD enabled for better performance.";
    }

    return "";
}

PerformanceOptimizer::QualityMode PerformanceOptimizer::recommendQualityMode(int atomCount) const
{
    // Claude Generated - Phase 3B: Auto-select quality based on atom count
    if (atomCount > 5000) {
        return Fast;  // <30 rings/slices
    } else if (atomCount > 2000) {
        return Fast;
    } else if (atomCount > 1000) {
        return Balanced;  // 16 rings/slices
    }

    return HighQuality;  // 32 rings/slices
}
