#include "../include/GLWidget.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <GL/gl.h>
#include <QPainter>
#include "SnapManager.h"  // Include SnapManager implementation
#include "Line.h"         // Include Line struct
#include "DxfHandler.h"   // Include DxfHandler

GLWidget::GLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , QOpenGLFunctions()  // Add base class initialization
    , hasFirstPoint(false)        // Match header order
    , isSelectingRectangle(false)
    , isDragging(false)
    , isCrossingSelection(false)
    , isDrawing(false)
    , lineToolActive(false)
    , firstPoint(0, 0)
    , currentStart(0, 0)
    , currentEnd(0, 0)
    , currentMode(MODE_NONE)
    , snapManager(nullptr)
    , pan(0, 0)
    , zoom(1.0f)
    , snapThreshold(5.0f) // Reduced from 10.0f to 5.0f for more precise snapping
    , m_statusBar(nullptr)
    , currentCommand("Ready")
    , orthoMode(false)
    , targetLength(0.0f)
    , lengthInput("")
    , hasLengthConstraint(false)
    , placingDimension(false)
    , dimStart(0, 0)
    , dimEnd(0, 0)
    , currentDimOffset(20.0f)
    , objectSelected(false)
    , selectedObjectIndex(-1)
    , isAwaitingMoveStartPoint(false)  // Match header order
    , isAwaitingMoveEndPoint(false)   // Initialize new variable
    , isZooming(false)               // Initialize zoom state
    , zoomStartPos(QPoint())         // Initialize zoom start position
    , zoomSensitivity(0.005f)        // Set zoom sensitivity
    , currentColor(Qt::white)  // Initialize current color
    , hasTrackPoint(false)  // Add initialization for hasTrackPoint
    , isShiftSnapping(false)  // Initialize shift snapping state
    , currentShiftSnap(0)  // Initialize shift snap index
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // Enable key events

    // Initialize SnapManager
    snapManager = new SnapManager(snapThreshold, zoom, lines);
    snapManager->updateSettings(snapThreshold, zoom, lines);  // Ensure initial settings are applied

    // Initialize snap history timer
    snapHistoryTimer = new QTimer(this);
    snapHistoryTimer->setInterval(100);  // Check every 100ms
    connect(snapHistoryTimer, &QTimer::timeout, this, [this]() {
        if (lastSnap.isActive && (QDateTime::currentMSecsSinceEpoch() / 1000.0f - lastSnap.timestamp) > snapHistoryTimeout) {
            clearSnapHistory();
            update();
        }
    });
    snapHistoryTimer->start();

    // Initialize track timer
    trackTimer = new QTimer(this);
    trackTimer->setInterval(100);  // Check every 100ms
    connect(trackTimer, &QTimer::timeout, this, &GLWidget::updateTrackPoints);
    trackTimer->start();

    tempIntersection.isValid = false;
    lastSnapPoints[0] = {QVector2D(0,0), QVector2D(0,0)};
    lastSnapPoints[1] = {QVector2D(0,0), QVector2D(0,0)};

    // Initialize track lines
    trackLines[0] = {QVector2D(0,0), QVector2D(0,0), false};
    trackLines[1] = {QVector2D(0,0), QVector2D(0,0), false};
}

GLWidget::~GLWidget()
{
    delete snapManager;
}

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    // Ensure SnapManager has the initial settings
    snapManager->updateSettings(snapThreshold, zoom, lines);
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    
    // Handle pan and zoom in world coordinates
    glScalef(zoom, zoom, 1.0f);
    glTranslatef(pan.x() / zoom, pan.y() / zoom, 0);

    // Draw existing lines
    for (size_t i = 0; i < lines.size(); ++i) {
        if (std::find(selectedObjectIndices.begin(), selectedObjectIndices.end(), static_cast<int>(i)) != selectedObjectIndices.end()) {
            // Selected lines get highlighted
            QColor highlightColor = lines[i].color.lighter(150);
            glColor4f(highlightColor.redF(), highlightColor.greenF(), highlightColor.blueF(), highlightColor.alphaF());
        }
        else {
            // Use the line's stored color
            glColor4f(lines[i].color.redF(), lines[i].color.greenF(), lines[i].color.blueF(), lines[i].color.alphaF());
        }
        glBegin(GL_LINES);
        glVertex2f(lines[i].start.x(), lines[i].start.y());
        glVertex2f(lines[i].end.x(), lines[i].end.y());
        glEnd();
    }

    // Draw ghost preview if in move mode and tracking
    if (currentMode == MODE_MOVE && ghostTracker.isTracking()) {
        renderGhostObjects();
    }

    // Draw dimensions
    glColor3f(0.0f, 1.0f, 0.0f);  // Green color for dimensions
    for (const auto& dim : dimensions) {
        drawDimension(dim);
    }

    // Draw snap marker using SnapManager
    if (snapManager->isSnapActive()) {
        snapManager->drawSnapMarker(pan, zoom);
    }

    // Draw current line if drawing
    if (isDrawing) {
        glColor4f(currentColor.redF(), currentColor.greenF(), currentColor.blueF(), currentColor.alphaF());
        glBegin(GL_LINES);
        glVertex2f(currentStart.x(), currentStart.y());
        glVertex2f(currentEnd.x(), currentEnd.y());
        glEnd();
    }

    // Draw selection rectangle if active
    if (isSelectingRectangle) {
        glColor3f(0.0f, 1.0f, 0.0f);  // Green color for selection rectangle
        glBegin(GL_LINE_LOOP);
        QVector2D topLeft = screenToWorld(selectionRect.topLeft());
        QVector2D topRight = screenToWorld(selectionRect.topRight());
        QVector2D bottomRight = screenToWorld(selectionRect.bottomRight());
        QVector2D bottomLeft = screenToWorld(selectionRect.bottomLeft());

        glVertex2f(topLeft.x(), topLeft.y());
        glVertex2f(topRight.x(), topRight.y());
        glVertex2f(bottomRight.x(), bottomRight.y());
        glVertex2f(bottomLeft.x(), bottomLeft.y());
        glEnd();
    }

    // Draw temporary point if it exists
    if (hasTempPoint) {
        glColor4f(1.0f, 1.0f, 0.0f, 0.8f);  // Yellow color
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glVertex2f(tempPoint.x(), tempPoint.y());
        glEnd();
        glPointSize(1.0f);
    }

    // Draw snap history and construction points
    if (lastSnap.isActive) {
        // Draw last snap point
        glColor4f(1.0f, 1.0f, 0.0f, 0.5f);  // Yellow, semi-transparent
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glVertex2f(lastSnap.point.x(), lastSnap.point.y());
        glEnd();
        
        // Draw construction point if available
        if (hasTempConstructPoint) {
            glColor4f(0.0f, 1.0f, 1.0f, 0.8f);  // Cyan
            glPointSize(6.0f);
            glBegin(GL_POINTS);
            glVertex2f(tempConstructPoint.x(), tempConstructPoint.y());
            glEnd();
            
            // Draw construction line
            glColor4f(0.0f, 1.0f, 1.0f, 0.3f);  // Faded cyan
            glBegin(GL_LINES);
            glVertex2f(lastSnap.point.x(), lastSnap.point.y());
            glVertex2f(tempConstructPoint.x(), tempConstructPoint.y());
            glEnd();
        }
        glPointSize(1.0f);
    }

    // Draw track points
    drawTrackPoints();

    // Draw track point and construction preview
    if (hasTrackPoint) {
        drawTrackPoint();
        
        // Draw potential construction point preview
        QVector2D mousePos = screenToWorld(mapFromGlobal(QCursor::pos()));
        QVector2D constructPoint = getConstructionPoint(mousePos);
        
        glPointSize(6.0f);
        glColor4f(0.0f, 1.0f, 1.0f, 0.5f);  // Cyan for construction preview
        glBegin(GL_POINTS);
        glVertex2f(constructPoint.x(), constructPoint.y());
        glEnd();
        
        // Draw construction line
        glColor4f(0.0f, 1.0f, 1.0f, 0.3f);  // Faded cyan
        glBegin(GL_LINES);
        glVertex2f(currentTrackPoint.point.x(), currentTrackPoint.point.y());
        glVertex2f(constructPoint.x(), constructPoint.y());
        glEnd();
        glPointSize(1.0f);
    }

    // Draw track lines
    drawTrackLines();

    // Draw temporary intersection point if valid
    if (tempIntersection.isValid) {
        glPointSize(8.0f);
        glColor4f(1.0f, 0.0f, 1.0f, 0.8f);  // Magenta color for intersection
        glBegin(GL_POINTS);
        glVertex2f(tempIntersection.point.x(), tempIntersection.point.y());
        glEnd();
        glPointSize(1.0f);
    }

    // Draw shift snap points and lines
    drawShiftSnapLines();
}

