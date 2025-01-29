#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector2D>
#include <QStatusBar>
#include <vector>
#include <QString>
#include "Line.h"  // Include Line struct
#include "DxfHandler.h"  // Add this include
#include "GhostTracker.h"  // Add this include

class SnapManager;  // Forward declare SnapManager

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    GLWidget(QWidget* parent = nullptr);
    virtual ~GLWidget();  // Add virtual destructor
    void addLine(const QVector2D& start, const QVector2D& end);
    void setStatusBar(QStatusBar* statusBar);
    void startLineDrawing();
    void startDimensionDrawing();  // Already public
    void cancelDrawing();

    // Add selection and movement methods
    void selectObjectAt(const QVector2D& point);
    void deselectObject();
    bool isObjectSelected() const { return objectSelected; }
    void moveSelectedObject(const QVector2D& delta);

    // Getter for selected objects
    const std::vector<int>& getSelectedObjects() const { return selectedObjectIndices; }

    // Drawing modes
    enum DrawMode {
        MODE_NONE,
        MODE_LINE,
        MODE_DIMENSION,
        MODE_MOVE  // Add MODE_MOVE
    };

    // Setter for currentMode
    void setCurrentMode(DrawMode mode);

    // Setter for currentCommand
    void setCurrentCommand(const QString& command);

    // Method to trigger updateCommandStatus
    void triggerUpdateCommandStatus();

    void zoomAll();  // Ensure zoomAll is declared as public

    bool saveDxf(const QString& filename);
    bool loadDxf(const QString& filename);

    void resetAll();  // Add this new method

    // Add color selection methods
    void setCurrentColor(const QColor& color) { currentColor = color; }
    QColor getCurrentColor() const { return currentColor; }
    void setSelectedObjectsColor(const QColor& color);

signals:
    void commandChanged(const QString& newCommand);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QVector2D screenToWorld(const QPoint& screenPos);
    QVector2D worldToScreen(const QVector2D& worldPos);
    void updateCommandStatus();
    void updateCoordinates(const QVector2D& worldPos);
    QVector2D constrainToOrtho(const QVector2D& start, const QVector2D& end);

    QVector2D snapPoint(const QVector2D& point);          // Declare snapPoint
    QVector2D findMidPoint(const QVector2D& point);       // Declare findMidPoint
    // QVector2D findPerpPoint(const QVector2D& point);   // Remove findPerpPoint

    struct Dimension {
        QVector2D start;
        QVector2D end;
        float measurement;
        QString text;
        float offset;  // Add offset for dimension line position
    };

    // Drawing state
    bool isDrawing;
    bool hasFirstPoint;
    bool lineToolActive;
    QVector2D firstPoint;
    QVector2D currentStart;
    QVector2D currentEnd;  // Moved before currentMode
    std::vector<Line> lines;
    DrawMode currentMode;  // Single declaration here

    std::vector<Dimension> dimensions;
    void drawDimension(const Dimension& dim);
    void addDimension(const QVector2D& start, const QVector2D& end, float offset);  // Updated signature

    // Modify the SnapManager pointer to reflect the updated constructor if needed
    SnapManager* snapManager;

    void renderText(float x, float y, const QString& text);  // Add renderText declaration

    // View state
    QVector2D pan;
    float zoom;
    QPoint lastMousePos;
    float snapThreshold;

    // Command bar state
    QStatusBar* m_statusBar;
    QString currentCommand;

    // Constraint state
    bool orthoMode;

    // Length constraint state
    float targetLength;
    QString lengthInput;
    bool hasLengthConstraint;

    // Dimension state
    bool placingDimension;
    QVector2D dimStart;
    QVector2D dimEnd;
    float currentDimOffset;

    void applyLengthConstraint(QVector2D& end);
    void processNumericInput(const QString& key);
    void resetDrawingState();

    // Selection state
    bool objectSelected;
    int selectedObjectIndex;  // Index of the selected line in 'lines' vector

    // Move operation state
    bool isMoving;

    // Rectangle selection state
    bool isSelectingRectangle; // Moved below hasMoveEnd

    // Selection rectangle positions
    QPoint selectionStartPos;    // Declare selection start position
    QPoint selectionEndPos;      // Declare selection end position
    QRect selectionRect;         // Declare selection rectangle

    // List of selected object indices
    std::vector<int> selectedObjectIndices;

    // Method to perform rectangle selection
    void performRectangleSelection(const QRect& rect);

    // Add the following declarations for two-step move
    bool isAwaitingMoveFinalPoint; // Indicates if waiting for the final point
    QVector2D moveHoldPoint;        // Holds the initial point for movement

    // Add new declarations for enhanced move workflow
    bool isAwaitingMoveStartPoint; // Indicates if waiting for move start point
    bool isAwaitingMoveEndPoint;   // Indicates if waiting for move end point
    QVector2D moveStartPoint;      // Holds the move start point

    // Add new declarations for zoom mode
    bool isZooming;               // Indicates if zoom mode is active
    QPoint zoomStartPos;          // Stores the initial mouse position for zoom
    float zoomSensitivity;        // Determines how sensitive the zoom is to mouse movement

    // Add ghost tracking support
    GhostTracker ghostTracker;
    void renderGhostObjects();

    // Add current color property
    QColor currentColor;

    bool isDragging;  // Track mouse dragging state
};
