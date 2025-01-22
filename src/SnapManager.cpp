#include "SnapManager.h"
#include "Line.h"
#include <GL/gl.h>
#include <algorithm> // For std::clamp
#define _USE_MATH_DEFINES
#include <math.h>

SnapManager::SnapManager(float snapThreshold, float zoomLevel, const std::vector<Line>& lines)
    : snapThreshold(snapThreshold)
    , zoom(zoomLevel)
    , lines(lines)
    , currentSnapPoint(0.0f, 0.0f)
    , snapActive(false)
    , currentSnapType(SNAP_NONE)
{
    // Initialization code...
}

SnapManager::~SnapManager()
{
    // Cleanup code...
}

QVector2D SnapManager::snapPoint(const QVector2D& point)
{
    QVector2D closestPoint = point;
    float baseThreshold = snapThreshold * 5.0f;  // Reduced from 10.0f
    float effectiveThreshold = baseThreshold / zoom;
    float minDistance = std::numeric_limits<float>::max();
    
    snapActive = false;
    currentSnapType = SNAP_NONE;

    // Check endpoints first (highest priority)
    for (const auto& line : lines) {
        float startDist = (point - line.start).length();
        float endDist = (point - line.end).length();
        
        if (startDist < effectiveThreshold && startDist < minDistance) {
            minDistance = startDist;
            closestPoint = line.start;
            snapActive = true;
            currentSnapType = SNAP_ENDPOINT;
        }
        if (endDist < effectiveThreshold && endDist < minDistance) {
            minDistance = endDist;
            closestPoint = line.end;
            snapActive = true;
            currentSnapType = SNAP_ENDPOINT;
        }
    }

    // Check intersections second (if no endpoint found)
    if (!snapActive) {
        for (size_t i = 0; i < lines.size(); i++) {
            for (size_t j = i + 1; j < lines.size(); j++) {
                QVector2D intersection;
                if (findIntersection(lines[i], lines[j], intersection)) {
                    float dist = (point - intersection).length();
                    if (dist < effectiveThreshold && dist < minDistance) {
                        minDistance = dist;
                        closestPoint = intersection;
                        snapActive = true;
                        currentSnapType = SNAP_INTERSECTION;
                    }
                }
            }
        }
    }

    // If no endpoint found, check midpoints
    if (!snapActive) {
        for (const auto& line : lines) {
            QVector2D midpoint = (line.start + line.end) * 0.5f;
            float midDist = (point - midpoint).length();
            if (midDist < effectiveThreshold && midDist < minDistance) {
                minDistance = midDist;
                closestPoint = midpoint;
                snapActive = true;
                currentSnapType = SNAP_MIDPOINT;
            }
        }
    }

    // Finally check line projections with the widest threshold
    if (!snapActive) {
        for (const auto& line : lines) {
            QVector2D ab = line.end - line.start;
            float ab_length_squared = ab.lengthSquared();
            
            if (ab_length_squared > 1e-6f) {
                QVector2D ap = point - line.start;
                float t = QVector2D::dotProduct(ap, ab) / ab_length_squared;
                
                if (t > 0.0f && t < 1.0f) {
                    QVector2D projection = line.start + ab * t;
                    float projDist = (point - projection).length();
                    if (projDist < effectiveThreshold && projDist < minDistance) {
                        minDistance = projDist;
                        closestPoint = projection;
                        snapActive = true;
                        currentSnapType = SNAP_LINE;
                    }
                }
            }
        }
    }

    currentSnapPoint = closestPoint;
    return currentSnapPoint;
}

void SnapManager::updateSettings(float newSnapThreshold, float newZoom, const std::vector<Line>& newLines)
{
    snapThreshold = std::max(newSnapThreshold, 1.0f);  // Ensure minimum threshold
    zoom = std::max(newZoom, 0.1f);  // Prevent zero or negative zoom
    lines = newLines;
    
    // Reset to clean state
    currentSnapType = SNAP_NONE;
    snapActive = false;
    currentSnapPoint = QVector2D(0, 0);
}

