#!/usr/bin/env python3
"""
Advanced Audio Analysis Tool
Analyzes captured audio files and provides detailed quality metrics
"""

import numpy as np
import matplotlib.pyplot as plt
import wave
import sys
import os
from scipy import signal
from scipy.fft import fft, fftfreq

def analyze_audio_file(filename):
    """Analyze a WAV file and provide detailed metrics"""
    try:
        with wave.open(filename, 'rb') as wav_file:
            # Get audio parameters
            sample_rate = wav_file.getframerate()
            channels = wav_file.getnchannels()
            sample_width = wav_file.getsampwidth()
            frames = wav_file.getnframes()
            duration = frames / sample_rate

            # Read audio data
            audio_data = wav_file.readframes(frames)

        # Convert to numpy array
        if sample_width == 2:  # 16-bit
            audio_array = np.frombuffer(audio_data, dtype=np.int16)
        else:
            print(f"⚠️  Unsupported sample width: {sample_width}")
            return

        print(f"🎵 Audio File Analysis: {os.path.basename(filename)}")
        print("=" * 60)

        # Basic properties
        print(f"📊 Basic Properties:")
        print(f"   Duration: {duration:.2f} seconds")
        print(f"   Sample Rate: {sample_rate} Hz")
        print(f"   Channels: {channels}")
        print(f"   Bit Depth: {sample_width * 8} bits")
        print(f"   Total Samples: {len(audio_array):,}")

        # Signal analysis
        print(f"\n🔊 Signal Analysis:")
        max_amp = np.max(np.abs(audio_array))
        min_val = np.min(audio_array)
        max_val = np.max(audio_array)
        rms = np.sqrt(np.mean(audio_array.astype(np.float64)**2))

        print(f"   Peak Amplitude: {max_amp} ({max_amp/32768*100:.1f}% of full scale)")
        print(f"   RMS Level: {rms:.0f} ({rms/32768*100:.1f}% of full scale)")
        print(f"   Dynamic Range: {min_val} to {max_val}")

        # Calculate SNR estimate
        if rms > 0:
            # Estimate noise floor from quietest 10% of samples
            sorted_abs = np.sort(np.abs(audio_array))
            noise_floor = np.mean(sorted_abs[:len(sorted_abs)//10])
            snr = 20 * np.log10(rms / noise_floor) if noise_floor > 0 else float('inf')
            print(f"   Estimated SNR: {snr:.1f} dB")

        # Frequency analysis
        print(f"\n🎼 Frequency Analysis:")

        # Perform FFT
        window_size = min(4096, len(audio_array))
        audio_windowed = audio_array[:window_size] * np.hanning(window_size)
        fft_data = fft(audio_windowed)
        freqs = fftfreq(window_size, 1/sample_rate)

        # Only use positive frequencies
        positive_freqs = freqs[:window_size//2]
        magnitude = np.abs(fft_data[:window_size//2])

        # Find dominant frequencies
        peak_indices = signal.find_peaks(magnitude, height=np.max(magnitude)*0.1)[0]
        if len(peak_indices) > 0:
            dominant_freqs = positive_freqs[peak_indices]
            dominant_mags = magnitude[peak_indices]

            # Sort by magnitude
            sorted_indices = np.argsort(dominant_mags)[::-1]

            print(f"   Dominant Frequencies:")
            for i, idx in enumerate(sorted_indices[:5]):  # Top 5
                freq = dominant_freqs[idx]
                mag = dominant_mags[idx]
                if freq > 0:  # Skip DC component
                    print(f"     {freq:.0f} Hz (magnitude: {mag:.0f})")

        # Speech frequency analysis
        speech_range = (positive_freqs >= 300) & (positive_freqs <= 3400)
        speech_power = np.sum(magnitude[speech_range]**2)
        total_power = np.sum(magnitude**2)
        speech_ratio = speech_power / total_power if total_power > 0 else 0

        print(f"   Speech Frequency Power: {speech_ratio*100:.1f}% of total")

        # Quality assessment
        print(f"\n✅ Quality Assessment:")

        if max_amp < 1000:
            print("   🔴 Very low signal - check microphone connection and gain")
        elif max_amp < 5000:
            print("   🟡 Low signal - consider increasing gain")
        elif max_amp > 30000:
            print("   🟡 High signal - possible clipping, consider reducing gain")
        else:
            print("   🟢 Good signal level")

        if speech_ratio > 0.6:
            print("   🟢 Good speech frequency content")
        elif speech_ratio > 0.3:
            print("   🟡 Moderate speech frequency content")
        else:
            print("   🔴 Low speech frequency content - check microphone placement")

        if snr > 20:
            print("   🟢 Excellent signal-to-noise ratio")
        elif snr > 10:
            print("   🟡 Good signal-to-noise ratio")
        else:
            print("   🔴 Poor signal-to-noise ratio - reduce background noise")

        return {
            'duration': duration,
            'sample_rate': sample_rate,
            'max_amplitude': max_amp,
            'rms_level': rms,
            'snr': snr if 'snr' in locals() else 0,
            'speech_ratio': speech_ratio,
            'audio_array': audio_array,
            'freqs': positive_freqs,
            'magnitude': magnitude
        }

    except Exception as e:
        print(f"❌ Error analyzing audio file: {e}")
        return None

def plot_audio_analysis(filename, analysis_data):
    """Create plots for audio analysis"""
    try:
        import matplotlib.pyplot as plt

        audio_array = analysis_data['audio_array']
        freqs = analysis_data['freqs']
        magnitude = analysis_data['magnitude']
        sample_rate = analysis_data['sample_rate']

        # Create time axis
        time_axis = np.linspace(0, len(audio_array) / sample_rate, len(audio_array))

        # Create plots
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle(f'Audio Analysis: {os.path.basename(filename)}', fontsize=16)

        # Time domain plot
        ax1.plot(time_axis, audio_array)
        ax1.set_title('Time Domain Waveform')
        ax1.set_xlabel('Time (seconds)')
        ax1.set_ylabel('Amplitude')
        ax1.grid(True)

        # Frequency domain plot
        ax2.semilogx(freqs[1:], 20*np.log10(magnitude[1:]))  # Skip DC
        ax2.set_title('Frequency Spectrum')
        ax2.set_xlabel('Frequency (Hz)')
        ax2.set_ylabel('Magnitude (dB)')
        ax2.grid(True)
        ax2.set_xlim([50, sample_rate//2])

        # Speech frequency range highlight
        ax2.axvspan(300, 3400, alpha=0.3, color='green', label='Speech Range')
        ax2.legend()

        # Histogram of amplitudes
        ax3.hist(audio_array, bins=50, alpha=0.7)
        ax3.set_title('Amplitude Distribution')
        ax3.set_xlabel('Amplitude')
        ax3.set_ylabel('Count')
        ax3.grid(True)

        # Spectrogram
        f, t, Sxx = signal.spectrogram(audio_array, sample_rate, nperseg=1024)
        ax4.pcolormesh(t, f, 10*np.log10(Sxx))
        ax4.set_title('Spectrogram')
        ax4.set_xlabel('Time (seconds)')
        ax4.set_ylabel('Frequency (Hz)')
        ax4.set_ylim([0, 4000])  # Focus on speech frequencies

        plt.tight_layout()

        # Save plot
        plot_filename = filename.replace('.wav', '_analysis.png')
        plt.savefig(plot_filename, dpi=150, bbox_inches='tight')
        print(f"📊 Analysis plot saved: {plot_filename}")

        # Show plot
        plt.show()

    except ImportError:
        print("📊 Matplotlib not available for plotting")
    except Exception as e:
        print(f"❌ Error creating plots: {e}")

def main():
    """Main function"""
    print("Audio File Analyzer")
    print("=" * 30)

    if len(sys.argv) != 2:
        print("Usage: python audio_analyzer.py <audio_file.wav>")
        print("\nExample:")
        print("  python audio_analyzer.py captured_audio/esp32_audio_20241216_142830.wav")

        # Look for recent audio files
        audio_dir = "captured_audio"
        if os.path.exists(audio_dir):
            wav_files = [f for f in os.listdir(audio_dir) if f.endswith('.wav')]
            if wav_files:
                wav_files.sort(reverse=True)  # Most recent first
                print(f"\n📁 Found recent audio files in {audio_dir}/:")
                for i, file in enumerate(wav_files[:5]):  # Show last 5
                    print(f"   {i+1}. {file}")
        return

    filename = sys.argv[1]

    if not os.path.exists(filename):
        print(f"❌ File not found: {filename}")
        return

    # Analyze the audio file
    analysis = analyze_audio_file(filename)

    if analysis:
        # Ask if user wants to see plots
        try:
            show_plots = input("\n📊 Generate analysis plots? (y/n): ").lower().startswith('y')
            if show_plots:
                plot_audio_analysis(filename, analysis)
        except KeyboardInterrupt:
            print("\n👋 Analysis complete!")

if __name__ == "__main__":
    main()