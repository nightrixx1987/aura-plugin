#pragma once

#include <JuceHeader.h>
#include "EQProcessor.h"

/**
 * LinearPhaseEQ: FFT-basierter Zero-Phase EQ
 * 
 * Funktionsprinzip:
 * 1. Sammelt Input-Samples in einem Overlap-Add-Buffer (50% Overlap)
 * 2. Wendet ein Hann-Fenster an
 * 3. FFT → Frequenzbereich
 * 4. Multipliziert mit der gewünschten Magnitude-Antwort (Phase = 0)
 * 5. IFFT → Zeitbereich
 * 6. Overlap-Add in den Output-Buffer
 * 
 * Latenz: FFT_SIZE / 2 Samples (bei 4096: ~46ms bei 44.1kHz)
 * 
 * Latenz-Stufen:
 *   Low:    2048 Samples (~46ms bei 44.1kHz)  — für Mixing
 *   Medium: 4096 Samples (~93ms bei 44.1kHz)  — Standardmodus
 *   High:   8192 Samples (~186ms bei 44.1kHz) — für Mastering (beste Qualität)
 */
class LinearPhaseEQ
{
public:
    enum class LatencyMode
    {
        Low = 0,    // 2048
        Medium,     // 4096
        High        // 8192
    };

    LinearPhaseEQ() = default;
    ~LinearPhaseEQ() = default;

    void prepare(double sampleRate, int /*samplesPerBlock*/, int numChannels)
    {
        currentSampleRate = sampleRate;
        maxChannels = juce::jmin(numChannels, 2);
        
        updateFFTSize();
    }

    void setLatencyMode(LatencyMode mode)
    {
        if (latencyMode != mode)
        {
            latencyMode = mode;
            updateFFTSize();
        }
    }

    LatencyMode getLatencyMode() const { return latencyMode; }

    int getLatencyInSamples() const { return fftSize / 2; }

    void reset()
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            inputBuffer[ch].clear();
            outputBuffer[ch].clear();
            overlapBuffer[ch].clear();
        }
        inputWritePos = 0;
        outputReadPos = 0;
        samplesUntilNextFFT = hopSize;
    }

    /**
     * Aktualisiert die Ziel-Magnitude-Antwort aus dem EQProcessor.
     * Sollte vom GUI/Timer-Thread aufgerufen werden — nicht im Audio-Thread.
     */
    void updateMagnitudeResponse(const EQProcessor& eqProcessor)
    {
        const int numBins = fftSize / 2 + 1;
        
        // Temporäres Array für die neue Magnitude-Antwort
        std::vector<float> newResponse(numBins);

        for (int bin = 0; bin < numBins; ++bin)
        {
            float freq = static_cast<float>(bin) * static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);
            if (freq < 1.0f) freq = 1.0f;
            
            // Magnitude in dB von allen EQ-Bändern + Output Gain
            float magnitudeDB = eqProcessor.getTotalMagnitudeForFrequency(freq);
            
            // dB zu linearem Gain
            newResponse[bin] = juce::Decibels::decibelsToGain(magnitudeDB);
        }

        // Atomic-Swap in den Ziel-Buffer (lock-free für Audio-Thread)
        juce::SpinLock::ScopedLockType lock(magnitudeLock);
        targetMagnitudeResponse.swap(newResponse);
        magnitudeResponseDirty.store(true);
    }

    /**
     * Verarbeitet einen Audio-Block mit linearer Phase.
     * Overlap-Add-Verfahren mit 50% Overlap.
     */
    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        // Lade neue Magnitude-Antwort (wenn verfügbar)
        if (magnitudeResponseDirty.load())
        {
            juce::SpinLock::ScopedLockType lock(magnitudeLock);
            currentMagnitudeResponse = targetMagnitudeResponse;
            magnitudeResponseDirty.store(false);
        }

        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(buffer.getNumChannels(), maxChannels);
        
        // Robustheit: Prüfe ob FFT-Engine initialisiert ist
        if (fft == nullptr || fftSize == 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            // Input-Samples in den Ring-Buffer schreiben
            for (int ch = 0; ch < numCh; ++ch)
            {
                inputBuffer[ch].setSample(0, inputWritePos, buffer.getSample(ch, i));
            }

            inputWritePos = (inputWritePos + 1) % fftSize;
            --samplesUntilNextFFT;

            // Wenn genug Samples gesammelt: FFT-Block verarbeiten
            if (samplesUntilNextFFT <= 0)
            {
                processFFTBlock(numCh);
                samplesUntilNextFFT = hopSize;
            }

            // Output aus dem Overlap-Add-Buffer lesen
            for (int ch = 0; ch < numCh; ++ch)
            {
                buffer.setSample(ch, i, outputBuffer[ch].getSample(0, outputReadPos));
                outputBuffer[ch].setSample(0, outputReadPos, 0.0f); // Gelesen → Clear
            }

            outputReadPos = (outputReadPos + 1) % (fftSize * 2);
        }
    }

    bool isEnabled() const { return enabled; }
    void setEnabled(bool shouldBeEnabled) { enabled = shouldBeEnabled; }

