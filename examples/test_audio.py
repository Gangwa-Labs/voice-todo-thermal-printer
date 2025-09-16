#!/usr/bin/env python3
"""
Test script for Voice-to-Todo Audio Processing
Use this to test your audio server setup
"""

import socket
import json
import requests
import time
import sys
import threading
from datetime import datetime

class AudioServerTester:
    def __init__(self, config_file='../audio_server/config.json'):
        """Initialize the tester with configuration"""
        self.load_config(config_file)

    def load_config(self, config_file):
        """Load configuration from JSON file"""
        try:
            with open(config_file, 'r') as f:
                config = json.load(f)

            self.audio_port = config.get('audio_port', 8080)
            self.esp32_ip = config.get('esp32_ip', '192.168.1.100')
            self.esp32_port = config.get('esp32_port', 80)

            print(f"Loaded config: Audio port {self.audio_port}, ESP32 at {self.esp32_ip}:{self.esp32_port}")

        except FileNotFoundError:
            print(f"Config file {config_file} not found, using defaults")
            self.audio_port = 8080
            self.esp32_ip = '192.168.1.100'
            self.esp32_port = 80

    def test_udp_connection(self):
        """Test UDP connection to audio server"""
        print("\n=== Testing UDP Connection ===")

        try:
            # Create test audio data (simulated)
            test_data = b'\x00\x01' * 512  # Simple test pattern

            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.sendto(test_data, ('localhost', self.audio_port))
            sock.close()

            print("✓ UDP test data sent successfully")
            print(f"  Sent {len(test_data)} bytes to localhost:{self.audio_port}")

        except Exception as e:
            print(f"✗ UDP test failed: {e}")

    def test_esp32_connection(self):
        """Test HTTP connection to ESP32"""
        print("\n=== Testing ESP32 Connection ===")

        try:
            # Test status endpoint
            url = f"http://{self.esp32_ip}:{self.esp32_port}/status"
            response = requests.get(url, timeout=5)

            if response.status_code == 200:
                print("✓ ESP32 status endpoint accessible")
                print(f"  Status: {response.text}")
            else:
                print(f"✗ ESP32 returned status {response.status_code}")

        except requests.exceptions.ConnectionError:
            print(f"✗ Cannot connect to ESP32 at {self.esp32_ip}")
            print("  Check if ESP32 is powered on and connected to WiFi")
        except requests.exceptions.Timeout:
            print(f"✗ Connection timeout to ESP32")
        except Exception as e:
            print(f"✗ ESP32 connection failed: {e}")

    def test_text_endpoint(self):
        """Test sending text to ESP32"""
        print("\n=== Testing Text Endpoint ===")

        test_text = "Test todo item from audio server"

        try:
            url = f"http://{self.esp32_ip}:{self.esp32_port}/receive-text"
            payload = {"text": test_text}
            headers = {"Content-Type": "application/json"}

            response = requests.post(url, json=payload, headers=headers, timeout=10)

            if response.status_code == 200:
                print("✓ Text endpoint working")
                print(f"  Response: {response.text}")
                print(f"  Sent text: '{test_text}'")
            else:
                print(f"✗ Text endpoint returned status {response.status_code}")
                print(f"  Response: {response.text}")

        except Exception as e:
            print(f"✗ Text endpoint test failed: {e}")

    def simulate_audio_stream(self, duration=3):
        """Simulate an audio stream to test the full pipeline"""
        print(f"\n=== Simulating Audio Stream ({duration}s) ===")

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

            # Simulate button press start
            print("Simulating button press...")

            start_time = time.time()
            packet_count = 0

            while time.time() - start_time < duration:
                # Generate fake audio data (sine wave pattern)
                timestamp = int((time.time() - start_time) * 1000)
                fake_audio = bytearray()

                for i in range(256):  # 256 samples per packet
                    # Simple sine wave pattern
                    value = int(1000 * (0.5 + 0.5 * (i % 32) / 32))
                    fake_audio.extend(value.to_bytes(2, 'little'))

                sock.sendto(fake_audio, ('localhost', self.audio_port))
                packet_count += 1

                time.sleep(0.05)  # ~20 packets per second

            sock.close()

            print(f"✓ Sent {packet_count} audio packets")
            print("Waiting for processing...")

            # Wait a moment for processing
            time.sleep(3)

        except Exception as e:
            print(f"✗ Audio simulation failed: {e}")

    def test_network_discovery(self):
        """Help discover ESP32 IP address on network"""
        print("\n=== Network Discovery ===")

        import subprocess
        import ipaddress
        import concurrent.futures

        def ping_host(ip):
            try:
                # Quick ping test
                result = subprocess.run(['ping', '-c', '1', '-W', '1000', str(ip)],
                                      capture_output=True, text=True, timeout=2)
                if result.returncode == 0:
                    return str(ip)
            except:
                pass
            return None

        try:
            # Get local network range
            hostname = socket.gethostname()
            local_ip = socket.gethostbyname(hostname)
            network = ipaddress.IPv4Network(f"{local_ip}/24", strict=False)

            print(f"Scanning network {network} for ESP32 devices...")
            print("This may take a moment...")

            # Ping sweep
            with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
                futures = [executor.submit(ping_host, ip) for ip in network.hosts()]
                live_hosts = [f.result() for f in concurrent.futures.as_completed(futures) if f.result()]

            print(f"Found {len(live_hosts)} responding hosts:")

            # Test each host for ESP32 characteristics
            for host in live_hosts:
                try:
                    url = f"http://{host}/status"
                    response = requests.get(url, timeout=2)
                    if "To-Do" in response.text or "ESP32" in response.text:
                        print(f"  ✓ Possible ESP32 at {host}: {response.text[:50]}...")
                except:
                    pass

        except Exception as e:
            print(f"Network discovery failed: {e}")

    def run_all_tests(self):
        """Run all tests"""
        print("Voice-to-Todo Audio Server Tester")
        print("=" * 50)

        self.test_esp32_connection()
        self.test_text_endpoint()
        self.test_udp_connection()
        self.simulate_audio_stream()

        print("\n" + "=" * 50)
        print("Testing complete!")
        print("\nIf any tests failed:")
        print("1. Check that the audio server is running")
        print("2. Verify ESP32 is powered and connected to WiFi")
        print("3. Update config.json with correct IP addresses")
        print("4. Check firewall settings")

def main():
    """Main function with command line options"""
    if len(sys.argv) > 1:
        command = sys.argv[1]
        tester = AudioServerTester()

        if command == "discover":
            tester.test_network_discovery()
        elif command == "esp32":
            tester.test_esp32_connection()
        elif command == "udp":
            tester.test_udp_connection()
        elif command == "text":
            tester.test_text_endpoint()
        elif command == "audio":
            duration = 5 if len(sys.argv) < 3 else int(sys.argv[2])
            tester.simulate_audio_stream(duration)
        else:
            print("Usage: python test_audio.py [discover|esp32|udp|text|audio]")
            print("  discover - Find ESP32 on network")
            print("  esp32    - Test ESP32 connection")
            print("  udp      - Test UDP audio connection")
            print("  text     - Test text endpoint")
            print("  audio    - Simulate audio stream")
    else:
        tester = AudioServerTester()
        tester.run_all_tests()

if __name__ == "__main__":
    main()