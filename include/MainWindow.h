#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStatusBar>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include "GLWidget.h"  // Add this include

class GLWidget;
class QToolBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNew();
    void onSaveDxf();
    void onLoadDxf();
    void onStartLineDrawing();
    void onStartDimensioning();
    void onStartMove();
    void onZoomAll();
    void showColorDialog();

private:
    void createActions();
    void createMenus();
    void createToolbars();

    GLWidget* glWidget;
    QAction* colorAction;
    QToolBar* drawingToolbar;
    
    // Add tool button members
    QToolButton* lineButton;
    QToolButton* moveButton;
    QToolButton* deleteButton;
    QToolButton* dimensionButton;
};

#endif // MAINWINDOW_H
