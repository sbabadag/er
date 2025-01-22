@echo off
setlocal

REM Set up Qt environment
set QTDIR=C:\Qt\6.8.1\mingw_64
set PATH=%QTDIR%\bin;%PATH%

REM Clean and create bin directory
if exist bin (rd /s /q bin)
mkdir bin
mkdir bin\platforms

REM Build commands
qmake
mingw32-make clean
mingw32-make

if errorlevel 1 (
    echo Build failed
    pause
    exit /b 1
)

REM Copy required DLLs to bin directory
copy /Y "%QTDIR%\bin\Qt6Core.dll" bin\
copy /Y "%QTDIR%\bin\Qt6Gui.dll" bin\
copy /Y "%QTDIR%\bin\Qt6Widgets.dll" bin\
copy /Y "%QTDIR%\bin\Qt6OpenGL.dll" bin\
copy /Y "%QTDIR%\bin\Qt6OpenGLWidgets.dll" bin\

REM Copy platform plugins
copy /Y "%QTDIR%\plugins\platforms\qwindows.dll" bin\platforms\
copy /Y "%QTDIR%\bin\Qt6Network.dll" bin\
copy /Y "%QTDIR%\bin\Qt6DBus.dll" bin\

echo Build successful

REM Run from bin directory
cd bin
start OGL.exe
cd ..
pause

:: filepath: /c:/RN/OGL/build.bat
@echo off
qmake
mingw32-make
echo Build complete.
if exist bin\OGL.exe (
    echo Running application...
    start bin\OGL.exe
) else (
    echo Build failed or executable not found.
    pause
)
