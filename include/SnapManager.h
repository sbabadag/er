#pragma once

#include <QVector2D>
#include <vector>
#include "Line.h"

class SnapManager {
public:
    enum SnapType {
        SNAP_NONE,
        SNAP_ENDPOINT,
        SNAP_MIDPOINT,
        SNAP_INTERSECTION,  // Add intersection snap type
        SNAP_LINE
    };

    SnapManager(float snapThreshold, float zoomLevel, const std::vector<Line>& lines);
    ~SnapManager();

    QVector2D snapPoint(const QVector2D& point);
    void updateSnap(const QVector2D& point);
    bool isSnapActive() const { return snapActive; }
    void drawSnapMarker(const QVector2D& pan, float zoom);
    void updateSettings(float newSnapThreshold, float newZoom, const std::vector<Line>& lines);
    QVector2D getCurrentSnapPoint() const { return currentSnapPoint; }

private:
    // Add the intersection helper function declaration
    static bool findIntersection(const Line& line1, const Line& line2, QVector2D& intersection);

    float snapThreshold;
    float zoom;
    std::vector<Line> lines;
    QVector2D currentSnapPoint;
    bool snapActive;
    SnapType currentSnapType;
};