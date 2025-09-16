# Complete Setup Guide

This guide will walk you through setting up your Voice-to-Todo Thermal Printer from start to finish.

## 📋 Prerequisites

### Hardware
- [ ] XIAO ESP32S3 development board
- [ ] INMP441 digital microphone module
- [ ] ESC/POS compatible thermal printer
- [ ] Push button (momentary switch)
- [ ] Breadboard and jumper wires
- [ ] USB-C cable for ESP32S3
- [ ] 5V power supply for thermal printer
- [ ] Computer for running audio processing server

### Software
- [ ] Arduino IDE 2.0+ or PlatformIO
- [ ] Python 3.8+ installed on your computer
- [ ] Git (for cloning the repository)

## 🔌 Hardware Assembly

### Step 1: Prepare Your Workspace
1. Ensure all components are clean and undamaged
2. Have your breadboard and jumper wires ready
3. Keep the circuit diagram handy (see `docs/circuit_diagram.png`)

### Step 2: INMP441 Microphone Wiring
Connect the INMP441 to your XIAO ESP32S3:

| INMP441 Pin | ESP32S3 Pin | Wire Color (suggested) |
|-------------|-------------|------------------------|
| VDD         | 3.3V        | Red                    |
| GND         | GND         | Black                  |
| WS (LRCLK)  | GPIO 6      | Yellow                 |
| SCK (BCLK)  | GPIO 5      | Green                  |
| SD (DOUT)   | GPIO 9      | Blue                   |
| L/R         | GND         | Black                  |

**Important Notes:**
- Connect L/R to GND to set the microphone to left channel
- Ensure solid connections to avoid audio noise
- Keep wires as short as practical

### Step 3: Push Button Wiring
Connect the push button:

| Button Pin | ESP32S3 Pin | Notes |
|------------|-------------|-------|
| One side   | GPIO 4      | Input pin |
| Other side | GND         | Pull-down via internal resistor |

**Button Configuration:**
- The button is configured as active-high in software
- Internal pull-up resistor is used
- No external resistors needed

### Step 4: Thermal Printer Wiring
Connect your thermal printer:

| Printer Pin | ESP32S3 Pin | Notes |
|-------------|-------------|-------|
| RX          | GPIO 44     | ESP32 TX → Printer RX |
| TX          | GPIO 43     | ESP32 RX → Printer TX |
| VCC         | 5V Supply   | External 5V power required |
| GND         | Common GND  | Shared with ESP32 ground |

**Printer Setup:**
- Most thermal printers require 5V, 2-3A power supply
- Ensure proper grounding between all components
- Test printer with simple text before proceeding

## 💻 Software Installation

### Step 1: Clone the Repository
```bash
git clone https://github.com/yourusername/voice-todo-printer.git
cd voice-todo-printer
```

### Step 2: Arduino Environment Setup

#### Install ESP32 Board Package
1. Open Arduino IDE
2. Go to File → Preferences
3. Add this URL to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to Tools → Board → Board Manager
5. Search for "ESP32" and install "esp32 by Espressif Systems"

#### Install Required Libraries
Install these libraries via Arduino Library Manager:
- **ArduinoJson** by Benoit Blanchon
- **WiFi** (built-in with ESP32 package)

### Step 3: Configure Arduino Code

#### Update WiFi Credentials
Edit `todo_habit_printer.ino`:
```cpp
// WiFi credentials
const char* ssid = "YOUR_WIFI_NETWORK";
const char* password = "YOUR_WIFI_PASSWORD";
```

#### Upload the Code
1. Connect ESP32S3 via USB-C
2. Select Board: "XIAO_ESP32S3"
3. Select correct COM port
4. Click Upload
5. Monitor Serial output to verify connection

### Step 4: Python Audio Server Setup

#### Install Python Dependencies
```bash
cd audio_server
pip install -r requirements.txt
```

**Note:** First-time Whisper installation may take several minutes as it downloads the model files.

#### Find Your ESP32's IP Address
1. Open Arduino Serial Monitor
2. Reset your ESP32
3. Note the IP address printed (e.g., "IP address: 192.168.1.105")

## ⚙️ Configuration

### Step 1: Create Audio Server Config
Create `audio_server/config.json`:
```json
{
  "audio_port": 8080,
  "esp32_ip": "192.168.1.105",
  "esp32_port": 80,
  "whisper_model": "base",
  "sample_rate": 16000,
  "channels": 1,
  "bits_per_sample": 16
}
```

**Whisper Model Options:**
- `tiny`: Fastest, least accurate (~1GB VRAM)
- `base`: Good balance (~1GB VRAM) - **Recommended**
- `small`: Better accuracy (~2GB VRAM)
- `medium`: High accuracy (~5GB VRAM)
- `large`: Best accuracy (~10GB VRAM)

