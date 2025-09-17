#!/usr/bin/env python3
"""
Comprehensive Audio System Test
Step-by-step testing of the ESP32 audio capture system
"""

import sys
import os
import subprocess
import time

def run_test_script(script_name, description):
    """Run a test script and handle the results"""
    print(f"\n🧪 Test: {description}")
    print("=" * 60)

    script_path = f"examples/{script_name}"

    if not os.path.exists(script_path):
        print(f"❌ Test script not found: {script_path}")
        return False

    print(f"🚀 Running: {script_path}")
    print("📋 Instructions will appear below...")
    print("-" * 40)

    try:
        # Run the script
        result = subprocess.run([sys.executable, script_path],
                              capture_output=False,
                              text=True,
                              timeout=300)  # 5 minute timeout

        if result.returncode == 0:
            print("✅ Test completed successfully!")
            return True
        else:
            print(f"⚠️  Test ended with code: {result.returncode}")
            return False

    except subprocess.TimeoutExpired:
        print("⏰ Test timed out (5 minutes)")
        return False
    except KeyboardInterrupt:
        print("⏹️  Test interrupted by user")
        return False
    except Exception as e:
        print(f"❌ Error running test: {e}")
        return False

def check_prerequisites():
    """Check if all prerequisites are met"""
    print("🔍 Checking Prerequisites...")
    print("-" * 30)

    checks = []

    # Check Python version
    if sys.version_info >= (3, 7):
        print("✅ Python version OK")
        checks.append(True)
    else:
        print("❌ Python 3.7+ required")
        checks.append(False)

    # Check required modules
    required_modules = ['numpy', 'matplotlib', 'scipy']
    for module in required_modules:
        try:
            __import__(module)
            print(f"✅ {module} installed")
            checks.append(True)
        except ImportError:
            print(f"❌ {module} not installed")
            checks.append(False)

    # Check ESP32 code file
    if os.path.exists('todo_habit_printer.ino'):
        print("✅ Arduino code found")
        checks.append(True)
    else:
        print("❌ Arduino code not found")
        checks.append(False)

    # Check examples directory
    if os.path.exists('examples/'):
        print("✅ Examples directory found")
        checks.append(True)
    else:
        print("❌ Examples directory not found")
        checks.append(False)

    return all(checks)

def show_hardware_checklist():
    """Show hardware setup checklist"""
    print("\n🔧 Hardware Setup Checklist")
    print("=" * 40)
    print("Before running audio tests, ensure:")
    print("")
    print("📱 ESP32S3 Setup:")
    print("   ✓ ESP32S3 powered and connected via USB")
    print("   ✓ Updated Arduino code uploaded")
    print("   ✓ Serial monitor shows 'WiFi connected!'")
    print("   ✓ I2S initialization successful")
    print("")
    print("🎤 Microphone Setup:")
    print("   ✓ INMP441 microphone(s) wired correctly")
    print("   ✓ Power connections: VDD → 3.3V, GND → GND")
    print("   ✓ I2S connections: WS → GPIO6, SCK → GPIO5, SD → GPIO9")
    print("   ✓ L/R configuration: Left → GND, Right → 3.3V (if dual)")
    print("")
    print("🔘 Button Setup:")
    print("   ✓ Push button connected to GPIO4 and GND")
    print("   ✓ Button press is detected (check serial monitor)")
    print("")
    print("🌐 Network Setup:")
    print("   ✓ ESP32 and computer on same WiFi network")
    print("   ✓ ESP32 IP address noted (from serial monitor)")
    print("   ✓ Computer firewall allows UDP on port 8080")
    print("")

    input("Press Enter when hardware setup is complete...")

def main():
    """Main test function"""
    print("🎤 ESP32 Audio System Test Suite")
    print("=" * 50)
    print("This script will guide you through testing your audio system")
    print("")

    # Check prerequisites
    if not check_prerequisites():
        print("\n❌ Prerequisites not met. Please install missing components:")
        print("   pip install numpy matplotlib scipy")
        return False

    # Show hardware checklist
    show_hardware_checklist()

    # Available tests
    tests = [
        {
            'name': 'Audio Capture Test',
            'script': 'audio_capture_test.py',
            'description': 'Captures audio from ESP32 and saves as WAV files',
            'instructions': [
                "This test will capture audio and save it as WAV files",
                "1. The server will start and listen for audio",
                "2. Press and hold the button on your ESP32",
                "3. Speak clearly: 'Testing microphone, one two three'",
                "4. Release the button",
                "5. The audio will be saved as a WAV file",
                "6. Press Ctrl+C to stop the server"
            ]
        },
        {
            'name': 'Real-time Monitor',
            'script': 'real_time_monitor.py',
            'description': 'Shows live audio levels and quality',
            'instructions': [
                "This test shows real-time audio levels",
                "1. Visual level meters will appear",
                "2. Press and hold the button on your ESP32",
                "3. Speak at normal volume and watch the levels",
                "4. Green/Yellow = Good, Red = Too loud, Blue = Too quiet",
                "5. Press Ctrl+C to stop monitoring"
            ]
        }
    ]

    print("\n📋 Available Tests:")
    for i, test in enumerate(tests, 1):
        print(f"   {i}. {test['name']} - {test['description']}")

    print(f"   0. Run all tests")
    print("")

    try:
        choice = input("Select test to run (0-2): ").strip()

        if choice == '0':
            # Run all tests
            success_count = 0
            for test in tests:
                print(f"\n{'='*60}")
                print(f"Instructions for {test['name']}:")
                for instruction in test['instructions']:
                    print(f"   {instruction}")
                print("="*60)

                input("Press Enter to start this test...")

                if run_test_script(test['script'], test['name']):
                    success_count += 1

            print(f"\n🎯 Test Summary: {success_count}/{len(tests)} tests completed successfully")

        elif choice in ['1', '2']:
            test_index = int(choice) - 1
            test = tests[test_index]

            print(f"\n📋 Instructions for {test['name']}:")
            for instruction in test['instructions']:
                print(f"   {instruction}")

            input("\nPress Enter to start the test...")
            run_test_script(test['script'], test['name'])

        else:
            print("❌ Invalid choice")
            return False

    except KeyboardInterrupt:
        print("\n👋 Tests cancelled by user")
    except Exception as e:
        print(f"\n❌ Error running tests: {e}")

    print("\n📁 Check the 'captured_audio/' directory for saved WAV files")
    print("🎧 Play these files in any audio player to hear your recordings")
    print("📊 Use audio_analyzer.py to analyze the quality of recordings")

    return True

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)