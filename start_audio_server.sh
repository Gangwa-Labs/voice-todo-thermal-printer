#!/bin/bash

# Voice-to-Todo Audio Server Startup Script
# This script starts the Whisper audio processing server

echo "🎤 Starting Voice-to-Todo Audio Server"
echo "======================================="

# Change to audio server directory
cd "$(dirname "$0")/audio_server"

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: Python 3 is not installed or not in PATH"
    exit 1
fi

# Check if config file exists
if [ ! -f "config.json" ]; then
    echo "❌ Error: config.json not found"
    echo "Please ensure you're running this from the project root directory"
    exit 1
fi

# Check if credentials file exists
if [ ! -f "../credentials.h" ]; then
    echo "⚠️  Warning: credentials.h not found"
    echo "Please copy credentials.h.example to credentials.h and configure your WiFi settings"
    echo ""
    read -p "Press Enter to continue anyway or Ctrl+C to exit..."
fi

# Display current IP address
echo "📡 Your computer's IP address:"
ifconfig | grep "inet " | grep -v 127.0.0.1 | head -1 | awk '{print "   " $2}'

# Read ESP32 IP from config
ESP32_IP=$(python3 -c "import json; print(json.load(open('config.json'))['esp32_ip'])")
echo "🎯 ESP32 expected at: $ESP32_IP"
echo ""

# Check if dependencies are installed
echo "🔍 Checking dependencies..."
python3 -c "import whisper, requests, numpy" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "❌ Missing dependencies. Installing..."
    pip install -r requirements.txt
    if [ $? -ne 0 ]; then
        echo "❌ Failed to install dependencies"
        exit 1
    fi
fi

echo "✅ Dependencies OK"
echo ""

# Display instructions
echo "📝 Instructions:"
echo "   1. Make sure your ESP32S3 is powered on and connected to WiFi"
echo "   2. Upload the Arduino sketch to your ESP32S3"
echo "   3. Hold the button on GPIO4 and speak your todo items"
echo "   4. Release the button to process and print"
echo ""
echo "🔧 Controls:"
echo "   - Press Ctrl+C to stop the server"
echo "   - Check the log file: whisper_server.log"
echo ""

# Start the server
echo "🚀 Starting Whisper audio server..."
echo "   Listening on port 8080 for UDP audio data"
echo "   Ready to process speech and send to ESP32"
echo ""

python3 whisper_server.py