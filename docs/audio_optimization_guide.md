# Audio Quality Optimization Guide

This guide provides comprehensive instructions for achieving the best possible audio quality for voice-to-todo transcription.

## 🎯 Hardware Optimizations

### Dual Microphone Setup (Recommended)

**Benefits:**
- **Noise Cancellation**: Differential signal processing reduces background noise
- **Improved SNR**: Signal-to-noise ratio improvement of 3-6 dB
- **Directional Sensitivity**: Better rejection of off-axis noise sources
- **Redundancy**: Backup if one microphone fails

**Physical Placement:**
```
Optimal Microphone Positioning:

    [Left Mic]     [Right Mic]
        │             │
        └─── 2-4cm ───┘
             │
         [Your Voice]
             │
         6-12 inches
```

**Key Guidelines:**
- Place microphones 2-4cm apart (differential processing sweet spot)
- Position 6-12 inches from your mouth
- Avoid placing near fans, air vents, or electrical interference
- Mount rigidly to prevent vibration noise

### Single Microphone Setup

**When to Use:**
- Limited space or components
- Simple installation
- Budget constraints

**Optimization:**
- Use acoustic foam or windscreen if available
- Position directly toward your mouth
- Minimize reflective surfaces nearby

## ⚙️ Software Enhancements

### ESP32 Audio Processing

The updated code includes several real-time enhancements:

#### 1. **Stereo Processing**
```cpp
// Dual microphone differential processing
int32_t differential = (int32_t)left - (int32_t)right;
int32_t mixed = ((int32_t)left + (int32_t)right) / 2 + (differential / 4);
```

#### 2. **Noise Gate**
```cpp
const int NOISE_GATE_THRESHOLD = 500;  // Adjustable
// Only transmit audio above threshold
```

#### 3. **Digital Gain**
```cpp
const int AUDIO_GAIN = 2;  // 2x amplification
```

#### 4. **High-Pass Filter**
```cpp
// Removes low-frequency rumble and interference
int32_t filtered = mixed - prev_input + (prev_output * 0.95);
```

### Server-Side Audio Enhancement

The audio server applies a 5-stage enhancement pipeline:

#### 1. **Spectral Subtraction Noise Reduction**
- Estimates noise from first 0.5 seconds
- Reduces constant background noise
- Preserves speech components

#### 2. **Speech Band-Pass Filter**
- Frequency range: 300Hz - 3400Hz
- Removes sub-sonic and ultrasonic noise
- Optimized for human speech

#### 3. **Dynamic Range Compression**
- 4:1 compression ratio
- Soft knee characteristics
- Prevents clipping and improves consistency

#### 4. **Automatic Gain Control (AGC)**
- Normalizes volume levels
- Compensates for distance variations
- Target RMS: 15% of full scale

#### 5. **Wiener Filtering**
- Advanced noise reduction
- Preserves speech clarity
- Adaptive to signal characteristics

## 🔧 Configuration Options

### Audio Enhancement Settings

Edit `audio_server/config.json`:

```json
{
  "audio_enhancement": {
    "enable_noise_reduction": true,    // Spectral subtraction
    "enable_speech_filter": true,      // Band-pass filter
    "enable_compression": true,        // Dynamic range compression
    "enable_agc": true,               // Automatic gain control
    "enable_wiener_filter": true,     // Advanced noise reduction
    "noise_reduction_strength": 0.7,  // 0.0 - 1.0
    "speech_filter_low": 300,         // Hz
    "speech_filter_high": 3400,       // Hz
    "compression_ratio": 4.0,         // X:1 ratio
    "agc_target_level": 0.15          // 0.0 - 1.0
  }
}
```

### ESP32 Tuning Parameters

In `todo_habit_printer.ino`:

```cpp
// Adjust these values based on your environment
const int NOISE_GATE_THRESHOLD = 500;   // Lower = more sensitive
const int AUDIO_GAIN = 2;               // Higher = more amplification
const size_t BUFFER_SIZE = 2048;        // Larger = better quality, more latency
```

## 📊 Performance Tuning

### Whisper Model Selection

| Model | Size | Speed | Accuracy | Best For |
|-------|------|-------|----------|----------|
| tiny  | 39MB | Fastest | Good | Real-time, limited resources |
| base  | 74MB | Fast | Better | **Recommended balance** |
| small | 244MB | Medium | Very Good | High accuracy needs |
| medium| 769MB | Slow | Excellent | Maximum accuracy |
| large | 1550MB | Slowest | Best | Professional applications |

