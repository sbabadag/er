#include "../include/MainWindow.h"
#include "../include/GLWidget.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QPushButton>
#include <QToolButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , glWidget(new GLWidget(this))
    , colorAction(nullptr)
    , drawingToolbar(nullptr)
{
    setCentralWidget(glWidget);
    
    // Create status bar
    QStatusBar* statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    glWidget->setStatusBar(statusBar);

    createActions();    // Add this first
    createMenus();
    createToolbars();  // Add toolbar creation - this now handles all connections
    // Remove setupConnections() call
}

MainWindow::~MainWindow()
{
}

void MainWindow::createMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    
    // Add New action at the start of File menu
    QAction* newAction = fileMenu->addAction(tr("&New"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::onNew);
    
    fileMenu->addSeparator();
    
    QAction* saveDxfAction = fileMenu->addAction(tr("&Save DXF..."));
    saveDxfAction->setShortcut(QKeySequence::Save);
    connect(saveDxfAction, &QAction::triggered, this, &MainWindow::onSaveDxf);

    QAction* loadDxfAction = fileMenu->addAction(tr("&Load DXF..."));
    loadDxfAction->setShortcut(QKeySequence::Open);
    connect(loadDxfAction, &QAction::triggered, this, &MainWindow::onLoadDxf);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(tr("Alt+F4"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createToolbars()
{
    drawingToolbar = addToolBar(tr("Drawing"));
    
    // Add New button at start of toolbar
    QAction* newAction = drawingToolbar->addAction(tr("New"));
    newAction->setStatusTip(tr("Clear all and start new drawing"));
    connect(newAction, &QAction::triggered, this, &MainWindow::onNew);
    
    drawingToolbar->addSeparator();
    
    // Line tool
    lineButton = new QToolButton(this);
    lineButton->setText(tr("Line"));
    lineButton->setToolTip(tr("Draw Line"));
    drawingToolbar->addWidget(lineButton);
    connect(lineButton, &QToolButton::clicked, this, &MainWindow::onStartLineDrawing);
    
    // Dimension tool
    dimensionButton = new QToolButton(this);
    dimensionButton->setText(tr("Dimension"));
    dimensionButton->setToolTip(tr("Add Dimension"));
    drawingToolbar->addWidget(dimensionButton);
    connect(dimensionButton, &QToolButton::clicked, this, &MainWindow::onStartDimensioning);
    
    drawingToolbar->addSeparator();
    
    // Move tool
    moveButton = new QToolButton(this);
    moveButton->setText(tr("Move"));
    moveButton->setToolTip(tr("Move Objects"));
    drawingToolbar->addWidget(moveButton);
    connect(moveButton, &QToolButton::clicked, this, &MainWindow::onStartMove);
    
    // Delete tool
    deleteButton = new QToolButton(this);
    deleteButton->setText(tr("Delete"));
    deleteButton->setToolTip(tr("Delete Objects"));
    drawingToolbar->addWidget(deleteButton);
    connect(deleteButton, &QToolButton::clicked, glWidget, &GLWidget::startDeleteMode);
    
    drawingToolbar->addSeparator();
    
    // Set up tool buttons in GLWidget
    glWidget->setToolButtons(lineButton, moveButton, deleteButton, dimensionButton);
}

void MainWindow::onSaveDxf()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save DXF"), "",
        tr("DXF Files (*.dxf);;All Files (*)"));
        
    if (filename.isEmpty())
        return;

    if (!filename.endsWith(".dxf", Qt::CaseInsensitive))
        filename += ".dxf";

    if (!glWidget->saveDxf(filename)) {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save the file."));
    }
}

void MainWindow::onLoadDxf()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Open DXF"), "",
        tr("DXF Files (*.dxf);;All Files (*)"));
        
    if (filename.isEmpty())
        return;

    if (!glWidget->loadDxf(filename)) {
        QMessageBox::warning(this, tr("Load Error"),
            tr("Could not load the file."));
    }
}

void MainWindow::onStartLineDrawing()
{
    glWidget->startLineDrawing();
}

void MainWindow::onStartDimensioning()
{
    glWidget->startDimensionDrawing();
}

void MainWindow::onStartMove()
{
    glWidget->setCurrentMode(GLWidget::MODE_MOVE);
}

void MainWindow::onZoomAll()
{
    glWidget->zoomAll();
}

void MainWindow::onNew()
{
    // Optional: Add confirmation dialog
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("New Drawing"),
        tr("Are you sure you want to clear everything and start a new drawing?"),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        glWidget->resetAll();
    }
}

void MainWindow::createActions()
{
    // Add color selection action
    colorAction = new QAction(tr("&Color..."), this);
    colorAction->setStatusTip(tr("Change line color"));
    connect(colorAction, &QAction::triggered, this, &MainWindow::showColorDialog);
    
    // Add to appropriate menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(colorAction);
}

void MainWindow::showColorDialog()
{
    QColor color = QColorDialog::getColor(glWidget->getCurrentColor(), this);
    if (color.isValid()) {
        if (glWidget->isObjectSelected()) {
            // Change color of selected objects
            glWidget->setSelectedObjectsColor(color);
        } else {
            // Set current color for new lines
            glWidget->setCurrentColor(color);
        }
    }
}
