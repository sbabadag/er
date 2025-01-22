:: filepath: /c:/RN/OGL/clean.bat
@echo off
mingw32-make clean
del Makefile*
rmdir /s /q build
rmdir /s /q bin
echo Clean complete.