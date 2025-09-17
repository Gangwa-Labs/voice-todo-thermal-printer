#!/usr/bin/env python3
"""
Audio Capture Test Script
Captures audio data from ESP32 and saves as WAV files for analysis
"""

import socket
import threading
import time
import wave
import numpy as np
from datetime import datetime
import sys
import os
import signal

class AudioCaptureServer:
    def __init__(self, port=8080, sample_rate=16000):
        self.port = port
        self.sample_rate = sample_rate
        self.audio_buffer = bytearray()
        self.is_recording = False
        self.last_audio_time = 0
        self.audio_timeout = 3.0  # seconds
        self.running = False
        self.sock = None

        # Create output directory
        self.output_dir = "captured_audio"
        os.makedirs(self.output_dir, exist_ok=True)

        print(f"📁 Audio files will be saved to: {self.output_dir}/")

    def start_server(self):
        """Start UDP server to capture audio"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind(('0.0.0.0', self.port))
            self.running = True

            print(f"🎤 Audio capture server listening on port {self.port}")
            print("📡 Waiting for ESP32 audio data...")
            print("🔘 Press the button on your ESP32 to start recording")
            print("⏹️  Press Ctrl+C to stop the server")
            print("-" * 60)

            # Start timeout checker thread
            timeout_thread = threading.Thread(target=self.check_audio_timeout, daemon=True)
            timeout_thread.start()

            while self.running:
                try:
                    # Set socket timeout to allow for clean shutdown
                    self.sock.settimeout(1.0)
                    data, addr = self.sock.recvfrom(4096)
                    self.handle_audio_data(data, addr)

                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        print(f"❌ Error receiving data: {e}")

        except Exception as e:
            print(f"❌ Failed to start server: {e}")
        finally:
            if self.sock:
                self.sock.close()

    def handle_audio_data(self, data, addr):
        """Handle incoming audio data"""
        current_time = time.time()

        if not self.is_recording:
            print(f"🎵 Started receiving audio from {addr[0]}")
            print(f"   Time: {datetime.now().strftime('%H:%M:%S')}")
            self.is_recording = True
            self.audio_buffer.clear()

        # Add data to buffer
        self.audio_buffer.extend(data)
        self.last_audio_time = current_time

        # Show progress
        duration = len(self.audio_buffer) / (self.sample_rate * 2)  # 16-bit samples
        print(f"\r📊 Recording: {duration:.1f}s ({len(self.audio_buffer)} bytes)", end='', flush=True)

    def check_audio_timeout(self):
        """Check for audio timeout and save recording"""
        while self.running:
            time.sleep(0.1)

            if self.is_recording and self.last_audio_time > 0:
                if time.time() - self.last_audio_time > self.audio_timeout:
                    print(f"\n⏹️  Recording finished (timeout)")
                    self.save_audio_recording()
                    self.is_recording = False
                    self.last_audio_time = 0

    def save_audio_recording(self):
        """Save the current audio buffer as a WAV file"""
        if len(self.audio_buffer) == 0:
            print("⚠️  No audio data to save")
            return

        try:
            # Generate filename with timestamp
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{self.output_dir}/esp32_audio_{timestamp}.wav"

            # Convert bytes to numpy array
            audio_array = np.frombuffer(self.audio_buffer, dtype=np.int16)

            # Calculate some basic stats
            duration = len(audio_array) / self.sample_rate
            max_amplitude = np.max(np.abs(audio_array)) if len(audio_array) > 0 else 0
            rms_level = np.sqrt(np.mean(audio_array.astype(np.float64)**2)) if len(audio_array) > 0 else 0

            # Save as WAV file
            with wave.open(filename, 'wb') as wav_file:
                wav_file.setnchannels(1)  # Mono
                wav_file.setsampwidth(2)  # 16-bit
                wav_file.setframerate(self.sample_rate)
                wav_file.writeframes(audio_array.tobytes())

            # Display save confirmation and stats
            print(f"💾 Audio saved: {filename}")
            print(f"   📏 Duration: {duration:.2f} seconds")
            print(f"   📊 Samples: {len(audio_array):,}")
            print(f"   🔊 Max Amplitude: {max_amplitude} ({max_amplitude/32768*100:.1f}%)")
            print(f"   📢 RMS Level: {rms_level:.0f} ({rms_level/32768*100:.1f}%)")

            # Audio quality assessment
            if max_amplitude < 1000:
                print("   ⚠️  Very quiet audio - check microphone and gain settings")
            elif max_amplitude > 30000:
                print("   ⚠️  Possible clipping - reduce gain settings")
            else:
                print("   ✅ Good audio levels")

            if duration < 0.5:
                print("   ⚠️  Very short recording - try speaking longer")

            print(f"📁 Open {filename} in audio software to analyze")
            print("-" * 60)
            print("🔘 Ready for next recording...")

        except Exception as e:
            print(f"❌ Failed to save audio: {e}")

    def stop_server(self):
        """Stop the server gracefully"""
        print(f"\n🛑 Stopping audio capture server...")
        self.running = False

        # Save any remaining audio
        if self.is_recording and len(self.audio_buffer) > 0:
            print("💾 Saving final recording...")
            self.save_audio_recording()

def signal_handler(signum, frame):
    """Handle Ctrl+C gracefully"""
    print(f"\n🔄 Shutting down...")
    sys.exit(0)

def main():
    """Main function"""
    print("ESP32 Audio Capture Test")
    print("=" * 40)
    print("This script captures audio from your ESP32 and saves it as WAV files")
    print("You can then play these files to hear what the microphone is capturing")
    print("")

    # Set up signal handler for Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)

    # Create and start server
    server = AudioCaptureServer()

    try:
        server.start_server()
    except KeyboardInterrupt:
        pass
    finally:
        server.stop_server()
        print("👋 Goodbye!")

if __name__ == "__main__":
    main()