QVector2D GLWidget::snapPoint(const QVector2D& point)
{
    // Skip snapping in delete mode
    if (currentMode == MODE_DELETE) {
        return point;
    }

    QVector2D worldPoint = point;
    
    // First check if we can snap to temp construction point
    if (hasTempConstructPoint) {
        float distToTemp = (worldPoint - tempConstructPoint).length();
        if (distToTemp <= snapThreshold / zoom) {
            return tempConstructPoint;
        }
    }

    // Update snap system with current settings
    snapManager->updateSettings(snapThreshold, zoom, lines);
    snapManager->updateSnap(worldPoint);
    
    if (snapManager->isSnapActive()) {
        QVector2D snapPoint = snapManager->getCurrentSnapPoint();
        
        // Update tracking with snap point
        updateTrackLine(snapPoint);

        // Check for intersection snap
        if (tempIntersection.isValid) {
            float distToIntersection = (point - tempIntersection.point).length();
            if (distToIntersection <= snapThreshold / zoom) {
                return tempIntersection.point;
            }
        }
        
        if (!hasTrackPoint) {
            // If no track point exists, create one
            QVector2D dir = (snapPoint - point).normalized();
            setTrackPoint(snapPoint, dir);
        }
        else {
            // If we have a track point, check for construction point
            QVector2D constructPoint = getConstructionPoint(snapPoint);
            float distToConstruct = (constructPoint - point).length();
            
            if (distToConstruct <= snapThreshold / zoom) {
                // Clear track point after using it
                clearTrackPoint();
                return constructPoint;
            }
        }
        
        // Update intersection tracking with new snap point
        updateIntersectionPoint(snapPoint);
        
        return snapPoint;
    }

    // Check if we can snap to intersection point
    if (tempIntersection.isValid) {
        float distToIntersection = (point - tempIntersection.point).length();
        if (distToIntersection <= snapThreshold / zoom) {
            return tempIntersection.point;
        }
    }
    
    return point;
}

void GLWidget::updateSnapHistory(const QVector2D& snapPoint, const QVector2D& dir)
{
    lastSnap.point = snapPoint;
    lastSnap.direction = dir;
    lastSnap.timestamp = QDateTime::currentMSecsSinceEpoch() / 1000.0f;
    lastSnap.isActive = true;
}

void GLWidget::clearSnapHistory()
{
    lastSnap.isActive = false;
    hasTempConstructPoint = false;
    tempIntersection.isValid = false;
    currentSnapIndex = 0;
    clearTrackLines();  // Clear track lines when clearing snap history
}

QVector2D GLWidget::calculatePerpendicularPoint(const QVector2D& base, const QVector2D& ref, const QVector2D& dir) const
{
    QVector2D v = base - ref;
    float dist = v.length();
    
    if (dist < 0.0001f) return base;

    float proj = QVector2D::dotProduct(v, dir);
    return ref + dir * proj;
}

QVector2D GLWidget::calculateParallelPoint(const QVector2D& base, const QVector2D& ref, const QVector2D& dir) const
{
    QVector2D v = base - ref;
    float dist = v.length();
    
    if (dist < 0.0001f) return base;

    // Project onto perpendicular direction
    QVector2D perpDir(-dir.y(), dir.x());
    float proj = QVector2D::dotProduct(v, perpDir);
    return base - perpDir * proj;
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-w/2.0f, w/2.0f, -h/2.0f, h/2.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
}

QVector2D GLWidget::screenToWorld(const QPoint& screenPos)
{
    return QVector2D(
        (screenPos.x() - width()/2.0f - pan.x()) / zoom,
        (-screenPos.y() + height()/2.0f - pan.y()) / zoom
    );
}

QVector2D GLWidget::worldToScreen(const QVector2D& worldPos)
{
    return QVector2D(
        worldPos.x() * zoom + width()/2.0f + pan.x(),
        -worldPos.y() * zoom + height()/2.0f + pan.y()
    );
}

QVector2D GLWidget::constrainToOrtho(const QVector2D& start, const QVector2D& end)
{
    if (!orthoMode) return end;
    
    QVector2D delta = end - start;
    if (abs(delta.x()) > abs(delta.y())) {
        // Horizontal line - use cursor's exact X position
        return QVector2D(end.x(), start.y());
    } else {
        // Vertical line - use cursor's exact Y position
        return QVector2D(start.x(), end.y());
    }
}

