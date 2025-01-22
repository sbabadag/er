#pragma once
#include <QMainWindow>
#include <QStatusBar>

class GLWidget;
class QToolBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNew();  // Add this
    void onSaveDxf();
    void onLoadDxf();
    void onStartLineDrawing();    // Add this
    void onStartDimensioning();   // Add this
    void onStartMove();          // Add this
    void onZoomAll();           // Add this
    void showColorDialog();

private:
    void createMenus();
    void createToolbars();   // Add this
    void createActions();    // Add this declaration
    GLWidget* glWidget;
    QToolBar* drawingToolbar;
    QAction* colorAction;
};
