@echo off
setlocal EnableDelayedExpansion

REM Verify Qt installation with quoted paths
set "QTDIR=C:\Qt\6.8.1\mingw_64"
if not exist "!QTDIR!" (
    echo Error: Qt directory not found at !QTDIR!
    pause
    goto end_script
)

REM Clean PATH but preserve system directories for tools
set "OLD_PATH=%PATH%"
set "SYS_PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;%ProgramFiles%\Git\cmd;%LOCALAPPDATA%\Microsoft\WindowsApps;%APPDATA%\npm"
set "PATH="
set "PATH=!QTDIR!\bin;C:\Qt\Tools\mingw1310_64\bin;!SYS_PATH!"

REM Verify tools with proper path quoting
where /q "qmake.exe" || (
    echo Error: qmake.exe not found in PATH
    pause
    goto end_script
)

where /q "mingw32-make.exe" || (
    echo Error: mingw32-make.exe not found in PATH
    pause
    goto end_script
)

REM Setup Python path with more comprehensive fallbacks
echo Searching for Python...
for %%P in (
    python.exe
    py.exe
    "C:\Python39\python.exe"
    "C:\Program Files\Python39\python.exe"
    "C:\Program Files\Python311\python.exe"
    "C:\Program Files\Python312\python.exe"
    "C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python39\python.exe"
    "C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python311\python.exe"
    "C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python312\python.exe"
    "C:\Users\%USERNAME%\AppData\Local\Microsoft\WindowsApps\python.exe"
    "C:\Users\%USERNAME%\AppData\Local\Microsoft\WindowsApps\python3.exe"
) do (
    echo Trying: %%P
    "%%P" --version >nul 2>&1 && (
        set "PYTHON_PATH=%%P"
        echo Found Python at: %%P
        goto python_found
    )
)

echo Checking Windows Store Python...
where /Q python || where /Q python3 || (
    echo Error: Python not found in PATH or standard locations.
    echo Please install Python 3.x from python.org or Microsoft Store
    pause
    goto end_script
)
set "PYTHON_PATH=python"
echo Found Python in PATH

:python_found
echo Testing Python functionality...
"!PYTHON_PATH!" -c "print('Python test successful')" || (
    echo Error: Python found but not working properly
    pause 
    goto end_script
)

REM Verify Python version
for /f "tokens=2" %%V in ('!PYTHON_PATH! -V 2^>^&1') do (
    echo Using Python version: %%V
)

REM Create clean build environment
if exist "bin" rd /s /q "bin"
mkdir "bin" "bin\platforms"

REM Create error handler script (single instance)
echo import sys, json, re > error_handler.py
echo. >> error_handler.py
echo def parse_compiler_error(line): >> error_handler.py
echo     pattern = re.compile(r'(.*?):(\d+):(\d+):\s*(error^|warning^|note):\s*(.*)') >> error_handler.py
echo     match = pattern.search(line) >> error_handler.py
echo     if match and match.group(4).lower() == 'error': >> error_handler.py
echo         return { >> error_handler.py
echo             'file': match.group(1).strip(), >> error_handler.py
echo             'line': int(match.group(2)), >> error_handler.py
echo             'message': match.group(5).strip() >> error_handler.py
echo         } >> error_handler.py
echo     return None >> error_handler.py
echo. >> error_handler.py
echo if __name__ == '__main__': >> error_handler.py
echo     errors = [] >> error_handler.py
echo     for line in sys.stdin: >> error_handler.py
echo         error = parse_compiler_error(line) >> error_handler.py
echo         if error: >> error_handler.py
echo             errors.append(error) >> error_handler.py
echo     with open('build_errors.json', 'w') as f: >> error_handler.py
echo         json.dump(errors, f, indent=2) >> error_handler.py
echo     for error in errors: >> error_handler.py
echo         print(f"{error['file']}||{error['line']}||{error['message']}") >> error_handler.py

REM Fix Node.js detection by including common installation paths
set "NODE_PATHS=C:\Program Files\nodejs;C:\Program Files (x86)\nodejs;%APPDATA%\npm"
set "PATH=%PATH%;%NODE_PATHS%"

echo Checking for Node.js installation...
for %%P in (
    "C:\Program Files\nodejs\node.exe"
    "C:\Program Files (x86)\nodejs\node.exe"
    "%APPDATA%\Local\Programs\node\node.exe"
    "%ProgramFiles%\nodejs\node.exe"
    "%ProgramFiles(x86)%\nodejs\node.exe"
) do (
    if exist "%%~P" (
        set "NODE_PATH=%%~P"
        echo Found Node.js at: %%~P
        goto node_found
    )
)

