#ifndef BREADCRUMBBAR_H
#define BREADCRUMBBAR_H

#include <QWidget>
#include <QString>
#include <QList>

/**
 * @brief BreadcrumbBar - Clickable path navigation widget
 *
 * Displays current directory path as clickable segments.
 * Example: Home > Projects > Experiment1
 *
 * Emits pathSelected() when user clicks on a path segment,
 * allowing navigation to parent directories.
 */
class BreadcrumbBar : public QWidget {
    Q_OBJECT

public:
    explicit BreadcrumbBar(QWidget* parent = nullptr);

    /**
     * @brief Set the current path to display
     * @param path Full filesystem path (e.g., "/home/user/Projects/Experiment1")
     */
    void setPath(const QString& path);

    /**
     * @brief Get the currently displayed path
     */
    QString path() const { return m_currentPath; }

    /**
     * @brief Set home directory for display (shows ~ instead of full path)
     * @param homeDir Home directory path
     */
    void setHomeDirectory(const QString& homeDir) { m_homeDir = homeDir; }

signals:
    /**
     * @brief Emitted when user clicks on a breadcrumb segment
     * @param path The full path of the clicked segment
     */
    void pathSelected(const QString& path);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

    /**
     * @brief Calculate the rectangle for each breadcrumb segment
     */
    void calculateSegments();

private:
    struct Segment {
        QString displayText;  // "Home", "Projects", "Experiment1"
        QString fullPath;     // Complete path up to this segment
        QRect rect;           // Visual bounding box
    };

    QString m_currentPath;
    QString m_homeDir;
    QList<Segment> m_segments;
    int m_hoverSegmentIndex = -1;

    static constexpr int PADDING_X = 5;
    static constexpr int PADDING_Y = 4;
    static constexpr int SEPARATOR_WIDTH = 12;
};

#endif // BREADCRUMBBAR_H
