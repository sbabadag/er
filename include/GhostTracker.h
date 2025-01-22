#ifndef GHOSTTRACKER_H
#define GHOSTTRACKER_H

#include <QPointF>
#include <QColor>

class GhostTracker {
public:
    GhostTracker();
    void startTracking(const QPointF& startPos);
    void updateGhost(const QPointF& currentPos);
    void stopTracking();
    bool isTracking() const { return tracking; }
    QPointF getOffset() const { return offset; }
    
    static QColor ghostColor() { return QColor(128, 128, 128, 128); }

private:
    bool tracking;
    QPointF startPosition;
    QPointF offset;
};

#endif // GHOSTTRACKER_H
