@echo off
REM Voice-to-Todo Audio Server Startup Script for Windows
REM This script starts the Whisper audio processing server

echo 🎤 Starting Voice-to-Todo Audio Server
echo =======================================

REM Change to audio server directory
cd /d "%~dp0audio_server"

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Error: Python is not installed or not in PATH
    pause
    exit /b 1
)

REM Check if config file exists
if not exist "config.json" (
    echo ❌ Error: config.json not found
    echo Please ensure you're running this from the project root directory
    pause
    exit /b 1
)

REM Check if credentials file exists
if not exist "..\credentials.h" (
    echo ⚠️  Warning: credentials.h not found
    echo Please copy credentials.h.example to credentials.h and configure your WiFi settings
    echo.
    pause
)

REM Display current IP address
echo 📡 Your computer's IP address:
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /C:"IPv4 Address"') do echo    %%a

REM Read ESP32 IP from config
for /f %%i in ('python -c "import json; print(json.load(open('config.json'))['esp32_ip'])"') do set ESP32_IP=%%i
echo 🎯 ESP32 expected at: %ESP32_IP%
echo.

REM Check if dependencies are installed
echo 🔍 Checking dependencies...
python -c "import whisper, requests, numpy" >nul 2>&1
if errorlevel 1 (
    echo ❌ Missing dependencies. Installing...
    pip install -r requirements.txt
    if errorlevel 1 (
        echo ❌ Failed to install dependencies
        pause
        exit /b 1
    )
)

echo ✅ Dependencies OK
echo.

REM Display instructions
echo 📝 Instructions:
echo    1. Make sure your ESP32S3 is powered on and connected to WiFi
echo    2. Upload the Arduino sketch to your ESP32S3
echo    3. Hold the button on GPIO4 and speak your todo items
echo    4. Release the button to process and print
echo.
echo 🔧 Controls:
echo    - Press Ctrl+C to stop the server
echo    - Check the log file: whisper_server.log
echo.

REM Start the server
echo 🚀 Starting Whisper audio server...
echo    Listening on port 8080 for UDP audio data
echo    Ready to process speech and send to ESP32
echo.

python whisper_server.py

pause