REM Try PATH-based detection as fallback
where node >nul 2>&1 && (
    for /f "tokens=*" %%P in ('where node') do (
        set "NODE_PATH=%%P"
        echo Found Node.js in PATH: %%P
        goto node_found
    )
)

echo Node.js is not installed.
echo Please download and install Node.js from https://nodejs.org/
start https://nodejs.org/
pause
goto end_script

:node_found
echo Testing Node.js installation...
"!NODE_PATH!" -v > nul 2>&1 || (
    echo Node.js found but not working properly
    pause
    goto end_script
)

for /f "tokens=*" %%V in ('"!NODE_PATH!" -v') do (
    echo Using Node.js version: %%V
)

REM Setup npm path
set "NPM_PATH=%APPDATA%\npm"
set "PATH=%PATH%;!NPM_PATH!"

echo Checking npm installation...
where npm >nul 2>&1 || (
    echo npm not found in PATH. Adding npm directory...
    if exist "!NODE_PATH!\..\npm.cmd" (
        set "PATH=!PATH!;!NODE_PATH!\.."
    )
)

REM Verify npm is working
call npm -v >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    echo npm installation verification failed
    echo Please repair Node.js installation
    pause
    goto end_script
)

for /f "tokens=*" %%V in ('npm -v') do (
    echo Using npm version: %%V
)

REM Check for GitHub CLI with system PATH
echo Checking for GitHub CLI...
set "GH_FOUND=0"
for %%P in ("%ProgramFiles%\GitHub CLI" "%LOCALAPPDATA%\GitHub CLI") do (
    if exist "%%~P\gh.exe" (
        set "PATH=!PATH!;%%~P"
        set "GH_FOUND=1"
        echo Found GitHub CLI at: %%~P\gh.exe
    )
)

if "!GH_FOUND!"=="0" (
    REM Try system PATH as fallback
    where gh.exe >nul 2>&1 && (
        set "GH_FOUND=1"
        for /f "tokens=*" %%P in ('where gh.exe') do (
            echo Found GitHub CLI at: %%P
        )
    )
)

if "!GH_FOUND!"=="0" (
    echo Error: GitHub CLI not found
    goto end_script
)

gh --version 2>nul | find "gh version" >nul
if !ERRORLEVEL! EQU 0 (
    for /f "tokens=*" %%v in ('gh --version') do echo %%v
    gh auth status >nul 2>&1 && (
        echo GitHub CLI authentication verified
    ) || (
        echo Error: GitHub CLI not authenticated
        echo Please run: gh auth login
        goto end_script
    )
)

REM Setup GitHub Copilot with extension check
set "USE_COPILOT=0"
echo Checking for GitHub Copilot...
gh extension list | find "copilot" >nul
if !ERRORLEVEL! EQU 0 (
    set "USE_COPILOT=1"
    echo Found GitHub Copilot extension
    call gh copilot explain "test" >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        echo Copilot extension is working
    ) else (
        echo Warning: Copilot extension found but not working
        echo Try reinstalling with: gh extension remove copilot ^&^& gh extension install github/gh-copilot
        set "USE_COPILOT=0"
    )
) else (
    echo Installing GitHub Copilot extension...
    call gh extension install github/gh-copilot
    if !ERRORLEVEL! EQU 0 (
        set "USE_COPILOT=1"
        echo Copilot extension installed successfully
    ) else (
        echo Failed to install Copilot extension
        set "USE_COPILOT=0"
    )
)

