#ifndef SNAPMANAGER_H
#define SNAPMANAGER_H

#include <QVector2D>
#include <vector>
#include "Line.h"

class SnapManager {
public:
    SnapManager(float snapThreshold, float zoomLevel, const std::vector<Line>& lines);
    ~SnapManager();

    void updateSettings(float newSnapThreshold, float newZoom, const std::vector<Line>& newLines);
    void updateSnap(const QVector2D& point);
    bool isSnapActive() const { return snapActive; }
    QVector2D getCurrentSnapPoint() const { return currentSnapPoint; }
    void drawSnapMarker(const QVector2D& pan, float zoom);
    bool checkTempPoint(const QVector2D& point, const QVector2D& tempPoint, bool hasTempPoint);

private:
    QVector2D snapPoint(const QVector2D& point);
    bool findIntersection(const Line& line1, const Line& line2, QVector2D& intersection);

    float snapThreshold;
    float zoom;
    std::vector<Line> lines;  // Remove const and reference
    QVector2D currentSnapPoint;
    bool snapActive;

    enum SnapType {
        SNAP_NONE,
        SNAP_ENDPOINT,
        SNAP_MIDPOINT,
        SNAP_INTERSECTION,
        SNAP_LINE
    };
    SnapType currentSnapType;
};

#endif // SNAPMANAGER_H
