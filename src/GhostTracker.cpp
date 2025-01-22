#include "GhostTracker.h"

GhostTracker::GhostTracker() : tracking(false), offset(0, 0) {}

void GhostTracker::startTracking(const QPointF& startPos) {
    tracking = true;
    startPosition = startPos;
    offset = QPointF(0, 0);
}

void GhostTracker::updateGhost(const QPointF& currentPos) {
    if (tracking) {
        offset = currentPos - startPosition;
    }
}

void GhostTracker::stopTracking() {
    tracking = false;
    offset = QPointF(0, 0);
}
