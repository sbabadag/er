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
    , isDrawing(false)
    , hasFirstPoint(false)
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
    , isMoving(false)
    , isAwaitingMoveStartPoint(false)  // Match header order
    , isDragging(false)                // Match header order
    , isSelectingRectangle(false)
    , isAwaitingMoveEndPoint(false)   // Initialize new variable
    , isZooming(false)               // Initialize zoom state
    , zoomStartPos(QPoint())         // Initialize zoom start position
    , zoomSensitivity(0.005f)        // Set zoom sensitivity
    , currentColor(Qt::white)  // Initialize current color
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // Enable key events

    // Initialize SnapManager
    snapManager = new SnapManager(snapThreshold, zoom, lines);
    snapManager->updateSettings(snapThreshold, zoom, lines);  // Ensure initial settings are applied
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
}

QVector2D GLWidget::snapPoint(const QVector2D& point)
{
    // Convert the point to world space for consistent snapping
    QVector2D worldPoint = point;
    
    // Update snap system with current settings
    snapManager->updateSettings(snapThreshold, zoom, lines);
    snapManager->updateSnap(worldPoint);
    
    if (snapManager->isSnapActive()) {
        // Get snap point in world coordinates
        QVector2D snapPoint = snapManager->getCurrentSnapPoint();
        return snapPoint;
    }
    
    return point;
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
    QVector2D worldPos = screenToWorld(event->pos());
    QVector2D snappedPos = snapPoint(worldPos);

    if (event->button() == Qt::MiddleButton) {
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
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
    else if (event->key() == Qt::Key_Escape) {
        cancelDrawing();
    }
    update();
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
    if (currentMode == MODE_MOVE) {
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
    } else {
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

    emit commandChanged(currentCommand); // Ensure signal is emitted
    updateCommandStatus();
}

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
        // Check if the line intersects with the selection rectangle
        // Using Cohen-Sutherland algorithm or any line-rectangle intersection method

        // Define the four edges of the rectangle
        QVector2D rectTopLeft = topLeft;
        QVector2D rectTopRight = topRight;
        QVector2D rectBottomRight = bottomRight;
        QVector2D rectBottomLeft = bottomLeft;

        // Function to check if two line segments intersect
        auto linesIntersect = [](const QVector2D& p1, const QVector2D& p2,
                                const QVector2D& q1, const QVector2D& q2) -> bool {
            auto orientation = [&](const QVector2D& a, const QVector2D& b, const QVector2D& c) -> int {
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
        };

        bool intersects = false;
        // Check intersection with all four edges of the rectangle
        intersects |= linesIntersect(line.start, line.end, rectTopLeft, rectTopRight);
        intersects |= linesIntersect(line.start, line.end, rectTopRight, rectBottomRight);
        intersects |= linesIntersect(line.start, line.end, rectBottomRight, rectBottomLeft);
        intersects |= linesIntersect(line.start, line.end, rectBottomLeft, rectTopLeft);

        // Additionally, check if the entire line is inside the rectangle
        bool inside = worldRect.contains(QPointF(line.start.x(), line.start.y())) &&
                      worldRect.contains(QPointF(line.end.x(), line.end.y()));

        if (intersects || inside) {
            selectedObjectIndices.push_back(static_cast<int>(i));
        }
    }

    if (!selectedObjectIndices.empty()) {
        objectSelected = true;
        selectedObjectIndex = selectedObjectIndices[0];  // Highlight the first selected object
    }
    else {
        objectSelected = false;
        selectedObjectIndex = -1;
    }
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