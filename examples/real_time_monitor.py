#!/usr/bin/env python3
"""
Real-time Audio Monitor
Shows live audio levels and quality metrics from ESP32
"""

import socket
import threading
import time
import numpy as np
from datetime import datetime
import sys

class RealTimeAudioMonitor:
    def __init__(self, port=8080):
        self.port = port
        self.running = False
        self.sock = None

        # Audio analysis variables
        self.sample_rate = 16000
        self.window_size = 1024
        self.audio_window = np.zeros(self.window_size, dtype=np.int16)
        self.window_index = 0

        # Display variables
        self.max_level = 0
        self.avg_level = 0
        self.peak_hold = 0
        self.peak_hold_time = 0

    def start_monitor(self):
        """Start real-time monitoring"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind(('0.0.0.0', self.port))
            self.running = True

            print("🎤 Real-time Audio Monitor")
            print("=" * 50)
            print(f"📡 Listening on port {self.port}")
            print("🔘 Press button on ESP32 to start monitoring")
            print("⏹️  Press Ctrl+C to stop")
            print("")

            # Start display thread
            display_thread = threading.Thread(target=self.display_levels, daemon=True)
            display_thread.start()

            while self.running:
                try:
                    self.sock.settimeout(1.0)
                    data, addr = self.sock.recvfrom(4096)
                    self.process_audio_data(data)

                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        print(f"\n❌ Error: {e}")

        except Exception as e:
            print(f"❌ Failed to start monitor: {e}")
        finally:
            if self.sock:
                self.sock.close()

    def process_audio_data(self, data):
        """Process incoming audio data"""
        # Convert bytes to int16 array
        audio_samples = np.frombuffer(data, dtype=np.int16)

        for sample in audio_samples:
            # Add to circular buffer
            self.audio_window[self.window_index] = sample
            self.window_index = (self.window_index + 1) % self.window_size

        # Calculate levels
        current_window = self.audio_window.copy()
        abs_samples = np.abs(current_window)

        # Current levels
        self.max_level = np.max(abs_samples)
        self.avg_level = np.mean(abs_samples)

        # Peak hold
        current_time = time.time()
        if self.max_level > self.peak_hold or (current_time - self.peak_hold_time) > 2.0:
            self.peak_hold = self.max_level
            self.peak_hold_time = current_time

    def display_levels(self):
        """Display real-time levels"""
        print("\n📊 Live Audio Levels:")
        print("Legend: ■ = Average  ▲ = Peak  ◆ = Peak Hold")
        print("")

        while self.running:
            try:
                # Calculate display values (0-50 characters wide)
                max_display = 50
                avg_bar_len = int((self.avg_level / 32768) * max_display)
                max_bar_len = int((self.max_level / 32768) * max_display)
                peak_hold_pos = int((self.peak_hold / 32768) * max_display)

                # Create level bars
                avg_bar = "■" * avg_bar_len + "·" * (max_display - avg_bar_len)
                max_bar = "▲" * max_bar_len + "·" * (max_display - max_bar_len)

                # Add peak hold indicator
                peak_bar = list("·" * max_display)
                if peak_hold_pos < max_display:
                    peak_bar[peak_hold_pos] = "◆"
                peak_bar = "".join(peak_bar)

                # Color coding for levels
                if self.max_level > 28000:
                    level_color = "🔴"  # Too high
                elif self.max_level > 20000:
                    level_color = "🟡"  # Good
                elif self.max_level > 5000:
                    level_color = "🟢"  # Optimal
                elif self.max_level > 1000:
                    level_color = "🔵"  # Low
                else:
                    level_color = "⚫"  # Very low

                # Calculate percentages
                avg_percent = (self.avg_level / 32768) * 100
                max_percent = (self.max_level / 32768) * 100

                # Clear previous lines and display current levels
                print(f"\r{' ' * 80}", end='')  # Clear line
                print(f"\r🎵 {level_color} Avg: {avg_bar} {avg_percent:4.1f}%", end='')
                print(f"\n     Max: {max_bar} {max_percent:4.1f}%", end='')
                print(f"\n    Peak: {peak_bar} {(self.peak_hold/32768)*100:4.1f}%", end='')

                # Audio quality indicators
                quality_indicators = []
                if self.max_level > 30000:
                    quality_indicators.append("⚠️CLIP")
                if self.avg_level < 1000:
                    quality_indicators.append("📢QUIET")
                if self.max_level > 5000 and self.max_level < 25000:
                    quality_indicators.append("✅GOOD")

                if quality_indicators:
                    print(f"\n    {' '.join(quality_indicators)}", end='')

                # Move cursor back up
                print("\r\033[3A", end='', flush=True)

                time.sleep(0.1)  # Update 10 times per second

            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"\n❌ Display error: {e}")
                break

    def stop_monitor(self):
        """Stop monitoring"""
        print(f"\n\n🛑 Stopping monitor...")
        self.running = False

def main():
    """Main function"""
    monitor = RealTimeAudioMonitor()

    try:
        monitor.start_monitor()
    except KeyboardInterrupt:
        monitor.stop_monitor()
        print("👋 Monitor stopped!")

if __name__ == "__main__":
    main()