private:
    void updateFFTSize()
    {
        switch (latencyMode)
        {
            case LatencyMode::Low:    fftSize = 2048; fftOrder = 11; break;
            case LatencyMode::Medium: fftSize = 4096; fftOrder = 12; break;
            case LatencyMode::High:   fftSize = 8192; fftOrder = 13; break;
        }

        hopSize = fftSize / 2;

        // FFT-Engine erstellen
        fft = std::make_unique<juce::dsp::FFT>(fftOrder);

        // Hann-Fenster erstellen
        window.resize(fftSize);
        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            window.data(), fftSize,
            juce::dsp::WindowingFunction<float>::hann, false);

        // Buffers allokieren
        for (int ch = 0; ch < 2; ++ch)
        {
            inputBuffer[ch].setSize(1, fftSize);
            inputBuffer[ch].clear();
            outputBuffer[ch].setSize(1, fftSize * 2);
            outputBuffer[ch].clear();
            overlapBuffer[ch].setSize(1, fftSize);
            overlapBuffer[ch].clear();
        }

        // FFT-Arbeitsbuffer (Real + Imaginary interleaved, doppelte Größe)
        fftWorkBuffer.resize(fftSize * 2, 0.0f);

        // Magnitude-Response mit Unity initialisieren
        int numBins = fftSize / 2 + 1;
        currentMagnitudeResponse.resize(numBins, 1.0f);
        targetMagnitudeResponse.resize(numBins, 1.0f);

        // Reset Positionen
        inputWritePos = 0;
        outputReadPos = fftSize - hopSize; // Latenz-Kompensation: Start bei -hopSize
        samplesUntilNextFFT = hopSize;
    }

    void processFFTBlock(int numChannels)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // 1. Input-Block aus dem Ring-Buffer extrahieren (ab aktuellem Schreibpunkt - fftSize)
            const int startPos = (inputWritePos + fftSize) % fftSize; // = inputWritePos (nach Modulo)
            
            // Kopiere fftSize Samples in den Arbeitsbuffer
            for (int i = 0; i < fftSize; ++i)
            {
                int readIdx = (startPos + i) % fftSize;
                fftWorkBuffer[i] = inputBuffer[ch].getSample(0, readIdx);
            }

            // 2. Hann-Fenster anwenden
            juce::FloatVectorOperations::multiply(fftWorkBuffer.data(), window.data(), fftSize);

            // 3. Zero-pad den Imaginary-Teil
            for (int i = fftSize; i < fftSize * 2; ++i)
                fftWorkBuffer[i] = 0.0f;

            // 4. FFT vorwärts
            fft->performRealOnlyForwardTransform(fftWorkBuffer.data(), true);

            // 5. Magnitude-Response im Frequenzbereich anwenden (Linear Phase EQ)
            //    Skalierung von Real- und Imaginärteil mit demselben Gain-Faktor
            //    modifiziert nur die Magnitude, nicht die Phase. Die lineare Phase
            //    (= konstante Gruppenlaufzeit von FFT_SIZE/2 Samples) ergibt sich
            //    aus dem symmetrischen Overlap-Add-Verfahren mit Hann-Fensterung.
            const int numBins = fftSize / 2 + 1;
            const int magResponseSize = static_cast<int>(currentMagnitudeResponse.size());

            for (int bin = 0; bin < numBins; ++bin)
            {
                float gain = (bin < magResponseSize) ? currentMagnitudeResponse[bin] : 1.0f;
                
                // Magnitude modifizieren, Original-Signal-Phase beibehalten
                fftWorkBuffer[bin * 2]     *= gain;  // Real
                fftWorkBuffer[bin * 2 + 1] *= gain;  // Imaginary
            }

            // 6. IFFT rückwärts
            fft->performRealOnlyInverseTransform(fftWorkBuffer.data());

            // 7. Overlap-Add in den Output-Buffer
            // KEIN zweites Hann-Fenster: Bei 50% Overlap erfüllt ein einzelnes Hann-Fenster
            // die COLA-Bedingung (Constant Overlap-Add): Hann(n) + Hann(n-N/2) = 1.0.
            // Doppeltes Hann-Fenster mit 50% Overlap wäre NICHT COLA:
            // Hann²(n) + Hann²(n-N/2) = 0.75 + 0.25·cos(4πn/N) ≠ const → Amplitudenmodulation!
            // Die Position im Output-Buffer ist relativ zum aktuellen outputReadPos
            int outputWriteStart = (outputReadPos + hopSize) % (fftSize * 2);
            
            for (int i = 0; i < fftSize; ++i)
            {
                int outIdx = (outputWriteStart + i) % (fftSize * 2);
                float current = outputBuffer[ch].getSample(0, outIdx);
                outputBuffer[ch].setSample(0, outIdx, current + fftWorkBuffer[i]);
            }
        }
    }

    // Parameter
    LatencyMode latencyMode = LatencyMode::Medium;
    bool enabled = false;
    double currentSampleRate = 44100.0;
    int maxChannels = 2;

    // FFT
    std::unique_ptr<juce::dsp::FFT> fft;
    int fftSize = 4096;
    int fftOrder = 12;
    int hopSize = 2048;

    // Buffers (pro Kanal)
    juce::AudioBuffer<float> inputBuffer[2];
    juce::AudioBuffer<float> outputBuffer[2];
    juce::AudioBuffer<float> overlapBuffer[2];

    // Fenster
    std::vector<float> window;

    // FFT-Arbeitsbuffer
    std::vector<float> fftWorkBuffer;

    // Magnitude-Response (linear, pro Bin)
    std::vector<float> currentMagnitudeResponse;   // Audio-Thread Kopie
    std::vector<float> targetMagnitudeResponse;     // GUI-Thread schreibt hier
    juce::SpinLock magnitudeLock;
    std::atomic<bool> magnitudeResponseDirty { false };

    // Ring-Buffer Positionen
    int inputWritePos = 0;
    int outputReadPos = 0;
    int samplesUntilNextFFT = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinearPhaseEQ)
};
