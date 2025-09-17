# Circuit Diagram and Wiring Guide

## Component Layout

```
                    Voice-to-Todo Thermal Printer
                    ==============================

    ┌─────────────────┐         ┌──────────────────┐         ┌─────────────────┐
    │   INMP441       │         │   XIAO ESP32S3   │         │ Thermal Printer │
    │   Microphone    │         │                  │         │                 │
    └─────────────────┘         └──────────────────┘         └─────────────────┘
           │                              │                              │
           └──────────────────────────────┼──────────────────────────────┘
                                          │
                                   ┌──────────────┐
                                   │ Push Button  │
                                   └──────────────┘
```

## Pin Configuration Table

| Component | Pin Name | ESP32S3 GPIO | Wire Color | Notes |
|-----------|----------|---------------|------------|-------|
| **INMP441 Microphone** | | | | |
| | VDD | 3.3V | Red | Power supply |
| | GND | GND | Black | Ground |
| | WS (LRCLK) | GPIO 6 | Yellow | Word Select |
| | SCK (BCLK) | GPIO 5 | Green | Serial Clock |
| | SD (DOUT) | GPIO 9 | Blue | Serial Data |
| | L/R | GND | Black | Left/Right select |
| **Push Button** | | | | |
| | Terminal 1 | GPIO 4 | Orange | Input pin |
| | Terminal 2 | GND | Black | Ground |
| **Thermal Printer** | | | | |
| | RX | GPIO 44 | Purple | Printer receive |
| | TX | GPIO 43 | Gray | Printer transmit |
| | VCC | 5V External | Red | 5V power supply |
| | GND | Common GND | Black | Shared ground |

## Detailed Wiring Diagrams

### Dual INMP441 Microphone Connection (Stereo Setup)
```
INMP441 #1 (Left)         XIAO ESP32S3      INMP441 #2 (Right)
┌─────────┐               ┌──────────────┐    ┌─────────┐
│   VDD   │◄─────────────►│ 3.3V         │◄──►│   VDD   │
│   GND   │◄─────────────►│ GND          │◄──►│   GND   │
│   WS    │◄─────────────►│ GPIO 6       │◄──►│   WS    │
│   SCK   │◄─────────────►│ GPIO 5       │◄──►│   SCK   │
│   SD    │◄─────────────►│ GPIO 9       │◄──►│   SD    │
│   L/R   │◄─────────────►│ GND          │    │   L/R   │◄──► 3.3V
└─────────┘               └──────────────┘    └─────────┘

Dual Microphone Wiring:
- Both microphones share WS, SCK, and SD lines
- Left mic: L/R → GND (left channel)
- Right mic: L/R → 3.3V (right channel)
- Provides stereo audio with noise reduction capabilities
```

### Alternative: Single Microphone Connection
```
INMP441                    XIAO ESP32S3
┌─────────┐               ┌──────────────┐
│   VDD   │◄─────────────►│ 3.3V         │
│   GND   │◄─────────────►│ GND          │
│   WS    │◄─────────────►│ GPIO 6       │
│   SCK   │◄─────────────►│ GPIO 5       │
│   SD    │◄─────────────►│ GPIO 9       │
│   L/R   │◄─────────────►│ GND          │
└─────────┘               └──────────────┘

Note: Connect L/R to GND for mono operation
```

### Push Button Connection
```
Push Button               XIAO ESP32S3
┌─────────┐               ┌──────────────┐
│    O    │◄─────────────►│ GPIO 4       │
│         │               │              │
│    O    │◄─────────────►│ GND          │
└─────────┘               └──────────────┘

Note: Internal pull-up resistor used in software
```

### Thermal Printer Connection
```
Thermal Printer           XIAO ESP32S3        5V Power Supply
┌─────────────┐          ┌──────────────┐     ┌──────────────┐
│ RX (Blue)   │◄────────►│ GPIO 44 (TX) │     │              │
│ TX (Green)  │◄────────►│ GPIO 43 (RX) │     │              │
│ VCC (Red)   │◄─────────┼──────────────┼────►│ +5V          │
│ GND (Black) │◄─────────┼─────────────►│     │              │
└─────────────┘          │ GND          │◄───►│ GND          │
                         └──────────────┘     └──────────────┘

Note: Printer requires separate 5V, 2-3A power supply
```

## Breadboard Layout Suggestion