void SnapManager::updateSnap(const QVector2D& point)
{
    snapPoint(point);  // This will update all internal state
}

void SnapManager::drawSnapMarker(const QVector2D& /*pan*/, float zoom)
{
    if (!snapActive)
        return;

    float markerSize = 15.0f / zoom;  // Increased marker size
    
    glLineWidth(3.0f);  // Thicker lines for better visibility

    switch (currentSnapType) {
        case SNAP_ENDPOINT:
            // Draw square
            glColor3f(1.0f, 1.0f, 0.0f);  // Yellow
            glBegin(GL_LINE_LOOP);
            glVertex2f(currentSnapPoint.x() - markerSize/2, currentSnapPoint.y() - markerSize/2);
            glVertex2f(currentSnapPoint.x() + markerSize/2, currentSnapPoint.y() - markerSize/2);
            glVertex2f(currentSnapPoint.x() + markerSize/2, currentSnapPoint.y() + markerSize/2);
            glVertex2f(currentSnapPoint.x() - markerSize/2, currentSnapPoint.y() + markerSize/2);
            glEnd();
            break;

        case SNAP_MIDPOINT:
            // Draw diamond
            glColor3f(0.0f, 1.0f, 1.0f);  // Cyan
            glBegin(GL_LINE_LOOP);
            glVertex2f(currentSnapPoint.x(), currentSnapPoint.y() - markerSize/2);
            glVertex2f(currentSnapPoint.x() + markerSize/2, currentSnapPoint.y());
            glVertex2f(currentSnapPoint.x(), currentSnapPoint.y() + markerSize/2);
            glVertex2f(currentSnapPoint.x() - markerSize/2, currentSnapPoint.y());
            glEnd();
            break;

        case SNAP_INTERSECTION:
            // Draw X for intersection
            glColor3f(1.0f, 0.0f, 1.0f);  // Magenta for intersections
            glBegin(GL_LINES);
            // First diagonal
            glVertex2f(currentSnapPoint.x() - markerSize/2, currentSnapPoint.y() - markerSize/2);
            glVertex2f(currentSnapPoint.x() + markerSize/2, currentSnapPoint.y() + markerSize/2);
            // Second diagonal
            glVertex2f(currentSnapPoint.x() - markerSize/2, currentSnapPoint.y() + markerSize/2);
            glVertex2f(currentSnapPoint.x() + markerSize/2, currentSnapPoint.y() - markerSize/2);
            glEnd();
            break;

        default:
            // Draw cross-hair for line snaps
            glColor3f(0.0f, 1.0f, 0.0f);  // Green
            glBegin(GL_LINES);
            glVertex2f(currentSnapPoint.x() - markerSize, currentSnapPoint.y());
            glVertex2f(currentSnapPoint.x() + markerSize, currentSnapPoint.y());
            glVertex2f(currentSnapPoint.x(), currentSnapPoint.y() - markerSize);
            glVertex2f(currentSnapPoint.x(), currentSnapPoint.y() + markerSize);
            glEnd();
            break;
    }

    glLineWidth(1.0f);
}

// Add this helper function to find intersection points
bool SnapManager::findIntersection(const Line& line1, const Line& line2, QVector2D& intersection)
{
    QVector2D p1 = line1.start;
    QVector2D p2 = line1.end;
    QVector2D p3 = line2.start;
    QVector2D p4 = line2.end;

    float x1 = p1.x(), y1 = p1.y();
    float x2 = p2.x(), y2 = p2.y();
    float x3 = p3.x(), y3 = p3.y();
    float x4 = p4.x(), y4 = p4.y();

    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::abs(denom) < 1e-6f)  // Lines are parallel
        return false;

    float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

    if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
        intersection.setX(x1 + t * (x2 - x1));
        intersection.setY(y1 + t * (y2 - y1));
        return true;
    }

    return false;
}