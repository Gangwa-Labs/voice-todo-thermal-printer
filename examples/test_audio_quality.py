#!/usr/bin/env python3
"""
Audio Quality Test Script
Tests the enhanced audio processing pipeline
"""

import sys
import os
sys.path.append('../audio_server')

import numpy as np
from whisper_server import WhisperAudioServer
import wave
import time

def generate_test_audio():
    """Generate test audio with noise for quality testing"""
    sample_rate = 16000
    duration = 3

    t = np.linspace(0, duration, sample_rate * duration)

    # Speech-like signal
    speech_signal = (
        np.sin(2 * np.pi * 440 * t) * 0.3 +
        np.sin(2 * np.pi * 880 * t) * 0.2 +
        np.sin(2 * np.pi * 1760 * t) * 0.1
    )

    # Add noise
    white_noise = np.random.normal(0, 0.1, len(t))
    low_freq_noise = np.sin(2 * np.pi * 60 * t) * 0.2
    high_freq_noise = np.sin(2 * np.pi * 8000 * t) * 0.15

    noisy_signal = speech_signal + white_noise + low_freq_noise + high_freq_noise
    noisy_signal = np.clip(noisy_signal * 32767, -32768, 32767).astype(np.int16)

    return noisy_signal, sample_rate

def test_audio_enhancement():
    """Test the audio enhancement pipeline"""
    print("🧪 Testing Audio Enhancement Pipeline")

    server = WhisperAudioServer('../audio_server/config.json')
    original_audio, sample_rate = generate_test_audio()

    print("🔧 Applying audio enhancements...")
    start_time = time.time()
    enhanced_audio = server.enhance_audio_quality(original_audio)
    processing_time = time.time() - start_time

    print(f"✅ Processing completed in {processing_time:.3f} seconds")
    print("📁 Audio enhancement test completed successfully")

    return True

if __name__ == "__main__":
    success = test_audio_enhancement()
    sys.exit(0 if success else 1)