```
                        Breadboard Layout
    ═══════════════════════════════════════════════════════════════

    Power Rails:
    ┌─ Red    (+)  ◄────── 3.3V from ESP32S3
    ├─ Black  (-)  ◄────── GND (common ground)
    └─ Red    (+)  ◄────── 5V external supply for printer

    Row 1-5:    INMP441 Microphone
    Row 6-8:    Push Button
    Row 9-12:   ESP32S3 (if using breakout board)
    Row 13-16:  Thermal Printer connections

    Connect components with jumper wires as per pin table above
```

## Power Requirements

### XIAO ESP32S3
- **Voltage**: 3.3V (provided via USB or onboard regulator)
- **Current**: ~100-300mA during operation
- **Peak Current**: ~500mA during WiFi transmission

### INMP441 Microphone
- **Voltage**: 3.3V
- **Current**: ~1mA
- **Note**: Very low power consumption

### Thermal Printer
- **Voltage**: 5V (separate power supply required)
- **Current**: 1.5-3A during printing
- **Idle Current**: ~100mA
- **Important**: ESP32S3 cannot power the printer directly

### Push Button
- **Voltage**: 3.3V logic level
- **Current**: Negligible (pulled up internally)

## Assembly Steps

### Step 1: Prepare Components
1. Test each component individually before assembly
2. Check continuity of all jumper wires
3. Verify power supply outputs correct voltages

### Step 2: Ground Connections First
1. Connect all GND pins to a common ground rail
2. Verify continuity between all ground points
3. This prevents ground loops and electrical issues

### Step 3: Power Connections
1. Connect 3.3V rail to ESP32S3 and INMP441
2. Set up separate 5V supply for thermal printer
3. Double-check voltage levels with multimeter

### Step 4: Signal Connections
1. Connect I2S signals (WS, SCK, SD) to microphone
2. Connect UART signals (TX, RX) to printer
3. Connect button input to GPIO 4

### Step 5: Testing
1. Power on system and check for proper operation
2. Verify no shorts or overheating
3. Test each subsystem individually

## Troubleshooting Common Wiring Issues

### No Audio Recording
- **Check**: I2S pin connections (WS, SCK, SD)
- **Verify**: INMP441 power (3.3V to VDD)
- **Test**: L/R pin connected to GND

### Button Not Responding
- **Check**: GPIO 4 connection
- **Verify**: Ground connection to button
- **Test**: Button mechanical operation

### Printer Not Working
- **Check**: TX/RX connections (may need to swap)
- **Verify**: 5V power supply to printer
- **Test**: Baud rate settings in code

### WiFi Connection Issues
- **Check**: ESP32S3 power supply stability
- **Verify**: Antenna connections (if external)
- **Test**: Network accessibility

## Safety Considerations

### Electrical Safety
- Always power off before making connections
- Use proper gauge wire for current levels
- Insulate all connections properly
- Check for shorts before applying power

### Component Protection
- Never exceed voltage ratings
- Use anti-static precautions with ESP32S3
- Ensure proper grounding of all components
- Monitor for overheating during operation

## Advanced Wiring Options

### Status LED (Optional)
Add a status LED to indicate recording state:
```
LED (with resistor)       XIAO ESP32S3
┌─────────────┐          ┌──────────────┐
│ Anode (+)   │◄─[220Ω]─►│ GPIO 10      │
│ Cathode (-) │◄─────────►│ GND          │
└─────────────┘          └──────────────┘
```

### External Antenna (Optional)
For better WiFi range:
```
External Antenna          XIAO ESP32S3
┌─────────────┐          ┌──────────────┐
│ Center      │◄─────────►│ Antenna Pad  │
│ Shield      │◄─────────►│ GND          │
└─────────────┘          └──────────────┘
```

### Power Switch (Recommended)
Add power control:
```
Power Switch             Power Supply
┌─────────────┐          ┌──────────────┐
│ Terminal 1  │◄─────────►│ +5V Out      │
│ Terminal 2  │◄─────────►│ To Device    │
└─────────────┘          └──────────────┘
```

## Final Assembly Tips

1. **Cable Management**: Use cable ties or spiral wrap to organize wires
2. **Strain Relief**: Secure cables near connection points
3. **Labeling**: Label wires for future maintenance
4. **Documentation**: Take photos of final assembly for reference
5. **Testing**: Test all functions before closing any enclosure

---

**Note**: This diagram shows logical connections. Physical breadboard layout may vary based on your specific components and preferences.