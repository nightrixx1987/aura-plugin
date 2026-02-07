#pragma once

#include <JuceHeader.h>

/**
 * FFTAnalyzer: Echtzeit-Spektrumanalyse mit FFT.
 * Verwendet JUCE's dsp::FFT für die Transformation.
 *
 * Pro-Q3/Pro-Q4 Style Features:
 * - Konfigurierbare FFT-Auflösung (1024-8192)
 * - Spektrum-Tilt-Kompensation (4.5 dB/Oktave Standard)
 * - Asymmetrisches Smoothing (schneller Attack, langsamer Release)
 * - Freeze-Modus
 */
class FFTAnalyzer
{
public:
    //==========================================================================
    // FFT-Auflösung (Pro-Q Style)
    //==========================================================================
    enum class FFTResolution
    {
        Low = 10,      // 1024 Punkte
        Medium = 11,   // 2048 Punkte (Standard)
        High = 12,     // 4096 Punkte
        Maximum = 13   // 8192 Punkte
    };

    //==========================================================================
    // Analyzer-Geschwindigkeit (Pro-Q Style)
    //==========================================================================
    enum class AnalyzerSpeed
    {
        VerySlow,   // Sehr langsamer Release
        Slow,       // Langsamer Release
        Medium,     // Standard
        Fast,       // Schneller Release
        VeryFast    // Sehr schneller Release
    };

    // Maximale FFT-Größe für Buffer-Allokation
    static constexpr int MAX_FFT_ORDER = 13;
    static constexpr int MAX_FFT_SIZE = 1 << MAX_FFT_ORDER;  // 8192
    static constexpr int MAX_NUM_BINS = MAX_FFT_SIZE / 2 + 1; // 4097

    FFTAnalyzer();
    ~FFTAnalyzer() = default;

    void prepare(double sampleRate);
    void reset();

    //==========================================================================
    // Sample-Eingabe
    //==========================================================================
    void pushSamples(const float* samples, int numSamples);
    void pushBuffer(const juce::AudioBuffer<float>& buffer);

    // FFT durchführen, wenn genug Samples vorhanden
    void processFFT();

    //==========================================================================
    // Spektrum-Daten abrufen
    //==========================================================================
    const std::vector<float>& getMagnitudes() const { return smoothedMagnitudes; }

    // Frequenz für einen Bin berechnen
    float getFrequencyForBin(int bin) const;

    // Bin für eine Frequenz berechnen
    int getBinForFrequency(float frequency) const;

    // Magnitude für eine bestimmte Frequenz (interpoliert, mit Tilt)
    float getMagnitudeForFrequency(float frequency) const;

    // Magnitude ohne Tilt-Kompensation
    float getRawMagnitudeForFrequency(float frequency) const;

    //==========================================================================
    // FFT-Auflösung (Pro-Q Style)
    //==========================================================================
    void setResolution(FFTResolution resolution);
    FFTResolution getResolution() const { return currentResolution; }
    int getCurrentFFTSize() const { return currentFFTSize; }
    int getCurrentNumBins() const { return currentNumBins; }

    //==========================================================================
    // Spektrum-Tilt-Kompensation (Pro-Q Style: 4.5 dB/Oktave um 1kHz)
    //==========================================================================
    void setTiltSlope(float slopeDbPerOctave) { tiltSlope = juce::jlimit(-12.0f, 12.0f, slopeDbPerOctave); }
    float getTiltSlope() const { return tiltSlope; }

    void setTiltCenterFrequency(float freq) { tiltCenterFreq = juce::jlimit(100.0f, 10000.0f, freq); }
    float getTiltCenterFrequency() const { return tiltCenterFreq; }

    void setTiltEnabled(bool enabled) { tiltEnabled = enabled; }
    bool isTiltEnabled() const { return tiltEnabled; }

    //==========================================================================
    // Analyzer-Geschwindigkeit (Pro-Q Style)
    //==========================================================================
    void setSpeed(AnalyzerSpeed speed);
    AnalyzerSpeed getSpeed() const { return currentSpeed; }

    // Benutzerdefiniertes Smoothing
    void setCustomSmoothing(float attack, float release);
    float getAttackCoeff() const { return attackCoeff; }
    float getReleaseCoeff() const { return releaseCoeff; }

    //==========================================================================
    // Freeze-Modus (Pro-Q Style)
    //==========================================================================
    void setFrozen(bool freeze);
    bool isFrozen() const { return frozen.load(); }

    //==========================================================================
    // Allgemeine Einstellungen
    //==========================================================================
    void setSmoothingFactor(float factor) { releaseCoeff = juce::jlimit(0.0f, 0.99f, factor); }
    void setFloorDB(float floor) { floorDB = floor; }
    float getFloorDB() const { return floorDB; }

    // Prüfen ob neue Daten verfügbar sind
    bool hasNewData() const { return newDataAvailable; }
    void clearNewDataFlag() { newDataAvailable = false; }

    // Aktuelle Sample-Rate
    double getSampleRate() const { return sampleRate; }

private:
    //==========================================================================
    // Tilt-Kompensation berechnen
    //==========================================================================
    float applyTiltCompensation(float frequency, float magnitudeDb) const;

    //==========================================================================
    // FFT-Objekte (dynamisch für variable Auflösung)
    //==========================================================================
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    // Aktuelle Auflösung
    FFTResolution currentResolution = FFTResolution::Medium;
    int currentFFTOrder = 11;
    int currentFFTSize = 2048;
    int currentNumBins = 1025;

    //==========================================================================
    // FIFO für eingehende Samples (max Größe alloziert)
    //==========================================================================
    std::vector<float> fifo;
    int fifoIndex = 0;
    bool fifoReady = false;

    //==========================================================================
    // FFT-Daten (dynamisch basierend auf Auflösung)
    //==========================================================================
    std::vector<float> fftData;
    std::vector<float> magnitudes;
    std::vector<float> smoothedMagnitudes;
    std::vector<float> frozenMagnitudes;  // Für Freeze-Modus

    //==========================================================================
    // Tilt-Kompensation Parameter
    //==========================================================================
    float tiltSlope = 4.5f;           // dB pro Oktave (Pro-Q Standard)
    float tiltCenterFreq = 1000.0f;   // Referenzfrequenz
    bool tiltEnabled = false;

    //==========================================================================
    // Smoothing Parameter (asymmetrisch)
    //==========================================================================
    AnalyzerSpeed currentSpeed = AnalyzerSpeed::Medium;
    float attackCoeff = 0.5f;    // Schneller Attack (kleinerer Wert = schneller)
    float releaseCoeff = 0.85f;  // Langsamer Release (größerer Wert = langsamer)

    //==========================================================================
    // Freeze-Modus
    //==========================================================================
    std::atomic<bool> frozen { false };

    //==========================================================================
    // Allgemeine Parameter
    //==========================================================================
    double sampleRate = 44100.0;
    float floorDB = -100.0f;

    std::atomic<bool> newDataAvailable { false };
    
    // SpinLock fuer thread-sichere Resolution-Aenderungen
    // (schuetzt vor Data Race wenn setResolution im GUI-Thread
    //  gleichzeitig mit pushSamples im Audio-Thread laeuft)
    juce::SpinLock resolutionLock;

    //==========================================================================
    // Interne Hilfsfunktionen
    //==========================================================================
    void reallocateBuffers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFTAnalyzer)
};