REM Create fix application script with proper syntax
echo import json, sys > apply_fixes.py
echo. >> apply_fixes.py
echo def apply_fix(file, line, fix): >> apply_fixes.py
echo     try: >> apply_fixes.py
echo         with open(file, 'r', encoding='utf-8') as f: >> apply_fixes.py
echo             lines = f.readlines() >> apply_fixes.py
echo         if 1 ^<= line ^<= len(lines): >> apply_fixes.py
echo             lines[line-1] = fix.strip() + '\n' >> apply_fixes.py
echo             with open(file, 'w', encoding='utf-8') as f: >> apply_fixes.py
echo                 f.writelines(lines) >> apply_fixes.py
echo             return True >> apply_fixes.py
echo         return False >> apply_fixes.py
echo     except Exception as e: >> apply_fixes.py
echo         print(f"Error applying fix: {e}") >> apply_fixes.py
echo         return False >> apply_fixes.py
echo. >> apply_fixes.py
echo def main(): >> apply_fixes.py
echo     try: >> apply_fixes.py
echo         with open('fixes.json', 'r', encoding='utf-8') as f: >> apply_fixes.py
echo             fixes = json.load(f) >> apply_fixes.py
echo         success = True >> apply_fixes.py
echo         for fix in fixes: >> apply_fixes.py
echo             if not apply_fix(fix['file'], fix['line'], fix['code']): >> apply_fixes.py
echo                 print(f"Failed to apply fix to {fix['file']} line {fix['line']}") >> apply_fixes.py
echo                 success = False >> apply_fixes.py
echo         return 0 if success else 1 >> apply_fixes.py
echo     except Exception as e: >> apply_fixes.py
echo         print(f"Error in main: {e}") >> apply_fixes.py
echo         return 1 >> apply_fixes.py
echo. >> apply_fixes.py
echo if __name__ == '__main__': >> apply_fixes.py
echo     sys.exit(main()) >> apply_fixes.py

:build_process
:build_loop
echo Building project...

REM Clean previous build artifacts
if exist "Makefile*" del /f /q Makefile*
if exist "*.o" del /f /q *.o

REM Run qmake and make with proper quoting
echo Running qmake...
"qmake.exe" -v
for %%F in (*.pro) do (
    echo Processing: %%F
    "qmake.exe" "%%F" || (
        echo Error: qmake failed for %%F
        pause
        goto end_script
    )
)

echo Running make...
"mingw32-make.exe" clean 2>nul
"mingw32-make.exe" -j%NUMBER_OF_PROCESSORS% 2> build_errors.txt
set "BUILD_ERROR=%ERRORLEVEL%"

if !BUILD_ERROR! NEQ 0 (
    echo Build failed with error code !BUILD_ERROR!
    type build_errors.txt
    
    REM Process errors with Copilot
    if !USE_COPILOT! EQU 1 (
        echo Processing errors with GitHub Copilot...
        set "FIXES_FOUND=0"
        echo [] > fixes.json
        
        type build_errors.txt | "!PYTHON_PATH!" error_handler.py > errors_formatted.txt
        if exist "errors_formatted.txt" (
            for /f "tokens=1,2,3 delims=||" %%a in (errors_formatted.txt) do (
                echo Processing error in %%a line %%b: %%c
                call gh copilot suggest --format="%%~b" "Fix this C++ error: %%~c in file %%~a at line %%~b" > "fix_suggestion.txt" 2>nul
                if exist "fix_suggestion.txt" (
                    findstr /v /c:"No suggestion available" "fix_suggestion.txt" >nul
                    if !ERRORLEVEL! EQU 0 (
                        echo Found potential fix, applying...
                        "!PYTHON_PATH!" -c "import json; f=open('fixes.json','r'); fixes=json.load(f); f.close(); fixes.append({'file':r'%%~a','line':int(%%~b),'code':open('fix_suggestion.txt','r').read().strip()}); f=open('fixes.json','w'); json.dump(fixes,f,indent=2); f.close()" 2>nul
                        set "FIXES_FOUND=1"
                    )
                    del "fix_suggestion.txt"
                )
            )
        )
        
        if "!FIXES_FOUND!"=="1" (
            echo Applying fixes...
            "!PYTHON_PATH!" apply_fixes.py
            if !ERRORLEVEL! EQU 0 (
                echo Fixes applied successfully, retrying build...
                timeout /t 2
                goto build_loop
            )
        )
    ) else (
        echo Copilot support not available - skipping automatic fixes
    )
    
    echo No more automatic fixes possible.
    set /p "retry=Retry build anyway? (Y/N): "
    if /i "!retry!"=="Y" goto build_loop
    goto end_script
) else (
    echo Build successful!
    REM Copy DLLs
    for %%F in (Core Gui Widgets OpenGL OpenGLWidgets Network DBus) do (
        copy /Y "%QTDIR%\bin\Qt6%%F.dll" "bin\" || echo Warning: Failed to copy Qt6%%F.dll
    )
    copy /Y "%QTDIR%\plugins\platforms\qwindows.dll" "bin\platforms\" || echo Warning: Failed to copy qwindows.dll
    
    if exist "bin\OGL.exe" (
        echo Starting application...
        start "" "bin\OGL.exe"
    )
)

:end_script
echo Build process complete
set "PATH=%OLD_PATH%"
pause
exit /b