// ...existing code...

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    // Clear temporary point when starting a new action
    clearTempPoint();
    // Clear construction points on new action
    clearSnapHistory();
    clearTrackPoint();  // Clear track point on new action
    clearTrackLines();  // Clear track lines on new action

    QVector2D worldPos = screenToWorld(event->pos());
    QVector2D snappedPos = snapPoint(worldPos);

    if (event->button() == Qt::MiddleButton) {
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // Clear tracking lines on left click unless holding Shift
        if (!(event->modifiers() & Qt::ShiftModifier)) {
            clearTrackLines();
        }
        if (currentMode == MODE_MOVE) {
            if (!isAwaitingMoveStartPoint && !isAwaitingMoveEndPoint) {
                // First click: Select object and start ghost preview
                selectObjectAt(worldPos);
                if (objectSelected) {
                    isAwaitingMoveStartPoint = true;
                    ghostTracker.startTracking(QPointF(snappedPos.x(), snappedPos.y()));
                    currentCommand = "Move: Click base point";
                    emit commandChanged(currentCommand);
                }
            }
            else if (isAwaitingMoveStartPoint) {
                // Second click: Set base point and update ghost
                moveStartPoint = snappedPos;
                isAwaitingMoveStartPoint = false;
                isAwaitingMoveEndPoint = true;
                ghostTracker.updateGhost(QPointF(snappedPos.x(), snappedPos.y()));
                currentCommand = "Move: Click destination point";
                emit commandChanged(currentCommand);
            }
            else if (isAwaitingMoveEndPoint) {
                // Third click: Complete move
                QVector2D delta = snappedPos - moveStartPoint;
                moveSelectedObject(delta);
                isAwaitingMoveEndPoint = false;
                ghostTracker.stopTracking();
                currentCommand = "Move completed";
                emit commandChanged(currentCommand);
                deselectObject();
            }
            updateCommandStatus();
            update();
            return;
        }
        if (currentMode == MODE_DELETE) {
            // Start rectangle selection in delete mode
            isSelectingRectangle = true;
            selectionStartPos = event->pos();
            selectionEndPos = event->pos();
            isCrossingSelection = false;
            selectionRect = QRect(selectionStartPos, selectionEndPos);
            update();
            return;
        }
        // ...existing code for other modes...
        else if (currentMode == MODE_LINE) {  // Prioritize line drawing
            if (!hasFirstPoint) {
                // First point of the line
                hasFirstPoint = true;
                firstPoint = snapPoint(worldPos);
                currentStart = firstPoint;
                currentEnd = firstPoint;
                isDrawing = true;
                // Reset move-related states
                isDragging = false;
                selectedObjectIndices.clear();
                objectSelected = false;
                selectedObjectIndex = -1;
            } else {
                // Second point - complete the line
                hasFirstPoint = false;
                isDrawing = false;
                QVector2D snappedEnd = snapPoint(worldPos);
                addLine(currentStart, snappedEnd); // Use snapped endpoint
            }
        }
        else {  // Handle other modes
            // Allow rectangle selection in MODE_NONE and MODE_MOVE
            if (currentMode == MODE_NONE || currentMode == MODE_MOVE) {
                // Initiate rectangle selection
                isSelectingRectangle = true;
                selectionStartPos = event->pos();
                selectionEndPos = event->pos();
                // Determine selection mode based on direction
                isCrossingSelection = false;  // Will be determined during mouse movement
                selectionRect = QRect(selectionStartPos, selectionEndPos);
            }
            // ...existing code for other modes...
        }
    }
    else if (event->button() == Qt::RightButton) {
        if (event->modifiers() & Qt::ShiftModifier) {
            // Initiate zoom mode
            isZooming = true;
            zoomStartPos = event->pos();
            currentCommand = "Zoom Mode: Drag horizontally to zoom";
            emit commandChanged(currentCommand);
            updateCommandStatus();
        }
        else {
            lastMousePos = event->pos();
        }
    }

    updateCommandStatus();
    update();
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::MiddleButton) {
        QPoint delta = event->pos() - lastMousePos;
        // Invert Y direction for natural panning
        pan += QVector2D(delta.x(), -delta.y()) / zoom;
        lastMousePos = event->pos();
        update();
        return;
    }

    QVector2D worldPos = screenToWorld(event->pos());
    
    // Update snap settings before getting snapped position
    snapManager->updateSettings(snapThreshold, zoom, lines);
    QVector2D snappedPos = snapPoint(worldPos);

    // Update coordinates and snap markers
    snapManager->updateSnap(worldPos);
    updateCoordinates(snappedPos);

    // Update line preview while drawing
    if (isDrawing && hasFirstPoint) {
        currentEnd = orthoMode ? constrainToOrtho(currentStart, snappedPos) : snappedPos;
        if (hasLengthConstraint && targetLength > 0) {
            applyLengthConstraint(currentEnd);
        }
        update();
    }

    // Handle ghost preview for move mode
    if (currentMode == MODE_MOVE && ghostTracker.isTracking()) {
        if (isAwaitingMoveEndPoint) {
            QVector2D delta = snappedPos - moveStartPoint;
            ghostTracker.updateGhost(QPointF(delta.x(), delta.y()));
        } else if (isAwaitingMoveStartPoint) {
            ghostTracker.updateGhost(QPointF(snappedPos.x(), snappedPos.y()));
        }
        update();
    }

    // Handle selection rectangle
    if (isSelectingRectangle) {
        selectionEndPos = event->pos();
        // Determine crossing selection mode: 
        // If dragging right to left OR bottom to top = crossing selection
        isCrossingSelection = (selectionEndPos.x() < selectionStartPos.x()) || 
                            (selectionEndPos.y() < selectionStartPos.y());
        selectionRect = QRect(selectionStartPos, selectionEndPos).normalized();
        update();
    }

    update();
}

// Remove or comment out isDragging related code in mouseReleaseEvent
void GLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    QVector2D worldPos = screenToWorld(event->pos());
    QVector2D snappedPos = snapPoint(worldPos);

    if (event->button() == Qt::LeftButton) {
        if (isSelectingRectangle) {
            isSelectingRectangle = false;
            performRectangleSelection(selectionRect);
            selectionRect = QRect();
            
            // If in delete mode, delete selected objects immediately
            if (currentMode == MODE_DELETE && !selectedObjectIndices.empty()) {
                deleteSelectedObjects();
                currentCommand = "Objects deleted. Select more objects to delete or ESC to exit";
                emit commandChanged(currentCommand);
            }
        }
    }
    else if (event->button() == Qt::RightButton) {
        if (isZooming) {
            // Finalize zoom mode
            isZooming = false;
            currentCommand = "Zoom Mode: Completed";
            emit commandChanged(currentCommand);
            updateCommandStatus();
        }
        else {
            update();
        }
    }
    
    if (currentMode == MODE_MOVE) {
        if (isAwaitingMoveEndPoint) {
            // Use snapped position for final move
            QVector2D delta = snappedPos - moveStartPoint;
            moveSelectedObject(delta);
            isAwaitingMoveEndPoint = false;
            ghostTracker.stopTracking();
        }
    }
}

// ...existing code...

void GLWidget::wheelEvent(QWheelEvent* event)
{
    QVector2D mouseWorld = screenToWorld(event->position().toPoint());
    
    float zoomFactor = event->angleDelta().y() > 0 ? 1.1f : 0.9f;
    float newZoom = zoom * zoomFactor;
    newZoom = std::clamp(newZoom, 0.01f, 100.0f);
    
    // Keep mouse position fixed in world space during zoom
    QVector2D oldWorld = mouseWorld;
    zoom = newZoom;
    QVector2D newWorld = screenToWorld(event->position().toPoint());
    pan += (newWorld - oldWorld) * zoom;

    float worldThreshold = snapThreshold / zoom;
    snapManager->updateSettings(worldThreshold, zoom, lines);
    
    update();
}

void GLWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F8) {
        orthoMode = !orthoMode;
        updateCommandStatus();
    }
    else if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9) {
        processNumericInput(event->text());
    }
    else if (event->key() == Qt::Key_Period) {
        processNumericInput(".");
    }
    else if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        processNumericInput("Enter");
    }
    else if (event->key() == Qt::Key_Backspace) {
        processNumericInput("Backspace");
    }
    else if (event->key() == Qt::Key_Escape) {
        if (currentMode == MODE_DELETE) {
            // Exit delete mode and update button state
            setCurrentMode(MODE_NONE);
            if (deleteButton) {
                deleteButton->setDown(false);
            }
            currentCommand = "Delete mode canceled";
            emit commandChanged(currentCommand);
        }
        cancelDrawing();
    }
    else if (event->key() == Qt::Key_Delete) {
        if (currentMode != MODE_DELETE) {
            startDeleteMode();
        }
    }
    if (event->key() == Qt::Key_Shift) {
        isShiftSnapping = true;
        if (snapManager && snapManager->hasCurrentSnapPoint()) {
            handleShiftSnap(snapManager->getCurrentSnapPoint());
        }
    }
    update();
}

void GLWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Shift) {
        isShiftSnapping = false;
        clearShiftSnaps();
        update();
    }
    event->accept();
}

void GLWidget::addLine(const QVector2D& start, const QVector2D& end)
{
    // Apply ortho constraint when adding the final line
    QVector2D finalEnd = orthoMode ? constrainToOrtho(start, end) : end;
    lines.push_back({start, finalEnd, currentColor});  // Use current color when adding line
    
    // Update snap system with new line
    if (snapManager) {
        snapManager->updateSettings(snapThreshold, zoom, lines);
    }
}

void GLWidget::setStatusBar(QStatusBar* statusBar)
{
    m_statusBar = statusBar;
    updateCommandStatus();
}

void GLWidget::updateCommandStatus()
{
    if (!m_statusBar) return;
    
    QString status;
    if (currentMode == MODE_DELETE) {
        status = "Delete Mode: Select objects to delete (ESC to exit)";
    }
    else if (currentMode == MODE_MOVE) {
        if (isAwaitingMoveStartPoint) {
            status = "Move Mode: Click to set Move Start Point";
        }
        else if (isAwaitingMoveEndPoint) {
            status = "Move Mode: Click to set Move End Point";
        }
        else if (objectSelected) {
            status = "Move Mode: Object Selected - Ready to Move";
        }
    }
    else if (isDrawing) {
        QString lengthStr = hasLengthConstraint ? 
            QString(" | Length: %1").arg(lengthInput.isEmpty() ? QString::number(targetLength) : lengthInput) : "";
        
        if (hasFirstPoint) {
            status = QString("Drawing Line: Click for end point%1%2")
                .arg(orthoMode ? " (Ortho)" : "")
                .arg(lengthStr);
        } else {
            status = QString("Drawing Line: Click for start point%1")
                .arg(orthoMode ? " (Ortho)" : "");
        }
    } else {
        status = QString("Ready - Left click: Draw line | Right click: Pan | Wheel: Zoom | F8: Ortho %1")
            .arg(orthoMode ? "ON" : "OFF");
    }
    
    m_statusBar->showMessage(status);

    // Emit the commandChanged signal with the current command
    emit commandChanged(currentCommand);
}

void GLWidget::updateCoordinates(const QVector2D& worldPos)
{
    if (!m_statusBar) return;
    
    QString coords = QString(" | X: %1, Y: %2").arg(worldPos.x()).arg(worldPos.y());
    QString currentMsg = m_statusBar->currentMessage();
    
    if (currentMsg.contains(" | X:")) {
        currentMsg = currentMsg.left(currentMsg.indexOf(" | X:"));
    }
    m_statusBar->showMessage(currentMsg + coords);
}

void GLWidget::processNumericInput(const QString& key)
{
    if (!isDrawing) return;
    
    if (key == "Enter" || key == "Return") {
        if (!lengthInput.isEmpty()) {
            targetLength = lengthInput.toFloat();
            hasLengthConstraint = true;
            lengthInput.clear();
            
            // Apply length constraint immediately
            if (hasFirstPoint && targetLength > 0) {
                // Get current direction from mouse position
                QVector2D direction = currentEnd - currentStart;
                if (direction.length() > 0) {
                    direction.normalize();
                    currentEnd = currentStart + direction * targetLength;
                    
                    // Complete the line if Enter was pressed
                    hasFirstPoint = false;
                    isDrawing = false;
                    addLine(currentStart, currentEnd);
                }
            }
            updateCommandStatus();
            update();
        }
    } 
    else if (key == "Backspace") {
        if (!lengthInput.isEmpty()) {
            lengthInput.chop(1); // Remove last character
            if (lengthInput.isEmpty()) {
                hasLengthConstraint = false;
            } else {
                targetLength = lengthInput.toFloat();
                hasLengthConstraint = true;
                if (hasFirstPoint) {
                    QVector2D direction = currentEnd - currentStart;
                    if (direction.length() > 0) {
                        direction.normalize();
                        currentEnd = currentStart + direction * targetLength;
                    }
                }
            }
        }
    }
    else {
        lengthInput += key;
        // Apply length immediately while typing
        if (!lengthInput.isEmpty()) {
            targetLength = lengthInput.toFloat();
            if (targetLength > 0) {
                hasLengthConstraint = true;
                QVector2D direction = currentEnd - currentStart;
                if (direction.length() > 0) {
                    direction.normalize();
                    currentEnd = currentStart + direction * targetLength;
                }
            }
        }
    }
    updateCommandStatus();
    update();
}

void GLWidget::applyLengthConstraint(QVector2D& end)
{
    if (!hasLengthConstraint || targetLength <= 0) return;

    QVector2D direction = end - currentStart;
    if (direction.length() > 0) {
        direction.normalize();
        end = currentStart + direction * targetLength;
    }
}

QVector2D GLWidget::findMidPoint(const QVector2D& point)
{
    for (const auto& line : lines) {
        QVector2D mid = (line.start + line.end) * 0.5f;
        if ((mid - point).length() < snapThreshold / zoom) {
            return mid;
        }
    }
    return point;
}

void GLWidget::startLineDrawing()
{
    resetDrawingState();
    currentMode = MODE_LINE;  // Use currentMode instead of lineToolActive
    currentCommand = "Line";
    
    // Ensure snap system is ready for line drawing
    if (snapManager) {
        snapManager->updateSettings(snapThreshold, zoom, lines);
    }
    
    updateCommandStatus();
    update();
}

void GLWidget::cancelDrawing()
{
    resetDrawingState();
    currentMode = MODE_NONE;  // Use currentMode
    currentCommand = "Ready";
    updateCommandStatus();
    update();
}

void GLWidget::resetDrawingState()
{
    isDrawing = false;
    hasFirstPoint = false;
    placingDimension = false;
    lengthInput.clear();
    hasLengthConstraint = false;
}

