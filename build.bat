@echo off
set RELEASE_DIR=..\MCTool_Build
if not exist %RELEASE_DIR% mkdir %RELEASE_DIR%

echo Building MCTool...
g++ -std=c++17 -Wall -Wextra -g3 -I./include src/*.cpp -o %RELEASE_DIR%\MCTool.exe -lws2_32 -lwininet -lgdi32 -lcomctl32 -lgdiplus -lcrypt32 -lole32 -lstdc++fs -mwindows
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo Copying required files...
copy config.json %RELEASE_DIR%\ >nul 2>&1
copy prompt_template.json %RELEASE_DIR%\ >nul 2>&1
copy start_game.bat %RELEASE_DIR%\ >nul 2>&1

echo Build completed. Output in %RELEASE_DIR%\
pause