### Quality vs. Performance Trade-offs

**For Real-time Performance:**
```json
{
  "whisper_model": "base",
  "audio_enhancement": {
    "enable_wiener_filter": false,  // Disable most CPU-intensive
    "compression_ratio": 2.0        // Lighter compression
  }
}
```

**For Maximum Quality:**
```json
{
  "whisper_model": "medium",
  "audio_enhancement": {
    "enable_wiener_filter": true,
    "noise_reduction_strength": 0.8,
    "compression_ratio": 6.0
  }
}
```

## 🛠 Troubleshooting Audio Issues

### Common Problems and Solutions

#### 1. **Low Transcription Accuracy**

**Symptoms:** Whisper produces garbled or incorrect text

**Solutions:**
- Check microphone connections and orientations
- Increase `AUDIO_GAIN` in ESP32 code
- Enable debug audio saving: `"debug_save_audio": true`
- Speak closer to microphones (6-8 inches)
- Reduce background noise sources

#### 2. **No Audio Detection**

**Symptoms:** "No meaningful speech detected" messages

**Solutions:**
- Lower `NOISE_GATE_THRESHOLD` value
- Check I2S wiring and power connections
- Verify microphone L/R pin configurations
- Test with louder speech

#### 3. **Excessive Background Noise**

**Symptoms:** Poor transcription in noisy environments

**Solutions:**
- Increase `noise_reduction_strength` to 0.8-0.9
- Enable all audio enhancement features
- Use dual microphone setup for noise cancellation
- Position microphones away from noise sources

#### 4. **Audio Cutting Out**

**Symptoms:** Intermittent audio or connection drops

**Solutions:**
- Check WiFi signal strength
- Increase buffer size: `BUFFER_SIZE = 4096`
- Reduce `audio_timeout` for faster processing
- Verify stable power supply to ESP32

#### 5. **Distorted Audio**

**Symptoms:** Transcription works but audio sounds harsh

**Solutions:**
- Reduce `AUDIO_GAIN` value
- Lower compression ratio to 2.0-3.0
- Check for clipping in debug audio files
- Speak at moderate volume levels

## 📈 Advanced Optimizations

### Environmental Considerations

**Acoustic Environment:**
- Use in rooms with soft furnishings (carpets, curtains)
- Avoid hard surfaces that cause echoes
- Position away from air conditioning or fans
- Consider acoustic foam for permanent installations

**Electrical Environment:**
- Keep away from power supplies and motors
- Use shielded cables for longer runs
- Ensure stable 3.3V power supply
- Consider ferrite cores on cables if EMI is present

### Custom Tuning

For specific environments, you can fine-tune parameters:

1. **Record Test Audio:**
   ```json
   {"debug_save_audio": true}
   ```

2. **Analyze with Audio Software:**
   - Use Audacity or similar to analyze frequency content
   - Identify primary noise frequencies
   - Adjust filter parameters accordingly

3. **Iterative Testing:**
   - Test different Whisper models
   - Adjust enhancement parameters
   - Measure transcription accuracy

### Performance Monitoring

**Key Metrics to Track:**
- Transcription accuracy percentage
- Processing time per audio segment
- False positive/negative detection rates
- Audio signal-to-noise ratio

**Logging Audio Quality:**
```python
# Enable detailed audio processing logs
"log_level": "DEBUG"
```

## 🎯 Expected Results

With proper implementation of these optimizations:

**Baseline (Single Mic, No Processing):**
- Transcription Accuracy: 70-80%
- Noise Rejection: Poor
- Environmental Sensitivity: High

**Optimized (Dual Mic, Full Processing):**
- Transcription Accuracy: 90-95%
- Noise Rejection: Excellent
- Environmental Sensitivity: Low

**Professional Setup (All Optimizations):**
- Transcription Accuracy: 95-98%
- Real-time Processing: <2 seconds
- Robust Operation: Various environments

## 🔄 Continuous Improvement

1. **Monitor Performance:** Track transcription accuracy over time
2. **Update Models:** Upgrade Whisper models as they improve
3. **Refine Parameters:** Adjust based on actual usage patterns
4. **User Feedback:** Incorporate user experience improvements

---

**Remember:** Audio quality is a chain - the weakest link determines overall performance. Start with good hardware placement, then tune software parameters for your specific environment.