void GLWidget::drawDimension(const Dimension& dim)
{
    // Draw extension lines
    glBegin(GL_LINES);
    glVertex2f(dim.start.x(), dim.start.y());
    glVertex2f(dim.start.x(), dim.start.y() + dim.offset);
    glVertex2f(dim.end.x(), dim.end.y());
    glVertex2f(dim.end.x(), dim.end.y() + dim.offset);
    glEnd();

    // Draw dimension line
    QVector2D direction = dim.end - dim.start;
    if (direction.length() == 0) return; // Prevent division by zero
    QVector2D perp = QVector2D(-direction.y(), direction.x()).normalized();
    QVector2D dimLineStart = QVector2D(dim.start.x(), dim.start.y()) + perp * dim.offset;
    QVector2D dimLineEnd = QVector2D(dim.end.x(), dim.end.y()) + perp * dim.offset;

    glBegin(GL_LINES);
    glVertex2f(dimLineStart.x(), dimLineStart.y());
    glVertex2f(dimLineEnd.x(), dimLineEnd.y());
    glEnd();

    // Draw arrows
    float arrowSize = 5.0f / zoom;
    QVector2D dir = direction.normalized();
    QVector2D perpOffset = perp * arrowSize;

    // Arrow 1 at dimLineStart
    glBegin(GL_LINE_LOOP);
    glVertex2f(dimLineStart.x(), dimLineStart.y());
    glVertex2f(dimLineStart.x() + dir.x() * arrowSize + perpOffset.x(),
               dimLineStart.y() + dir.y() * arrowSize + perpOffset.y());
    glVertex2f(dimLineStart.x() + dir.x() * arrowSize - perpOffset.x(),
               dimLineStart.y() + dir.y() * arrowSize - perpOffset.y());
    glEnd();

    // Arrow 2 at dimLineEnd
    glBegin(GL_LINE_LOOP);
    glVertex2f(dimLineEnd.x(), dimLineEnd.y());
    glVertex2f(dimLineEnd.x() - dir.x() * arrowSize + perpOffset.x(),
               dimLineEnd.y() - dir.y() * arrowSize + perpOffset.y());
    glVertex2f(dimLineEnd.x() - dir.x() * arrowSize - perpOffset.x(),
               dimLineEnd.y() - dir.y() * arrowSize - perpOffset.y());
    glEnd();

    // Draw measurement text centered on the dimension line
    QVector2D center = (dimLineStart + dimLineEnd) * 0.5f;
    renderText(center.x(), center.y(), dim.text);  // Position text on the dimension line
}

void GLWidget::renderText(float x, float y, const QString& text)
{
    QPainter painter(this);
    painter.setPen(Qt::green);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // Convert to screen coordinates
    QVector2D screenPos = worldToScreen(QVector2D(x, y));
    
    // Center the text using QRectF and alignment flags
    QFontMetrics fm = painter.fontMetrics();
    int textWidth = fm.horizontalAdvance(text);
    int textHeight = fm.height();
    
    QRectF rect(screenPos.x() - textWidth / 2, screenPos.y() - textHeight / 2, textWidth, textHeight);
    
    painter.drawText(rect, Qt::AlignCenter, text);
    painter.end();
}

void GLWidget::addDimension(const QVector2D& start, const QVector2D& end, float offset)
{
    Dimension dim;
    dim.start = start;
    dim.end = end;
    dim.offset = offset;
    dim.measurement = (end - start).length();
    dim.text = QString::number(dim.measurement, 'f', 2);
    dimensions.push_back(dim);
}

void GLWidget::startDimensionDrawing()
{
    currentMode = MODE_DIMENSION;
    resetDrawingState();
    currentMode = MODE_DIMENSION;
    currentCommand = "Dimension";
    updateCommandStatus();
    update();
}

void GLWidget::selectObjectAt(const QVector2D& point)
{
    const float selectionRadius = 10.0f / zoom;  // Adjust as needed
    int index = -1;

    for (size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];
        // Check distance from point to line segment
        QVector2D ab = line.end - line.start;
        QVector2D ap = point - line.start;
        float ab_length_squared = ab.lengthSquared();
        float t = QVector2D::dotProduct(ap, ab) / ab_length_squared;
        t = std::clamp(t, 0.0f, 1.0f);
        QVector2D projection = line.start + ab * t;
        float distance = (point - projection).length();

        if (distance <= selectionRadius) {
            index = static_cast<int>(i);
            break;
        }
    }

    if (index != -1) {
        objectSelected = true;
        selectedObjectIndices.clear();
        selectedObjectIndices.push_back(index);
        // Set moveHoldPoint to the initial click position for accurate delta calculation
        moveHoldPoint = point;
    }
    else {
        deselectObject();
    }
}

void GLWidget::deselectObject()
{
    objectSelected = false;
    selectedObjectIndex = -1;
}

void GLWidget::moveSelectedObject(const QVector2D& delta)
{
    for (int index : selectedObjectIndices) {
        if (index >= 0 && index < static_cast<int>(lines.size())) {
            lines[index].start += delta;
            lines[index].end += delta;
        }
    }
    
    snapManager->updateSettings(snapThreshold, zoom, lines);
    update();
}

void GLWidget::setCurrentMode(DrawMode mode)
{
    currentMode = mode;
    
    // Update all tool button states
    if (deleteButton) deleteButton->setDown(mode == MODE_DELETE);
    if (moveButton) moveButton->setDown(mode == MODE_MOVE);
    if (lineButton) lineButton->setDown(mode == MODE_LINE);
    if (dimensionButton) dimensionButton->setDown(mode == MODE_DIMENSION);
    
    // Reset and reinitialize snap system when changing modes
    if (snapManager) {
        snapManager->updateSettings(snapThreshold, zoom, lines);
    }

    // Clear selection and reset move states when mode changes
    if (mode != MODE_MOVE && mode != MODE_NONE) {
        selectedObjectIndices.clear();
        objectSelected = false;
        selectedObjectIndex = -1;
        isDragging = false;
        update();
    }

    if (mode == MODE_MOVE) {
        currentCommand = "Move: Drag Objects";
    }
    else {
        currentCommand = "Ready";
    }

    emit commandChanged(currentCommand);
    updateCommandStatus();
    update();
}

// Add these new methods to set button pointers
void GLWidget::setToolButtons(QToolButton* line, QToolButton* move, 
                            QToolButton* del, QToolButton* dimension)
{
    lineButton = line;
    moveButton = move;
    deleteButton = del;
    dimensionButton = dimension;
}

// ...existing code...

void GLWidget::setCurrentCommand(const QString& command)
{
    currentCommand = command;
}

void GLWidget::triggerUpdateCommandStatus()
{
    updateCommandStatus();
}

void GLWidget::performRectangleSelection(const QRect& rect)
{
    selectedObjectIndices.clear();
    if (rect.isNull()) {
        objectSelected = false;
        selectedObjectIndex = -1;
        return;
    }

    // Convert QRect to world coordinates
    QVector2D topLeft(screenToWorld(rect.topLeft()));
    QVector2D topRight(screenToWorld(rect.topRight()));
    QVector2D bottomRight(screenToWorld(rect.bottomRight()));
    QVector2D bottomLeft(screenToWorld(rect.bottomLeft()));

    QRectF worldRect(QPointF(std::min(topLeft.x(), bottomRight.x()), std::min(topLeft.y(), bottomRight.y())),
                    QPointF(std::max(topLeft.x(), bottomRight.x()), std::max(topLeft.y(), bottomRight.y())));

    for (size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];
        bool shouldSelect = false;

        if (isCrossingSelection) {
            // Crossing selection (right-to-left OR bottom-to-top):
            // Select if line intersects with rectangle or has endpoint inside
            bool intersects = false;
            intersects |= linesIntersect(line.start, line.end, topLeft, topRight);
            intersects |= linesIntersect(line.start, line.end, topRight, bottomRight);
            intersects |= linesIntersect(line.start, line.end, bottomRight, bottomLeft);
            intersects |= linesIntersect(line.start, line.end, bottomLeft, topLeft);

            bool pointInside = worldRect.contains(QPointF(line.start.x(), line.start.y())) ||
                             worldRect.contains(QPointF(line.end.x(), line.end.y()));

            shouldSelect = intersects || pointInside;
        } else {
            // Window selection (left-to-right AND top-to-bottom):
            // Select only if line is completely inside
            shouldSelect = worldRect.contains(QPointF(line.start.x(), line.start.y())) &&
                         worldRect.contains(QPointF(line.end.x(), line.end.y()));
        }

        if (shouldSelect) {
            selectedObjectIndices.push_back(static_cast<int>(i));
        }
    }

    objectSelected = !selectedObjectIndices.empty();
    if (objectSelected) {
        selectedObjectIndex = selectedObjectIndices[0];
    } else {
        selectedObjectIndex = -1;
    }
    
    // Update status after selection
    updateCommandStatus();
    update();
}

