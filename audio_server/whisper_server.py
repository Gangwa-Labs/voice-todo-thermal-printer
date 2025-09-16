#!/usr/bin/env python3
"""
Voice-to-Todo Audio Processing Server
Uses OpenAI Whisper for local speech recognition
"""

import socket
import threading
import json
import requests
import numpy as np
import whisper
import io
import wave
import time
from datetime import datetime
import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('whisper_server.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

class WhisperAudioServer:
    def __init__(self, config_file='config.json'):
        """Initialize the audio server with configuration"""
        self.load_config(config_file)
        self.whisper_model = None
        self.audio_buffer = bytearray()
        self.is_recording = False
        self.udp_socket = None
        self.last_audio_time = 0
        self.audio_timeout = 2.0  # seconds of silence before processing

    def load_config(self, config_file):
        """Load configuration from JSON file"""
        try:
            with open(config_file, 'r') as f:
                config = json.load(f)

            self.audio_port = config.get('audio_port', 8080)
            self.esp32_ip = config.get('esp32_ip', '192.168.1.100')
            self.esp32_port = config.get('esp32_port', 80)
            self.whisper_model_name = config.get('whisper_model', 'base')
            self.sample_rate = config.get('sample_rate', 16000)
            self.channels = config.get('channels', 1)
            self.bits_per_sample = config.get('bits_per_sample', 16)

            logger.info(f"Configuration loaded from {config_file}")

        except FileNotFoundError:
            logger.warning(f"Config file {config_file} not found, using defaults")
            self.audio_port = 8080
            self.esp32_ip = '192.168.1.100'
            self.esp32_port = 80
            self.whisper_model_name = 'base'
            self.sample_rate = 16000
            self.channels = 1
            self.bits_per_sample = 16

    def load_whisper_model(self):
        """Load the Whisper model for speech recognition"""
        try:
            logger.info(f"Loading Whisper model: {self.whisper_model_name}")
            self.whisper_model = whisper.load_model(self.whisper_model_name)
            logger.info("Whisper model loaded successfully")
        except Exception as e:
            logger.error(f"Failed to load Whisper model: {e}")
            raise

    def start_udp_server(self):
        """Start UDP server to receive audio data from ESP32"""
        try:
            self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_socket.bind(('0.0.0.0', self.audio_port))
            logger.info(f"UDP server listening on port {self.audio_port}")

            # Start audio timeout checker
            timeout_thread = threading.Thread(target=self.check_audio_timeout, daemon=True)
            timeout_thread.start()

            while True:
                try:
                    data, addr = self.udp_socket.recvfrom(4096)
                    self.handle_audio_data(data, addr)
                except Exception as e:
                    logger.error(f"Error receiving UDP data: {e}")

        except Exception as e:
            logger.error(f"Failed to start UDP server: {e}")
            raise

    def handle_audio_data(self, data, addr):
        """Handle incoming audio data from ESP32"""
        current_time = time.time()

        if not self.is_recording:
            logger.info(f"Started receiving audio from {addr[0]}")
            self.is_recording = True
            self.audio_buffer.clear()

        self.audio_buffer.extend(data)
        self.last_audio_time = current_time

        logger.debug(f"Received {len(data)} bytes, total buffer: {len(self.audio_buffer)} bytes")

    def check_audio_timeout(self):
        """Check for audio timeout and process buffered audio"""
        while True:
            time.sleep(0.1)

            if self.is_recording and self.last_audio_time > 0:
                if time.time() - self.last_audio_time > self.audio_timeout:
                    logger.info("Audio timeout reached, processing audio...")
                    self.process_audio_buffer()

    def process_audio_buffer(self):
        """Process the audio buffer using Whisper"""
        if len(self.audio_buffer) == 0:
            logger.warning("No audio data to process")
            return

        try:
            logger.info(f"Processing {len(self.audio_buffer)} bytes of audio data")

            # Convert audio buffer to numpy array
            audio_array = np.frombuffer(self.audio_buffer, dtype=np.int16)

            # Normalize to float32 [-1, 1] as expected by Whisper
            audio_float = audio_array.astype(np.float32) / 32768.0

            # Use Whisper to transcribe
            logger.info("Running speech recognition...")
            result = self.whisper_model.transcribe(
                audio_float,
                fp16=False,
                language='en',
                task='transcribe'
            )

            transcribed_text = result['text'].strip()
            confidence = result.get('segments', [{}])[0].get('avg_logprob', 0)

            logger.info(f"Transcription: '{transcribed_text}' (confidence: {confidence:.2f})")

            if transcribed_text:
                self.send_text_to_esp32(transcribed_text)
            else:
                logger.warning("No speech detected in audio")

        except Exception as e:
            logger.error(f"Error processing audio: {e}")
        finally:
            self.is_recording = False
            self.audio_buffer.clear()
            self.last_audio_time = 0

    def send_text_to_esp32(self, text):
        """Send transcribed text back to ESP32"""
        try:
            url = f"http://{self.esp32_ip}:{self.esp32_port}/receive-text"
            payload = {"text": text}
            headers = {"Content-Type": "application/json"}

            logger.info(f"Sending text to ESP32: {url}")
            response = requests.post(url, json=payload, headers=headers, timeout=10)

            if response.status_code == 200:
                logger.info(f"Successfully sent text to ESP32: '{text}'")
            else:
                logger.error(f"ESP32 responded with status {response.status_code}: {response.text}")

        except requests.exceptions.RequestException as e:
            logger.error(f"Failed to send text to ESP32: {e}")

    def save_audio_debug(self, filename_prefix="debug_audio"):
        """Save current audio buffer to WAV file for debugging"""
        if len(self.audio_buffer) == 0:
            return

        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{filename_prefix}_{timestamp}.wav"

            # Convert to numpy array
            audio_array = np.frombuffer(self.audio_buffer, dtype=np.int16)

            # Save as WAV file
            with wave.open(filename, 'wb') as wav_file:
                wav_file.setnchannels(self.channels)
                wav_file.setsampwidth(self.bits_per_sample // 8)
                wav_file.setframerate(self.sample_rate)
                wav_file.writeframes(audio_array.tobytes())

            logger.info(f"Debug audio saved as {filename}")

        except Exception as e:
            logger.error(f"Failed to save debug audio: {e}")

    def run(self):
        """Main server loop"""
        logger.info("Starting Voice-to-Todo Audio Processing Server")

        try:
            # Load Whisper model
            self.load_whisper_model()

            # Start UDP server
            self.start_udp_server()

        except KeyboardInterrupt:
            logger.info("Server stopped by user")
        except Exception as e:
            logger.error(f"Server error: {e}")
        finally:
            if self.udp_socket:
                self.udp_socket.close()
                logger.info("UDP socket closed")

def main():
    """Main entry point"""
    print("=" * 60)
    print("Voice-to-Todo Audio Processing Server")
    print("Using OpenAI Whisper for local speech recognition")
    print("=" * 60)

    server = WhisperAudioServer()

    # Display configuration
    print(f"\nConfiguration:")
    print(f"  Audio Port: {server.audio_port}")
    print(f"  ESP32 IP: {server.esp32_ip}")
    print(f"  Whisper Model: {server.whisper_model_name}")
    print(f"  Sample Rate: {server.sample_rate} Hz")
    print(f"\nPress Ctrl+C to stop the server")
    print("-" * 60)

    server.run()

if __name__ == "__main__":
    main()