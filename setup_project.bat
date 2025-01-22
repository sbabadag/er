:: filepath: /c:/RN/OGL/setup_project.bat
@echo off
mkdir include
mkdir src
mkdir lib
mkdir bin
mkdir build
mkdir build\obj
mkdir build\moc
mkdir build\ui
mkdir build\rcc

:: Move header files
move *.h include\

:: Move source files
move *.cpp src\

:: Update status
echo Directory structure created and files moved.
echo Running qmake...
qmake
echo Done.