void GLWidget::deleteSelectedObjects()
{
    if (selectedObjectIndices.empty()) return;

    // Sort indices in descending order to remove from back to front
    std::sort(selectedObjectIndices.begin(), selectedObjectIndices.end(), std::greater<int>());

    // Remove the selected lines
    for (int index : selectedObjectIndices) {
        if (index >= 0 && index < static_cast<int>(lines.size())) {
            lines.erase(lines.begin() + index);
        }
    }

    // Clear selection
    selectedObjectIndices.clear();
    objectSelected = false;
    selectedObjectIndex = -1;

    // Update snap manager and UI
    if (snapManager) {
        snapManager->updateSettings(snapThreshold, zoom, lines);
    }
    
    updateCommandStatus();
    update();
}

// ...existing code...

void GLWidget::zoomAll()
{
    if (lines.empty()) {
        // Reset to default view if no lines are present
        pan = QVector2D(0, 0);
        zoom = 1.0f;
        update();
        return;
    }

    // Determine the bounding box of all lines
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (const auto& line : lines) {
        minX = std::min(minX, std::min(line.start.x(), line.end.x()));
        minY = std::min(minY, std::min(line.start.y(), line.end.y()));
        maxX = std::max(maxX, std::max(line.start.x(), line.end.x()));
        maxY = std::max(maxY, std::max(line.start.y(), line.end.y()));
    }

    // Calculate the center of the bounding box
    QVector2D center((minX + maxX) / 2.0f, (minY + maxY) / 2.0f);

    // Calculate the required zoom to fit all lines
    float viewWidth = static_cast<float>(width());
    float viewHeight = static_cast<float>(height());

    float dataWidth = maxX - minX;
    float dataHeight = maxY - minY;

    if (dataWidth == 0.0f) dataWidth = 1.0f;
    if (dataHeight == 0.0f) dataHeight = 1.0f;

    float zoomX = viewWidth / (dataWidth * 1.2f);  // Add padding
    float zoomY = viewHeight / (dataHeight * 1.2f); // Add padding

    // Use the smaller zoom to fit both width and height
    zoom = std::min(zoomX, zoomY);

    // Center the view
    pan = QVector2D(-center.x() * zoom, -center.y() * zoom);

    // Update SnapManager with new zoom and pan
    snapManager->updateSettings(snapThreshold, zoom, lines);

    update();
}

bool GLWidget::saveDxf(const QString& filename)
{
    bool success = DxfHandler::saveDxf(filename, lines);
    if (success) {
        currentCommand = "File saved: " + filename;
    } else {
        currentCommand = "Error saving file!";
    }
    emit commandChanged(currentCommand);
    updateCommandStatus();
    return success;
}

bool GLWidget::loadDxf(const QString& filename)
{
    std::vector<Line> loadedLines;
    bool success = DxfHandler::loadDxf(filename, loadedLines);
    
    if (success) {
        lines = loadedLines;
        snapManager->updateSettings(snapThreshold, zoom, lines);
        currentCommand = "File loaded: " + filename;
        zoomAll();  // Adjust view to show all loaded lines
    } else {
        currentCommand = "Error loading file!";
    }
    
    emit commandChanged(currentCommand);
    updateCommandStatus();
    update();
    return success;
}

// ...existing code...

void GLWidget::resetAll()
{
    // Clear all data
    lines.clear();
    dimensions.clear();
    selectedObjectIndices.clear();
    
    // Reset view
    pan = QVector2D(0, 0);
    zoom = 1.0f;
    
    // Reset states
    isDrawing = false;
    hasFirstPoint = false;
    objectSelected = false;
    selectedObjectIndex = -1;
    isDragging = false;
    currentMode = MODE_NONE;
    
    // Reset snap system
    if (snapManager) {
        snapManager->updateSettings(snapThreshold, zoom, lines);
    }
    
    // Update UI
    currentCommand = "Ready";
    emit commandChanged(currentCommand);
    updateCommandStatus();
    update();
}

// ...existing code...

void GLWidget::renderGhostObjects() {
    if (!ghostTracker.isTracking() || selectedObjectIndices.empty()) {
        return;
    }

    QPointF offset = ghostTracker.getOffset();
    QVector2D moveOffset(offset.x(), offset.y());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    QColor ghost = GhostTracker::ghostColor();
    glColor4f(ghost.redF(), ghost.greenF(), ghost.blueF(), ghost.alphaF());

    glBegin(GL_LINES);
    for (int index : selectedObjectIndices) {
        if (index >= 0 && static_cast<size_t>(index) < lines.size()) {
            const Line& line = lines[index];
            QVector2D ghostStart, ghostEnd;
            
            if (isAwaitingMoveEndPoint) {
                // Show ghost at offset from original position
                ghostStart = line.start + moveOffset;
                ghostEnd = line.end + moveOffset;
            } else {
                // Show ghost at current mouse position
                ghostStart = line.start;
                ghostEnd = line.end;
                QVector2D delta(offset.x(), offset.y());
                ghostStart += delta;
                ghostEnd += delta;
            }
            
            glVertex2f(ghostStart.x(), ghostStart.y());
            glVertex2f(ghostEnd.x(), ghostEnd.y());
        }
    }
    glEnd();

    glDisable(GL_BLEND);
}

void GLWidget::setSelectedObjectsColor(const QColor& color)
{
    for (int index : selectedObjectIndices) {
        if (index >= 0 && index < static_cast<int>(lines.size())) {
            lines[index].color = color;
        }
    }
    update();
}

// Add the line intersection helper function
bool GLWidget::linesIntersect(const QVector2D& p1, const QVector2D& p2,
                            const QVector2D& q1, const QVector2D& q2) const
{
    auto orientation = [](const QVector2D& a, const QVector2D& b, const QVector2D& c) -> int {
        float val = (b.y() - a.y()) * (c.x() - b.x()) - 
                    (b.x() - a.x()) * (c.y() - b.y());
        if (val == 0.0f) return 0; // colinear
        return (val > 0.0f) ? 1 : 2; // clock or counterclock wise
    };

    int o1 = orientation(p1, p2, q1);
    int o2 = orientation(p1, p2, q2);
    int o3 = orientation(q1, q2, p1);
    int o4 = orientation(q1, q2, p2);

    if (o1 != o2 && o3 != o4)
        return true;

    return false;
}

// ...rest of existing code...

void GLWidget::startDeleteMode()
{
    if (currentMode == MODE_DELETE) {
        // If already in delete mode, exit it
        setCurrentMode(MODE_NONE);
        currentCommand = "Delete mode exited";
    } else {
        // Enter delete mode
        setCurrentMode(MODE_DELETE);
        currentCommand = "Delete Mode: Select objects to delete (snapping disabled)";
        // Snapping is handled in snapPoint method, no need to modify SnapManager
    }
    emit commandChanged(currentCommand);
    updateCommandStatus();
    update();
}

