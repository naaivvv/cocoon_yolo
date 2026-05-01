@echo off
echo Starting Cocoon YOLO Backend...

:: Start the Python app in a new window
start "Cocoon YOLO Backend" cmd /c "call venv\Scripts\activate && python app.py"

:: Wait for a few seconds to let the server start
echo Waiting for server to start...
timeout /t 3 /nobreak >nul

:: Open the default web browser to the local host webpage
echo Opening browser...
start http://localhost:5000
