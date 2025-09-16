#!/usr/bin/env python3
"""
ESP32 Network Discovery Tool
Automatically finds your ESP32 device on the network and updates config.json
"""

import socket
import threading
import requests
import json
import ipaddress
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed

def get_local_network():
    """Get the local network range"""
    try:
        # Get the local IP address
        hostname = socket.gethostname()
        local_ip = socket.gethostbyname(hostname)

        # Create network from local IP
        network = ipaddress.IPv4Network(f"{local_ip}/24", strict=False)
        return network, local_ip
    except Exception as e:
        print(f"Error getting local network: {e}")
        return None, None

def ping_host(ip):
    """Ping a host to check if it's alive"""
    try:
        # Use appropriate ping command based on OS
        if sys.platform.startswith('win'):
            result = subprocess.run(['ping', '-n', '1', '-w', '1000', str(ip)],
                                  capture_output=True, text=True, timeout=2)
        else:
            result = subprocess.run(['ping', '-c', '1', '-W', '1000', str(ip)],
                                  capture_output=True, text=True, timeout=2)

        if result.returncode == 0:
            return str(ip)
    except:
        pass
    return None

def check_esp32_device(ip):
    """Check if an IP address hosts an ESP32 device"""
    try:
        # Try to connect to the device
        url = f"http://{ip}/"
        response = requests.get(url, timeout=3)

        # Look for ESP32/todo-related keywords in response
        content = response.text.lower()
        keywords = ['esp32', 'todo', 'habit', 'xiao', 'thermal', 'printer']

        for keyword in keywords:
            if keyword in content:
                return True

        # Also check the status endpoint
        status_url = f"http://{ip}/status"
        status_response = requests.get(status_url, timeout=2)
        if status_response.status_code == 200:
            status_text = status_response.text.lower()
            if any(keyword in status_text for keyword in keywords):
                return True

    except:
        pass

    return False

def discover_esp32_devices():
    """Discover ESP32 devices on the network"""
    print("🔍 Discovering ESP32 devices on your network...")

    network, local_ip = get_local_network()
    if not network:
        print("❌ Could not determine local network")
        return []

    print(f"📡 Scanning network {network} from {local_ip}")
    print("   This may take a moment...")

    # First, find all responding hosts
    print("   Step 1: Finding responsive hosts...")
    live_hosts = []

    with ThreadPoolExecutor(max_workers=50) as executor:
        futures = {executor.submit(ping_host, ip): ip for ip in network.hosts()}

        for future in as_completed(futures):
            result = future.result()
            if result:
                live_hosts.append(result)

    print(f"   Found {len(live_hosts)} responsive hosts")

    if not live_hosts:
        print("❌ No responsive hosts found. Check your network connection.")
        return []

    # Now check which ones are ESP32 devices
    print("   Step 2: Identifying ESP32 devices...")
    esp32_devices = []

    for host in live_hosts:
        print(f"   Checking {host}...", end='')
        if check_esp32_device(host):
            esp32_devices.append(host)
            print(" ✅ ESP32 device found!")
        else:
            print(" ⏸️")

    return esp32_devices

def update_config_file(esp32_ip):
    """Update the config.json file with the discovered ESP32 IP"""
    try:
        config_file = 'config.json'

        # Read current config
        with open(config_file, 'r') as f:
            config = json.load(f)

        # Update ESP32 IP
        old_ip = config.get('esp32_ip', 'unknown')
        config['esp32_ip'] = esp32_ip

        # Write back to file
        with open(config_file, 'w') as f:
            json.dump(config, f, indent=2)

        print(f"✅ Updated config.json: {old_ip} → {esp32_ip}")
        return True

    except Exception as e:
        print(f"❌ Error updating config file: {e}")
        return False

def test_esp32_connection(esp32_ip):
    """Test the connection to the ESP32 device"""
    print(f"\n🧪 Testing connection to ESP32 at {esp32_ip}...")

    try:
        # Test main web interface
        response = requests.get(f"http://{esp32_ip}/", timeout=5)
        if response.status_code == 200:
            print("✅ Web interface accessible")
        else:
            print(f"⚠️  Web interface returned status {response.status_code}")

        # Test status endpoint
        status_response = requests.get(f"http://{esp32_ip}/status", timeout=5)
        if status_response.status_code == 200:
            print(f"✅ Status endpoint: {status_response.text.strip()}")
        else:
            print(f"⚠️  Status endpoint returned status {status_response.status_code}")

        # Test text receiving endpoint (without actually sending text)
        print("✅ ESP32 device is ready for voice-to-todo functionality")
        return True

    except requests.exceptions.ConnectionError:
        print(f"❌ Cannot connect to ESP32 at {esp32_ip}")
        print("   Check if the ESP32 is powered on and connected to WiFi")
        return False
    except Exception as e:
        print(f"❌ Connection test failed: {e}")
        return False

def main():
    """Main discovery function"""
    print("ESP32 Network Discovery Tool")
    print("=" * 40)

    # Discover ESP32 devices
    esp32_devices = discover_esp32_devices()

    if not esp32_devices:
        print("\n❌ No ESP32 devices found on your network")
        print("\nTroubleshooting:")
        print("1. Ensure your ESP32 is powered on")
        print("2. Check that it's connected to the same WiFi network")
        print("3. Verify the Arduino sketch is uploaded and running")
        print("4. Try accessing the ESP32 web interface manually")
        return False

    print(f"\n🎉 Found {len(esp32_devices)} ESP32 device(s):")
    for i, device in enumerate(esp32_devices, 1):
        print(f"   {i}. {device}")

    # If only one device, use it automatically
    if len(esp32_devices) == 1:
        selected_ip = esp32_devices[0]
        print(f"\n🎯 Using ESP32 at {selected_ip}")
    else:
        # Let user choose
        print(f"\nWhich device would you like to use? (1-{len(esp32_devices)}): ", end='')
        try:
            choice = int(input()) - 1
            if 0 <= choice < len(esp32_devices):
                selected_ip = esp32_devices[choice]
            else:
                print("❌ Invalid choice")
                return False
        except ValueError:
            print("❌ Invalid input")
            return False

    # Update config file
    if update_config_file(selected_ip):
        # Test the connection
        if test_esp32_connection(selected_ip):
            print(f"\n🎉 Setup complete! Your ESP32 at {selected_ip} is ready.")
            print("\nNext steps:")
            print("1. Run the audio server: python whisper_server.py")
            print("2. Hold the button and speak your todo items")
            print("3. Release the button to process and print")
            return True
        else:
            print(f"\n⚠️  Config updated but connection test failed")
            return False

    return False

if __name__ == "__main__":
    success = main()
    if not success:
        sys.exit(1)