// ...existing code...

void GLWidget::clearTempPoint()
{
    hasTempPoint = false;
    tempPointLifetime = 0.0f;
    update();
}

void GLWidget::addTrackPoint(const QVector2D& point, const QVector2D& dir, TrackPoint::Type type)
{
    // Remove any existing track points that are too close
    trackPoints.erase(
        std::remove_if(trackPoints.begin(), trackPoints.end(),
            [&](const TrackPoint& tp) {
                return (tp.point - point).length() < snapThreshold / zoom;
            }),
        trackPoints.end()
    );

    TrackPoint tp;
    tp.point = point;
    tp.direction = dir;
    tp.timestamp = QDateTime::currentMSecsSinceEpoch();
    tp.type = type;
    trackPoints.push_back(tp);
}

void GLWidget::updateTrackPoints()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    trackPoints.erase(
        std::remove_if(trackPoints.begin(), trackPoints.end(),
            [&](const TrackPoint& tp) {
                return (currentTime - tp.timestamp) / 1000.0f > trackTimeout;
            }),
        trackPoints.end()
    );
    
    if (!trackPoints.empty()) {
        update();
    }
}

void GLWidget::drawTrackPoints() const
{
    for (const auto& tp : trackPoints) {
        glPointSize(8.0f);
        switch (tp.type) {
            case TrackPoint::SNAP:
                glColor4f(1.0f, 1.0f, 0.0f, 0.5f);  // Yellow
                break;
            case TrackPoint::TRACK:
                glColor4f(0.0f, 1.0f, 1.0f, 0.5f);  // Cyan
                break;
            case TrackPoint::PARALLEL:
                glColor4f(0.0f, 1.0f, 0.0f, 0.5f);  // Green
                break;
            case TrackPoint::PERP:
                glColor4f(1.0f, 0.5f, 0.0f, 0.5f);  // Orange
                break;
        }
        
        // Draw the point
        glBegin(GL_POINTS);
        glVertex2f(tp.point.x(), tp.point.y());
        glEnd();

        // Draw direction indicator if applicable
        if (tp.type == TrackPoint::PARALLEL || tp.type == TrackPoint::PERP) {
            float len = 20.0f / zoom;
            QVector2D end = tp.point + tp.direction * len;
            glBegin(GL_LINES);
            glVertex2f(tp.point.x(), tp.point.y());
            glVertex2f(end.x(), end.y());
            glEnd();
        }
    }
    glPointSize(1.0f);
}

void GLWidget::setTrackPoint(const QVector2D& point, const QVector2D& dir)
{
    currentTrackPoint.point = point;
    currentTrackPoint.direction = dir;
    currentTrackPoint.timestamp = QDateTime::currentMSecsSinceEpoch();
    currentTrackPoint.isBase = true;
    hasTrackPoint = true;
}

void GLWidget::clearTrackPoint()
{
    hasTrackPoint = false;
}

QVector2D GLWidget::getConstructionPoint(const QVector2D& currentPos) const
{
    if (!hasTrackPoint) return currentPos;

    QVector2D basePoint = currentTrackPoint.point;
    QVector2D baseDir = currentTrackPoint.direction;
    
    // Calculate both perpendicular and parallel points
    QVector2D perpDir(-baseDir.y(), baseDir.x());
    QVector2D perpPoint = calculatePerpendicularPoint(currentPos, basePoint, perpDir);
    QVector2D parPoint = calculateParallelPoint(currentPos, basePoint, baseDir);
    
    // Return the closest point to the current position
    float perpDist = (perpPoint - currentPos).length();
    float parDist = (parPoint - currentPos).length();
    
    return (perpDist < parDist) ? perpPoint : parPoint;
}

void GLWidget::drawTrackPoint() const
{
    if (!hasTrackPoint) return;

    // Draw base point
    glPointSize(8.0f);
    glColor4f(1.0f, 1.0f, 0.0f, 0.5f);  // Yellow for base point
    glBegin(GL_POINTS);
    glVertex2f(currentTrackPoint.point.x(), currentTrackPoint.point.y());
    glEnd();

    // Draw direction indicator
    float len = 20.0f / zoom;
    QVector2D end = currentTrackPoint.point + currentTrackPoint.direction * len;
    glBegin(GL_LINES);
    glVertex2f(currentTrackPoint.point.x(), currentTrackPoint.point.y());
    glVertex2f(end.x(), end.y());
    glEnd();
    glPointSize(1.0f);
}

void GLWidget::updateIntersectionPoint(const QVector2D& snapPoint)
{
    // Store the current snap point and its direction
    lastSnapPoints[currentSnapIndex].point = snapPoint;
    lastSnapPoints[currentSnapIndex].direction = (snapPoint - lastSnapPoints[1-currentSnapIndex].point).normalized();
    
    // Calculate intersection if we have two points
    if (currentSnapIndex == 1) {
        tempIntersection.point = calculateIntersection(
            lastSnapPoints[0].point, lastSnapPoints[0].direction,
            lastSnapPoints[1].point, lastSnapPoints[1].direction
        );
        tempIntersection.isValid = true;
    }
    
    // Toggle index between 0 and 1
    currentSnapIndex = 1 - currentSnapIndex;
}