### Step 2: Update ESP32 Configuration (if needed)
If your computer's IP address is not `192.168.1.100`, update this line in the Arduino code:
```cpp
const char* AUDIO_SERVER_IP = "192.168.1.YOUR_COMPUTER_IP";
```

## 🚀 First Run

### Step 1: Start the Audio Server
```bash
cd audio_server
python whisper_server.py
```

You should see:
```
====================================
Voice-to-Todo Audio Processing Server
Using OpenAI Whisper for local speech recognition
====================================

Configuration:
  Audio Port: 8080
  ESP32 IP: 192.168.1.105
  Whisper Model: base
  Sample Rate: 16000 Hz

Loading Whisper model: base
Whisper model loaded successfully
UDP server listening on port 8080
```

### Step 2: Test the System
1. Power on your ESP32S3 device
2. Wait for WiFi connection (check Serial Monitor)
3. Access the web interface: `http://192.168.1.105` (your ESP32's IP)
4. Verify device status shows "Ready"

### Step 3: Test Voice Recording
1. Hold the push button
2. Speak clearly: "Buy groceries and call mom"
3. Release the button
4. Watch the audio server console for transcription
5. Check if a todo list prints automatically

## 🔧 Troubleshooting

### Hardware Issues

#### No Audio Recording
- **Check I2S Wiring**: Verify all INMP441 connections
- **Button Not Working**: Test button with multimeter
- **Power Issues**: Ensure stable 3.3V to microphone

#### Printer Not Working
- **Baud Rate**: Try different rates (9600, 19200, 115200)
- **Wiring**: Swap TX/RX if no response
- **Power**: Ensure adequate 5V supply for printer

### Software Issues

#### WiFi Connection Failed
```
Error: WiFi connection timeout
```
**Solutions:**
- Verify network name and password
- Check if network is 2.4GHz (ESP32 doesn't support 5GHz)
- Try moving closer to router

#### Audio Server Connection Error
```
Error: Failed to send text to ESP32
```
**Solutions:**
- Verify ESP32 IP address in config.json
- Check firewall settings on computer
- Ensure both devices are on same network

#### Whisper Model Loading Error
```
Error: Failed to load Whisper model
```
**Solutions:**
- Check internet connection (for initial download)
- Verify sufficient disk space (~2GB for base model)
- Try smaller model (tiny) if memory limited

### Audio Quality Issues

#### Poor Transcription Accuracy
- **Speak Clearly**: Enunciate words clearly
- **Reduce Background Noise**: Use in quiet environment
- **Check Distance**: Stay 6-12 inches from microphone
- **Upgrade Model**: Try 'small' or 'medium' Whisper model

#### No Audio Detected
- **Button Timing**: Hold button throughout speech
- **Microphone Orientation**: INMP441 has directional pickup
- **Volume Level**: Speak at normal conversation volume

## 📊 Performance Optimization

### Audio Server Performance
- **Faster GPU**: CUDA-enabled GPU significantly speeds up processing
- **More RAM**: Larger models require more memory
- **SSD Storage**: Faster model loading from SSD

### Network Optimization
- **Dedicated WiFi**: Use 5GHz network for computer, 2.4GHz for ESP32
- **Quality of Service**: Prioritize UDP traffic if possible
- **Local Network**: Keep all devices on same subnet

## 🛡️ Security Considerations

### Network Security
- Change default WiFi credentials
- Use WPA3 encryption if available
- Consider isolated IoT network

### Audio Privacy
- All processing is local (no cloud services)
- Audio is not stored permanently
- Consider disabling debug audio saving in production

## 📈 Advanced Configuration

### Custom Todo Parsing
Modify the `parseTodoItems()` function in Arduino code to:
- Add custom action words
- Change parsing delimiters
- Implement priority levels

### Multiple Device Support
- Use different UDP ports for multiple ESP32 devices
- Implement device identification in audio packets
- Load balance processing across multiple computers

## 🔄 Maintenance

### Regular Updates
- Update Arduino libraries monthly
- Keep Whisper package current: `pip install --upgrade openai-whisper`
- Monitor for ESP32 firmware updates

### Backup Configuration
- Save working `config.json`
- Document any custom code modifications
- Keep wiring diagrams for reference

## 📞 Getting Help

If you encounter issues not covered in this guide:

1. **Check the logs**: Audio server creates `whisper_server.log`
2. **Enable debug mode**: Set logging level to DEBUG
3. **Create an issue**: Include logs and hardware details
4. **Community forums**: ESP32 and Arduino communities are helpful

---

**Congratulations! Your Voice-to-Todo system is now ready to use! 🎉**