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
from scipy import signal
from scipy.ndimage import uniform_filter1d

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
            self.debug_save_audio = config.get('debug_save_audio', False)

            logger.info(f"Configuration loaded from {config_file}")
            if self.debug_save_audio:
                logger.info("Debug audio saving enabled")

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
            # Safely clear buffer
            try:
                self.audio_buffer.clear()
            except BufferError:
                self.audio_buffer = bytearray()

        try:
            self.audio_buffer.extend(data)
            self.last_audio_time = current_time

            logger.debug(f"Received {len(data)} bytes, total buffer: {len(self.audio_buffer)} bytes")

            # Prevent buffer from getting too large (max 30 seconds at 16kHz, 16-bit)
            max_buffer_size = 30 * self.sample_rate * 2  # 30 seconds of audio
            if len(self.audio_buffer) > max_buffer_size:
                logger.warning("Audio buffer too large, truncating...")
                # Keep only the last 20 seconds
                keep_size = 20 * self.sample_rate * 2
                self.audio_buffer = self.audio_buffer[-keep_size:]

        except Exception as e:
            logger.error(f"Error handling audio data: {e}")
            # Reset recording state on error
            self.is_recording = False
            self.audio_buffer = bytearray()

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

            # Create a copy of the buffer to avoid buffer export issues
            audio_data = bytes(self.audio_buffer)

            # Convert audio buffer to numpy array
            audio_array = np.frombuffer(audio_data, dtype=np.int16)

            # Check if we have enough audio data
            if len(audio_array) < self.sample_rate:  # Less than 1 second
                logger.warning("Insufficient audio data for processing")
                return

            # Apply advanced audio processing for better quality
            processed_audio = self.enhance_audio_quality(audio_array)

            # Normalize to float32 [-1, 1] as expected by Whisper
            audio_float = processed_audio.astype(np.float32) / 32768.0

            # Use Whisper to transcribe
            logger.info("Running speech recognition...")
            result = self.whisper_model.transcribe(
                audio_float,
                fp16=False,
                language='en',
                task='transcribe',
                verbose=False
            )

            transcribed_text = result['text'].strip()

            # Safely get confidence score
            segments = result.get('segments', [])
            if segments and len(segments) > 0:
                confidence = segments[0].get('avg_logprob', -1.0)
            else:
                confidence = -1.0

            logger.info(f"Transcription: '{transcribed_text}' (confidence: {confidence:.2f})")

            if transcribed_text and len(transcribed_text) > 2:
                self.send_text_to_esp32(transcribed_text)
            else:
                logger.warning("No meaningful speech detected in audio")
                # Save audio for debugging if enabled
                debug_save = getattr(self, 'debug_save_audio', False)
                if debug_save:
                    self.save_audio_debug("no_speech_detected")

        except Exception as e:
            logger.error(f"Error processing audio: {e}")
            import traceback
            logger.error(f"Full traceback: {traceback.format_exc()}")

            # Save audio for debugging
            try:
                self.save_audio_debug("processing_error")
            except:
                pass
        finally:
            self.is_recording = False
            # Clear buffer safely
            try:
                self.audio_buffer.clear()
            except BufferError:
                # If buffer is locked, create a new one
                self.audio_buffer = bytearray()
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

            # Create a copy of the buffer to avoid buffer export issues
            audio_data = bytes(self.audio_buffer)

            # Convert to numpy array
            audio_array = np.frombuffer(audio_data, dtype=np.int16)

            # Save as WAV file
            import wave
            with wave.open(filename, 'wb') as wav_file:
                wav_file.setnchannels(self.channels)
                wav_file.setsampwidth(self.bits_per_sample // 8)
                wav_file.setframerate(self.sample_rate)
                wav_file.writeframes(audio_array.tobytes())

            logger.info(f"Debug audio saved as {filename}")

        except Exception as e:
            logger.error(f"Failed to save debug audio: {e}")

    def enhance_audio_quality(self, audio_data):
        """Apply advanced audio processing to improve quality"""
        try:
            # Convert to float for processing
            audio_float = audio_data.astype(np.float64)

            # 1. Noise Reduction using spectral subtraction
            audio_denoised = self.spectral_subtraction(audio_float)

            # 2. Apply band-pass filter for speech frequencies (300Hz - 3400Hz)
            audio_filtered = self.apply_speech_filter(audio_denoised)

            # 3. Dynamic range compression
            audio_compressed = self.apply_compression(audio_filtered)

            # 4. Automatic gain control
            audio_agc = self.apply_agc(audio_compressed)

            # 5. Speech enhancement using Wiener filtering
            audio_enhanced = self.wiener_filter(audio_agc)

            # Convert back to int16
            audio_enhanced = np.clip(audio_enhanced, -32768, 32767).astype(np.int16)

            logger.debug("Audio enhancement applied successfully")
            return audio_enhanced

        except Exception as e:
            logger.warning(f"Audio enhancement failed, using original: {e}")
            return audio_data

    def spectral_subtraction(self, audio):
        """Reduce noise using spectral subtraction"""
        # Estimate noise from first 0.5 seconds (assumed to be mostly noise)
        noise_samples = min(int(0.5 * self.sample_rate), len(audio) // 4)
        noise_estimate = np.mean(np.abs(audio[:noise_samples])) * 1.5

        # Apply spectral subtraction
        cleaned = audio.copy()
        mask = np.abs(cleaned) < noise_estimate
        cleaned[mask] *= 0.1  # Reduce noise components

        return cleaned

    def apply_speech_filter(self, audio):
        """Apply band-pass filter for speech frequencies"""
        nyquist = self.sample_rate / 2
        low_freq = 300 / nyquist   # 300 Hz
        high_freq = 3400 / nyquist # 3400 Hz

        # Design band-pass filter
        b, a = signal.butter(4, [low_freq, high_freq], btype='band')
        filtered = signal.filtfilt(b, a, audio)

        return filtered

    def apply_compression(self, audio):
        """Apply dynamic range compression"""
        # Simple soft knee compression
        threshold = 0.7 * np.max(np.abs(audio))
        ratio = 4.0  # 4:1 compression ratio

        compressed = audio.copy()
        mask = np.abs(compressed) > threshold

        # Apply compression to samples above threshold
        excess = np.abs(compressed[mask]) - threshold
        compressed[mask] = np.sign(compressed[mask]) * (threshold + excess / ratio)

        return compressed

    def apply_agc(self, audio):
        """Apply automatic gain control"""
        # Target RMS level
        target_rms = 0.15 * 32768

        # Calculate current RMS
        current_rms = np.sqrt(np.mean(audio**2))

        if current_rms > 0:
            # Apply gain to reach target RMS
            gain = target_rms / current_rms
            # Limit gain to prevent over-amplification
            gain = min(gain, 3.0)  # Max 3x gain
            audio = audio * gain

        return audio

    def wiener_filter(self, audio):
        """Apply Wiener filtering for speech enhancement"""
        # Simple Wiener filter implementation
        # Estimate signal and noise power spectral densities
        window_size = min(512, len(audio) // 8)

        # Smooth the signal for noise estimation
        smoothed = uniform_filter1d(np.abs(audio), size=window_size, mode='nearest')

        # Calculate Wiener filter coefficients
        signal_power = smoothed**2
        noise_power = np.mean(signal_power[:window_size])  # Estimate from beginning

        wiener_gain = signal_power / (signal_power + noise_power)

        # Apply filter
        enhanced = audio * wiener_gain

        return enhanced

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