QVector2D GLWidget::calculateIntersection(const QVector2D& p1, const QVector2D& dir1,
                                        const QVector2D& p2, const QVector2D& dir2) const
{
    // Calculate intersection of two lines defined by point and direction
    float x1 = p1.x(), y1 = p1.y();
    float x2 = p1.x() + dir1.x(), y2 = p1.y() + dir1.y();
    float x3 = p2.x(), y3 = p2.y();
    float x4 = p2.x() + dir2.x(), y4 = p2.y() + dir2.y();
    
    float denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (qAbs(denominator) < 0.0001f) {
        return (p1 + p2) * 0.5f; // Return midpoint if lines are parallel
    }
    
    float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denominator;
    return QVector2D(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
}

void GLWidget::updateTrackLine(const QVector2D& snapPoint)
{
    // If both lines not active yet, start first line
    if (!trackLines[0].isActive) {
        trackLines[0].start = snapPoint;
        trackLines[0].end = snapPoint;
        trackLines[0].isActive = true;
        return;
    }

    // If first line active but second isn't, create second line
    if (!trackLines[1].isActive) {
        trackLines[1].start = snapPoint;
        trackLines[1].end = snapPoint;
        trackLines[1].isActive = true;
        
        // Calculate intersection right away
        QVector2D dir1 = (trackLines[0].end - trackLines[0].start).normalized();
        QVector2D dir2 = (snapPoint - snapPoint).normalized();  // Will be updated with mouse movement
        tempIntersection.point = snapPoint;  // Start at snap point
        tempIntersection.isValid = true;
        return;
    }

    // Both lines exist, update end point of current line
    if (currentSnapIndex == 0) {
        trackLines[0].end = snapPoint;
    } else {
        trackLines[1].end = snapPoint;
    }

    // Update intersection if both lines are valid
    QVector2D dir1 = (trackLines[0].end - trackLines[0].start).normalized();
    QVector2D dir2 = (trackLines[1].end - trackLines[1].start).normalized();
    
    if (dir1.lengthSquared() > 0.0001f && dir2.lengthSquared() > 0.0001f) {
        tempIntersection.point = calculateIntersection(
            trackLines[0].start, dir1,
            trackLines[1].start, dir2
        );
        tempIntersection.isValid = true;
    }
}

void GLWidget::drawTrackLines() // Remove const qualifier
{
    glColor4f(0.0f, 0.8f, 0.8f, 0.5f);  // Cyan color for track lines
    glLineWidth(1.0f);

    glBegin(GL_LINES);
    if (trackLines[0].isActive) {
        glVertex2f(trackLines[0].start.x(), trackLines[0].start.y());
        glVertex2f(trackLines[0].end.x(), trackLines[0].end.y());
    }
    if (trackLines[1].isActive) {
        glVertex2f(trackLines[1].start.x(), trackLines[1].start.y());
        glVertex2f(trackLines[1].end.x(), trackLines[1].end.y());
    }
    glEnd();
}

void GLWidget::clearTrackLines()
{
    trackLines[0].isActive = false;
    trackLines[1].isActive = false;
    tempIntersection.isValid = false;
    currentSnapIndex = 0;
}

void GLWidget::updateTracking(const QVector2D& snapPoint)
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // Remove expired tracking points
    trackingPoints.erase(
        std::remove_if(trackingPoints.begin(), trackingPoints.end(),
            [currentTime, this](const TrackingState& ts) {
                return !ts.isActive || 
                       ((currentTime - ts.timestamp) / 1000.0f) > trackingTimeout;
            }),
        trackingPoints.end());
    
    // Add new tracking point
    TrackingState newTrack;
    newTrack.point = snapPoint;
    newTrack.isActive = true;
    newTrack.timestamp = currentTime;
    
    // Determine tracking type
    if (orthoMode) {
        newTrack.type = TrackingState::ORTHO;
        // Calculate nearest 90-degree direction
        QVector2D mousePos = screenToWorld(mapFromGlobal(QCursor::pos()));
        QVector2D dir = (mousePos - snapPoint).normalized();
        float angle = atan2(dir.y(), dir.x());
        angle = round(angle / (M_PI/2)) * (M_PI/2);
        newTrack.direction = QVector2D(cos(angle), sin(angle));
    } else {
        newTrack.type = TrackingState::NORMAL;
        if (!trackingPoints.empty()) {
            newTrack.direction = (snapPoint - trackingPoints.back().point).normalized();
        }
    }
    
    trackingPoints.push_back(newTrack);
}

void GLWidget::drawTrackingLines()
{
    for (const auto& track : trackingPoints) {
        if (!track.isActive) continue;
        
        // Draw track line
        glColor4f(0.0f, 0.8f, 0.8f, 0.3f);  // Cyan, semi-transparent
        glBegin(GL_LINES);
        float len = 1000.0f / zoom;  // Long enough to cross screen
        QVector2D start = track.point - track.direction * len;
        QVector2D end = track.point + track.direction * len;
        glVertex2f(start.x(), start.y());
        glVertex2f(end.x(), end.y());
        glEnd();
        
        // Draw tracking point
        drawTrackingPoint(track);
    }
}

void GLWidget::drawTrackingPoint(const TrackingState& track)
{
    glPointSize(6.0f);
    switch (track.type) {
        case TrackingState::ORTHO:
            glColor4f(0.0f, 1.0f, 0.0f, 0.8f);  // Green
            break;
        case TrackingState::PERP:
            glColor4f(1.0f, 0.5f, 0.0f, 0.8f);  // Orange
            break;
        case TrackingState::PARALLEL:
            glColor4f(0.0f, 0.8f, 1.0f, 0.8f);  // Light blue
            break;
        default:
            glColor4f(1.0f, 1.0f, 0.0f, 0.8f);  // Yellow
    }
    
    glBegin(GL_POINTS);
    glVertex2f(track.point.x(), track.point.y());
    glEnd();
    glPointSize(1.0f);
}

QVector2D GLWidget::findTrackingIntersection(const QVector2D& mousePos)
{
    if (trackingPoints.size() < 2) return mousePos;
    
    // Find two nearest tracking lines
    float minDist1 = std::numeric_limits<float>::max();
    float minDist2 = std::numeric_limits<float>::max();
    const TrackingState* track1 = nullptr;
    const TrackingState* track2 = nullptr;
    
    for (const auto& track : trackingPoints) {
        if (!track.isActive) continue;
        
        float dist = calculateDistanceToLine(mousePos, track.point, track.direction);
        if (dist < minDist1) {
            minDist2 = minDist1;
            track2 = track1;
            minDist1 = dist;
            track1 = &track;
        } else if (dist < minDist2) {
            minDist2 = dist;
            track2 = &track;
        }
    }
    
    if (track1 && track2) {
        return calculateIntersection(
            track1->point, track1->direction,
            track2->point, track2->direction
        );
    }
    
    return mousePos;
}

void GLWidget::handleShiftSnap(const QVector2D& point) 
{
    if (currentShiftSnap < 2) {
        // Store new snap point
        shiftSnaps[currentShiftSnap].point = point;
        shiftSnaps[currentShiftSnap].isActive = true;

        if (currentShiftSnap == 1) {
            // Calculate direction between points
            QVector2D dir = (point - shiftSnaps[0].point).normalized();
            shiftSnaps[0].direction = dir;
            shiftSnaps[1].direction = dir;

            // Calculate intersection and create temp point
            tempIntersection.point = point;
            tempIntersection.isValid = true;
        }
        
        currentShiftSnap++;
        update();
    }
}

void GLWidget::clearShiftSnaps()
{
    shiftSnaps[0].isActive = false;
    shiftSnaps[1].isActive = false;
    currentShiftSnap = 0;
    isShiftSnapping = false;
    tempIntersection.isValid = false;
    update();
}

void GLWidget::drawShiftSnapLines()
{
    if (!shiftSnaps[0].isActive) return;

    // Draw first snap point
    glPointSize(8.0f);
    glColor4f(1.0f, 1.0f, 0.0f, 0.8f);  // Yellow
    glBegin(GL_POINTS);
    glVertex2f(shiftSnaps[0].point.x(), shiftSnaps[0].point.y());
    glEnd();

    if (shiftSnaps[1].isActive) {
        // Draw second snap point
        glVertex2f(shiftSnaps[1].point.x(), shiftSnaps[1].point.y());
        
        // Draw line between points
        glColor4f(0.0f, 1.0f, 1.0f, 0.5f);  // Cyan
        glBegin(GL_LINES);
        glVertex2f(shiftSnaps[0].point.x(), shiftSnaps[0].point.y());
        glVertex2f(shiftSnaps[1].point.x(), shiftSnaps[1].point.y());
        glEnd();

        // Draw intersection point
        if (tempIntersection.isValid) {
            glPointSize(10.0f);
            glColor4f(1.0f, 0.0f, 1.0f, 0.8f);  // Magenta
            glBegin(GL_POINTS);
            glVertex2f(tempIntersection.point.x(), tempIntersection.point.y());
            glEnd();
        }
    }
    glPointSize(1.0f);
}

// ...rest of existing code...
