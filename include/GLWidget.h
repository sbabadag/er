#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector2D>
#include <QStatusBar>
#include <vector>
#include <QString>
#include <QColor>
#include <QPushButton>
#include <QToolButton>  // Add QToolButton include
#include <QTimer>
#include <QDateTime>  // Add QDateTime include
#include "Line.h"
#include "DxfHandler.h"
#include "GhostTracker.h"

class SnapManager;  // Forward declare SnapManager

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLWidget(QWidget* parent = nullptr);
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
        MODE_MOVE,  // Add MODE_MOVE
        MODE_DELETE  // Add delete mode
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

    // Add method to set tool buttons
    void setToolButtons(QToolButton* line, QToolButton* move, 
                       QToolButton* del, QToolButton* dimension);

signals:
    void commandChanged(const QString& newCommand);

public slots:
    void deleteSelectedObjects();
    void startDeleteMode();  // Add new slot

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;  // Add keyReleaseEvent

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
    bool hasFirstPoint;
    bool isSelectingRectangle;
    bool isDragging;
    bool isCrossingSelection;
    bool isDrawing;
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
    // bool isSelectingRectangle; // Moved below hasMoveEnd

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

    // bool isDragging;  // Track mouse dragging state
    // bool isCrossingSelection;  // Add this member variable

    // Add helper function for line intersection
    bool linesIntersect(const QVector2D& p1, const QVector2D& p2,
                       const QVector2D& q1, const QVector2D& q2) const;

    QString modeToString() const;  // Add helper function

    // Tool button pointers
    QToolButton* lineButton = nullptr;
    QToolButton* moveButton = nullptr;
    QToolButton* deleteButton = nullptr;
    QToolButton* dimensionButton = nullptr;

    // Add temporary point storage
    QVector2D tempPoint;
    bool hasTempPoint = false;
    float tempPointLifetime = 0.0f;  // Time in seconds the temp point should remain

    // Function to clear temporary point
    void clearTempPoint();

    // Snap point history and construction
    struct SnapHistory {
        QVector2D point;
        QVector2D direction;  // Store direction for perpendicular calculations
        float timestamp;
        bool isActive;
    };

    SnapHistory lastSnap = {QVector2D(0,0), QVector2D(0,0), 0.0f, false};
    QVector2D tempConstructPoint;  // Store construction/perpendicular point
    bool hasTempConstructPoint = false;
    float snapHistoryTimeout = 2.0f;  // Seconds to keep snap history

    void updateSnapHistory(const QVector2D& snapPoint, const QVector2D& dir);
    void clearSnapHistory();
    QVector2D calculatePerpendicularPoint(const QVector2D& base, const QVector2D& ref, const QVector2D& dir) const;
    QVector2D calculateParallelPoint(const QVector2D& base, const QVector2D& ref, const QVector2D& dir) const;
    
    // Timer for managing snap history
    QTimer* snapHistoryTimer;

    // Track point system - consolidated declarations
    struct TrackPoint {
        QVector2D point;
        QVector2D direction;
        qint64 timestamp;
        bool isActive = false;
        QVector2D reference;
        bool isBase = false;
        enum Type {
            SNAP,
            TRACK,
            PARALLEL,
            PERP
        } type = SNAP;
    };

    // Single instance of tracking variables
    std::vector<TrackPoint> trackPoints;
    TrackPoint currentTrackPoint;
    TrackPoint lastTrackPoint;
    bool hasTrackPoint = false;
    bool hasActiveTracking = false;
    static constexpr float trackTimeout = 2.0f;
    static constexpr float trackSnapThreshold = 10.0f;
    QTimer* trackTimer = nullptr;

    // Track point methods
    void setTrackPoint(const QVector2D& point, const QVector2D& dir);
    void clearTrackPoint();
    void drawTrackPoint() const;
    QVector2D getConstructionPoint(const QVector2D& currentPos) const;
    void addTrackPoint(const QVector2D& point, const QVector2D& dir, TrackPoint::Type type);
    void updateTrackPoints();
    void drawTrackPoints() const;
    void updateTrackingPoint(const QVector2D& point);
    void clearTrackingPoints();
    bool isNearTrackPoint(const QVector2D& point, const TrackPoint& track) const;
    QVector2D snapToTracking(const QVector2D& point) const;

    // Add intersection tracking
    struct IntersectionPoint {
        QVector2D point;
        bool isValid;
    };
    
    IntersectionPoint tempIntersection;
    void updateIntersectionPoint(const QVector2D& snapPoint);
    QVector2D calculateIntersection(const QVector2D& p1, const QVector2D& dir1,
                                  const QVector2D& p2, const QVector2D& dir2) const;
    
    // Track last two snap points for intersection
    struct SnapPoint {
        QVector2D point;
        QVector2D direction;
    };
    SnapPoint lastSnapPoints[2];
    int currentSnapIndex = 0;

    struct TrackLine {
        QVector2D start;
        QVector2D end;
        bool isActive;
    };
    
    // Track lines for intersection
    TrackLine trackLines[2];
    void updateTrackLine(const QVector2D& snapPoint);
    void drawTrackLines(); // Remove const qualifier
    void clearTrackLines();

    // Track points storage for temporary construction
    struct TrackingState {
        QVector2D point;      // Base point for tracking
        QVector2D direction;  // Track line direction
        bool isActive;
        qint64 timestamp;  // Add timestamp field
        enum Type {
            NORMAL,     // Standard tracking
            ORTHO,      // Orthogonal tracking (0, 90, 180, 270)
            PERP,       // Perpendicular tracking
            PARALLEL    // Parallel tracking
        } type;
    };
    
    std::vector<TrackingState> trackingPoints;  // Store multiple tracking points
    float trackingTimeout = 5.0f;  // How long tracking points remain active
    
    void updateTracking(const QVector2D& snapPoint);
    void drawTrackingLines();
    void clearTracking();
    QVector2D findTrackingIntersection(const QVector2D& mousePos);
    bool checkPerpendicularTracking(const QVector2D& point, const QVector2D& line);
    bool checkParallelTracking(const QVector2D& point, const QVector2D& line);
    
    // Visual feedback
    void drawTrackingPoint(const TrackingState& track);
    void drawConstructionLines();

    // Add helper function for distance calculation
    float calculateDistanceToLine(const QVector2D& point, const QVector2D& linePoint, 
                                const QVector2D& lineDir) const {
        QVector2D v = point - linePoint;
        float proj = QVector2D::dotProduct(v, lineDir);
        QVector2D projPoint = linePoint + lineDir * proj;
        return (point - projPoint).length();
    }

    // Add shift-snap tracking
    struct ShiftSnapPoint {
        QVector2D point;
        bool isActive;
        QVector2D direction;  // Direction from previous point
    };
    
    ShiftSnapPoint shiftSnaps[2];  // Store two shift-snap points
    int currentShiftSnap = 0;      // Track which point we're on
    bool isShiftSnapping = false;   // Track if shift is being held

    void handleShiftSnap(const QVector2D& point);
    void clearShiftSnaps();
    void drawShiftSnapLines();
};

#endif // GLWIDGET_H
