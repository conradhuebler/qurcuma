#include "breadcrumbbar.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>
#include <QStyleOption>

BreadcrumbBar::BreadcrumbBar(QWidget* parent)
    : QWidget(parent) {
    setStyleSheet(
        "BreadcrumbBar { "
        "    background-color: palette(base); "
        "    border: 1px solid palette(mid); "
        "    border-radius: 4px; "
        "    padding: 2px; "
        "} "
    );
    setMaximumHeight(32);
    setMinimumHeight(28);
    setCursor(Qt::ArrowCursor);
}

void BreadcrumbBar::setPath(const QString& path) {
    if (m_currentPath == path) {
        return;
    }

    m_currentPath = path;
    m_segments.clear();
    calculateSegments();
    update();
}

void BreadcrumbBar::calculateSegments() {
    m_segments.clear();

    if (m_currentPath.isEmpty()) {
        return;
    }

    // Replace home directory with ~
    QString displayPath = m_currentPath;
    if (!m_homeDir.isEmpty() && m_currentPath.startsWith(m_homeDir)) {
        displayPath = "~" + m_currentPath.mid(m_homeDir.length());
    }

    // Split path into segments
    QStringList parts;
    if (displayPath.startsWith("~")) {
        parts << "Home";
        QString remainder = displayPath.mid(1);
        if (!remainder.isEmpty() && remainder != "/") {
            remainder.remove(0, 1);  // Remove leading /
            parts << remainder.split('/');
        }
    } else if (displayPath.startsWith("/")) {
        parts << "Root";
        QString remainder = displayPath.mid(1);
        if (!remainder.isEmpty()) {
            remainder.remove(remainder.length() - 1, 1);  // Remove trailing /
            parts << remainder.split('/');
        }
    } else {
        parts = displayPath.split('/');
    }

    // Build segments with their full paths
    QString currentPath;
    for (int i = 0; i < parts.size(); ++i) {
        const QString& part = parts[i];

        if (i == 0) {
            if (part == "Home") {
                currentPath = m_homeDir;
            } else if (part == "Root") {
                currentPath = "/";
            } else {
                currentPath = part;
            }
        } else {
            if (!currentPath.endsWith("/")) {
                currentPath += "/";
            }
            currentPath += part;
        }

        Segment seg;
        seg.displayText = part;
        seg.fullPath = currentPath;
        m_segments.append(seg);
    }

    // Calculate visual rectangles
    int x = PADDING_X;
    const int y = PADDING_Y;
    const int height = this->height() - 2 * PADDING_Y;

    for (int i = 0; i < m_segments.size(); ++i) {
        QFontMetrics fm(this->font());
        int textWidth = fm.horizontalAdvance(m_segments[i].displayText);
        int segmentWidth = textWidth + 2 * PADDING_X;

        m_segments[i].rect = QRect(x, y, segmentWidth, height);
        x += segmentWidth;

        // Add separator width (except after last segment)
        if (i < m_segments.size() - 1) {
            x += SEPARATOR_WIDTH;
        }
    }
}

void BreadcrumbBar::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &opt, &painter, this);

    // Draw segments
    for (int i = 0; i < m_segments.size(); ++i) {
        const Segment& seg = m_segments[i];

        // Draw background for hovered segment
        if (i == m_hoverSegmentIndex) {
            painter.fillRect(seg.rect, palette().highlight());
        }

        // Draw text
        QColor textColor = (i == m_hoverSegmentIndex) ? palette().highlightedText().color()
                                                      : palette().text().color();
        painter.setPen(textColor);
        painter.drawText(seg.rect, Qt::AlignCenter, seg.displayText);

        // Draw separator (except after last segment)
        if (i < m_segments.size() - 1) {
            int sepX = seg.rect.right() + (SEPARATOR_WIDTH / 2) - 3;
            int sepY = seg.rect.top() + 2;
            int sepHeight = seg.rect.height() - 4;

            painter.setPen(palette().mid().color());
            painter.drawText(sepX, sepY, 6, sepHeight, Qt::AlignCenter, ">");
        }
    }
}

void BreadcrumbBar::mousePressEvent(QMouseEvent* event) {
    for (int i = 0; i < m_segments.size(); ++i) {
        if (m_segments[i].rect.contains(event->pos())) {
            emit pathSelected(m_segments[i].fullPath);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void BreadcrumbBar::mouseMoveEvent(QMouseEvent* event) {
    int prevHoverIndex = m_hoverSegmentIndex;

    m_hoverSegmentIndex = -1;
    for (int i = 0; i < m_segments.size(); ++i) {
        if (m_segments[i].rect.contains(event->pos())) {
            m_hoverSegmentIndex = i;
            setCursor(Qt::PointingHandCursor);
            break;
        }
    }

    if (m_hoverSegmentIndex != prevHoverIndex) {
        update();
    }

    if (m_hoverSegmentIndex == -1) {
        setCursor(Qt::ArrowCursor);
    }

    QWidget::mouseMoveEvent(event);
}

void BreadcrumbBar::leaveEvent(QEvent* event) {
    if (m_hoverSegmentIndex != -1) {
        m_hoverSegmentIndex = -1;
        update();
    }
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}
