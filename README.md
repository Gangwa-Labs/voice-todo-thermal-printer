# Voice-to-Todo Thermal Printer

A smart thermal printer system that converts your voice into printed todo lists using the XIAO ESP32S3, INMP441 microphone, and OpenAI Whisper for local speech recognition.

## 🎯 Features

- **Voice-to-Todo**: Record todo items by holding a button and automatically print them
- **Web Interface**: Manual todo list and habit tracker creation
- **Local Processing**: Audio processing runs locally using OpenAI Whisper
- **Thermal Printing**: Physical printouts of your todo lists and habit trackers
- **WiFi Enabled**: Wireless communication between microphone and processing computer

## 🛠 Hardware Requirements

### Main Components
- **XIAO ESP32S3** - Main microcontroller
- **INMP441 Microphone** - Digital I2S microphone
- **Thermal Printer** - Compatible with ESC/POS commands
- **Push Button** - For recording trigger
- **Breadboard and jumper wires**

### Pin Configuration
```
INMP441 Microphone:
- WS (Word Select) → GPIO 6
- SCK (Serial Clock) → GPIO 5
- SD (Serial Data) → GPIO 9
- VDD → 3.3V
- GND → GND

Push Button:
- One side → GPIO 4
- Other side → GND
- Pull-up resistor built into code

Thermal Printer:
- RX → GPIO 44
- TX → GPIO 43
- VCC → 5V
- GND → GND
```

## 📁 Project Structure

```
todo_habit_printer/
├── todo_habit_printer.ino     # Arduino sketch for ESP32S3
├── audio_server/
│   ├── whisper_server.py      # Python audio processing server
│   ├── requirements.txt       # Python dependencies
│   └── config.json           # Configuration file
├── docs/
│   ├── circuit_diagram.png    # Wiring diagram
│   └── setup_guide.md        # Detailed setup instructions
├── examples/
│   └── test_audio.py         # Test script for audio processing
└── README.md                 # This file
```

## 🚀 Quick Start

### 1. Hardware Setup
1. Wire the components according to the pin configuration above
2. Connect the thermal printer to a 5V power supply
3. Upload the Arduino sketch to your XIAO ESP32S3

### 2. Software Setup

#### Install Python Dependencies
```bash
cd audio_server
pip install -r requirements.txt
```

#### Configure Network Settings
1. Create WiFi credentials file:
   ```bash
   cp credentials.h.example credentials.h
   ```
   Then edit `credentials.h` with your actual WiFi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_NETWORK";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* AUDIO_SERVER_IP = "192.168.1.100"; // Your computer's IP
   ```

2. Update the ESP32 IP in `audio_server/config.json` after first boot

#### Run the Audio Processing Server

**Easy Start (Recommended):**
```bash
./start_audio_server.sh    # On macOS/Linux
# or
start_audio_server.bat     # On Windows
```

**Manual Start:**
```bash
cd audio_server
python whisper_server.py
```

**Auto-discover ESP32 (if IP changed):**
```bash
cd audio_server
python auto_discover_esp32.py
```

### 3. Usage
1. Power on the ESP32S3 device and wait for WiFi connection
2. Start the Python audio server: `./start_audio_server.sh`
3. Hold the button on GPIO4 and speak your todo items clearly
4. Release the button - audio will be processed and todo list printed automatically

## 🔧 Configuration

### ESP32S3 Configuration
Update these variables in the Arduino code:
- WiFi credentials
- Audio server IP address
- Printer baud rate

### Audio Server Configuration
Edit `audio_server/config.json`:
```json
{
  "audio_port": 8080,
  "esp32_ip": "192.168.1.xxx",
  "whisper_model": "base",
  "sample_rate": 16000
}
```

## 📖 How It Works

1. **Voice Recording**: Press and hold the button to record audio via the INMP441 microphone
2. **Audio Streaming**: Audio data streams over WiFi to your computer via UDP
3. **Speech Recognition**: OpenAI Whisper converts audio to text locally
4. **Text Processing**: Smart parsing extracts todo items from natural speech
5. **Thermal Printing**: Formatted todo list prints automatically

## 🌐 Web Interface

Access the web interface by navigating to your ESP32's IP address:
- Create manual todo lists
- Design habit tracking cards
- View device status
- Configure settings

## 📋 Example Voice Commands

- "Buy groceries, call mom, finish the report"
- "Remember to water the plants and pick up dry cleaning"
- "Schedule dentist appointment, pay electric bill, organize closet"

## 🔍 Troubleshooting

### Common Issues
- **No audio recording**: Check I2S wiring and button connection
- **WiFi connection failed**: Verify network credentials
- **Printer not responding**: Check baud rate and wiring
- **Audio server errors**: Ensure Python dependencies are installed

### Debug Mode
Enable serial debugging in the Arduino code to monitor:
- WiFi connection status
- Audio streaming
- Button presses
- Print commands

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments

- OpenAI for the Whisper speech recognition model
- Arduino and ESP32 communities
- Thermal printer libraries and documentation

## 📞 Support

- Create an issue for bug reports
- Check the docs/ folder for detailed guides
- See examples/ for code samples

---

**Happy Todo-ing! 📝✨**