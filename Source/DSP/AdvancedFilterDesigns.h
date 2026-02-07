#pragma once

#include <JuceHeader.h>
#include <vector>
#include <complex>

/**
 * AdvancedFilterDesigns: Erweiterte Filteralgorithmen für professionelle Audio-Verarbeitung.
 * Enthält Linear Phase Filter, Phase-Matching und weitere Algorithmen.
 */
class AdvancedFilterDesigns
{
public:
    /**
     * Linear Phase FIR Filter Design
     * Erzeugt lineare Phase mit verzögertem Ausgang (kein Phasenverzerrung)
     */
    class LinearPhaseFIR
    {
    public:
        LinearPhaseFIR();

        // Designen des Filters basierend auf gewünschter Frequenzantwort
        void designFromMagnitudeResponse(const float* targetMagnitudes, int numFreqs, int filterOrder);

        // Filter anwenden
        float processSample(float sample);
        void processBlock(juce::AudioBuffer<float>& buffer);

        // Latenz (in Samples)
        int getLatency() const;

        void reset();
        void prepare(double sampleRate, int blockSize);

    private:
        std::vector<float> coefficients;
        std::vector<float> delayLine;
        int delayIndex = 0;
        int filterOrder = 256;
        double sampleRate = 44100.0;
    };

    /**
     * Phase-Matching Algorithmus
     * Gleicht die Phase zwischen mehreren Bändern ab für kohärentes Mixa
     */
    class PhaseMatching
    {
    public:
        // Berechne Phasendifferenzen zwischen Referenz und Signal
        static void alignPhases(juce::AudioBuffer<float>& signal,
                               const juce::AudioBuffer<float>& reference);

        // Glättung von Phasenkurven (lokale Regression)
        static std::vector<float> smoothPhaseResponse(const std::vector<float>& phaseValues, int windowSize = 5);
    };

    /**
     * Zero-Latency Linear Phase Approximate
     * Verwendet Allpass-Filter zur Phase-Korrektur in Echtzeit (mit minimalem Latenzverzug)
     */
    class ZeroLatencyLinearPhaseApprox
    {
    public:
        ZeroLatencyLinearPhaseApprox();

        void prepare(double sampleRate, int blockSize);
        float processSample(float sample);
        void processBlock(juce::AudioBuffer<float>& buffer);
        void reset();

        // Minimale Latenz in Samples (typisch 64-256 Samples)
        int getLatency() const { return approximationLatency; }

    private:
        // Allpass-Filter-Koeffizienten für Phase-Matching
        struct AllpassStage
        {
            float a1 = 0.0f;
            float z1 = 0.0f;

            float process(float sample)
            {
                float output = -sample + a1 * (sample + z1);
                z1 = output;
                return output;
            }

            void reset() { z1 = 0.0f; }
        };

        std::vector<AllpassStage> allpassFilters;
        int approximationLatency = 64;
        double sampleRate = 44100.0;
    };

    /**
     * Minimum Phase zu Linear Phase Converter
     * Konvertiert Minimum-Phase EQ (typisch in IIR-Filtern) zu Linear Phase
     * durch zusätzliche Hilbert-Transform für Phasenkorrektur
     */
    class MinimumToLinearPhaseConverter
    {
    public:
        // Berechne die fehlende Phase basierend auf Magnitude-Response
        static std::vector<float> computeLinearPhaseFromMagnitude(
            const std::vector<float>& magnitudeResponseDB,
            int fftSize = 8192
        );

        // Glätte Phasenkurve (mit Kramers-Kronig-Beziehung)
        static std::vector<float> computeMinimumPhaseFromMagnitude(
            const std::vector<float>& magnitudeResponseDB
        );
    };

    /**
     * Dynamic Range Expander/Compressor für sanfte Ecken bei extremen EQ-Werten
     * Verhindert Übersteuerung bei starken EQ-Kurven
     */
    class DynamicSafetyLimiter
    {
    public:
        void prepare(double sampleRate);
        void processSample(float& sample);
        void setThreshold(float thresholdDB) { thresholdDB_ = thresholdDB; }

    private:
        float thresholdDB_ = 12.0f;
        float releaseTime = 100.0f; // ms
        float envelope = 0.0f;
        double sampleRate = 44100.0;
    };
};

/**
 * FFT-basierte Spektrumanalyse für hochpräzise Band-Erstellung
 */
class SpectrumAnalysisFFT
{
public:
    void prepare(int fftSize = 8192);

    // Analyse-Block
    void analyzeBlock(const juce::AudioBuffer<float>& buffer);

    // Peak-Frequenzen finden
    std::vector<float> findPeaks(int numPeaks = 5, float minMagnitudeDB = -40.0f);

    // Magnitude an spezifischer Frequenz
    float getMagnitudeAtFrequency(float frequency);

    // Gesamtes Spektrum auslesen
    const std::vector<float>& getSpectrum() const { return magnitudeSpectrum; }

private:
    juce::dsp::FFT fft;
    std::vector<float> fftBuffer;
    std::vector<float> magnitudeSpectrum;
    std::vector<float> phaseSpectrum;
    double sampleRate = 44100.0;
};
