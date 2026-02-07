#pragma once

#include <JuceHeader.h>
#include "../DSP/FFTAnalyzer.h"
#include "CustomLookAndFeel.h"

/**
 * SpectrumAnalyzer: Pro-Q3/Pro-Q4 Style Spektrum-Visualisierung.
 *
 * Features:
 * - Konfigurierbare dB-Range (60/90/120 dB)
 * - Spektrum-Tilt-Kompensation
 * - Pre/Post EQ Visualisierung
 * - Hover-Frequenzanzeige
 * - Peak-Detection und Labels
 * - Pre-allozierte Buffers für Performance
 */
class SpectrumAnalyzer : public juce::Component,
                         public juce::Timer
{
public:
    //==========================================================================
    // dB-Range Einstellungen (Pro-Q Style)
    //==========================================================================
    enum class DBRange
    {
        Range60dB,   // -60 bis 0 dB
        Range90dB,   // -90 bis 0 dB (Pro-Q Standard)
        Range120dB   // -120 bis 0 dB
    };

    //==========================================================================
    // Peak-Information für Labels
    //==========================================================================
    struct PeakInfo
    {
        float frequency = 0.0f;
        float magnitude = -100.0f;
        float x = 0.0f;
        float y = 0.0f;
        bool valid = false;
    };

    //==========================================================================
    // Settings-Struktur für alle Analyzer-Einstellungen
    //==========================================================================
    struct Settings
    {
        DBRange range = DBRange::Range90dB;
        bool showPeakLabels = true;
        bool showHoverInfo = true;
        bool showGrid = true;
        bool showFrequencyLabels = true;
    };

    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override;
    
    // Timer-Kontrolle - muss nach vollständiger Initialisierung aufgerufen werden
    void startAnalyzer() { startTimerHz(60); }
    void stopAnalyzer() { stopTimer(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    //==========================================================================
    // Mouse-Events für Hover-Anzeige
    //==========================================================================
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    //==========================================================================
    // FFT-Analyzer setzen
    //==========================================================================
    void setAnalyzer(FFTAnalyzer* preAnalyzer, FFTAnalyzer* postAnalyzer = nullptr);

    //==========================================================================
    // Anzeige-Einstellungen
    //==========================================================================
    void setShowPre(bool show) { showPre = show; }
    bool getShowPre() const { return showPre; }

    void setShowPost(bool show) { showPost = show; }
    bool getShowPost() const { return showPost; }

    void setEnabled(bool enabled);
    bool isAnalyzerEnabled() const { return isEnabled; }

    //==========================================================================
    // dB-Range (Pro-Q Style: 60/90/120 dB)
    //==========================================================================
    void setDBRange(DBRange range);
    DBRange getDBRange() const { return settings.range; }

    //==========================================================================
    // Settings
    //==========================================================================
    void setSettings(const Settings& newSettings);
    const Settings& getSettings() const { return settings; }

    void setShowPeakLabels(bool show) { settings.showPeakLabels = show; }
    void setShowHoverInfo(bool show) { settings.showHoverInfo = show; }
    void setShowGrid(bool show) { settings.showGrid = show; }
    
    //==========================================================================
    // Reference Spectrum (für Vergleich mit geladenen Audio-Dateien)
    //==========================================================================
    void setReferenceSpectrumEnabled(bool enabled) { showReferenceSpectrum = enabled; }
    bool isReferenceSpectrumEnabled() const { return showReferenceSpectrum; }
    void setReferenceSpectrum(const std::vector<float>& magnitudes) { referenceSpectrumData = magnitudes; }
    
    //==========================================================================
    // NEU: Match Curve Overlay (Korrektur-Kurve vom SpectralMatcher)
    //==========================================================================
    void setMatchCurveEnabled(bool enabled) { showMatchCurve = enabled; }
    bool isMatchCurveEnabled() const { return showMatchCurve; }
    void setMatchCurve(const std::vector<float>& correctionDb) { matchCurveData = correctionDb; }
    void clearMatchCurve() { matchCurveData.clear(); showMatchCurve = false; }

    //==========================================================================
    // Soothe/Suppressor Visualization
    //==========================================================================
    void setSootheCurveEnabled(bool enabled) { showSootheCurve = enabled; }
    bool isSootheCurveEnabled() const { return showSootheCurve; }
    void setSootheCurveData(const float* gainReductions, int numBins, double sampleRate, int fftSize);

    //==========================================================================
    // Koordinaten-Konvertierung (für externe Komponenten wie EQCurve)
    //==========================================================================
    float frequencyToX(float frequency) const;
    float xToFrequency(float x) const;

    // Spektrum dB zu Y (0 dB oben, -range dB unten)
    float spectrumDbToY(float db) const;
    float yToSpectrumDb(float y) const;

    // EQ dB zu Y (±12 dB um 0 dB in der Mitte, für EQ-Kurve Overlay)
    float eqDbToY(float db) const;
    float yToEqDb(float y) const;

    //==========================================================================
    // Frequenz- und dB-Bereich
    //==========================================================================
    void setFrequencyRange(float minHz, float maxHz);
    void setEQDecibelRange(float minDB, float maxDB);

    float getMinFrequency() const { return minFreq; }
    float getMaxFrequency() const { return maxFreq; }
    float getSpectrumMinDB() const { return spectrumMinDB; }
    float getSpectrumMaxDB() const { return spectrumMaxDB; }
    float getEQMinDB() const { return eqMinDB; }
    float getEQMaxDB() const { return eqMaxDB; }

    //==========================================================================
    // Rechter Margin für dB-Skala
    //==========================================================================
    int getRightMargin() const { return rightMargin; }

private:
    FFTAnalyzer* preFFT = nullptr;
    FFTAnalyzer* postFFT = nullptr;

    bool showPre = true;
    bool showPost = true;
    bool isEnabled = true;

    Settings settings;

    //==========================================================================
    // Frequenz- und dB-Bereich
    //==========================================================================
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;

    // Spektrum-Anzeige (basierend auf DBRange)
    float spectrumMinDB = -90.0f;
    float spectrumMaxDB = 0.0f;

    // EQ-Grid: ±36 dB (für EQ-Kurve Overlay - muss mit EQCurveComponent übereinstimmen!)
    float eqMinDB = -36.0f;
    float eqMaxDB = 36.0f;

    // Layout
    static constexpr int rightMargin = 50;

    //==========================================================================
    // Hover-State
    //==========================================================================
    bool mouseIsOver = false;
    juce::Point<float> mousePosition;
    float hoveredFrequency = 0.0f;
    float hoveredSpectrumDb = -100.0f;

    //==========================================================================
    // Peak-Detection
    //==========================================================================
    static constexpr int MAX_PEAKS = 5;
    std::array<PeakInfo, MAX_PEAKS> detectedPeaks;

    //==========================================================================
    // Pre-allozierte Buffers für Performance (WICHTIG!)
    //==========================================================================
    std::vector<float> preYValues;
    std::vector<float> postYValues;
    std::vector<float> smoothingTemp;
    int lastWidth = 0;
    static constexpr int OVERSAMPLE = 4;

    //==========================================================================
    // Spektrum-Pfade
    //==========================================================================
    juce::Path preSpectrumPath;
    juce::Path postSpectrumPath;
    juce::Path referenceSpectrumPath;
    
    //==========================================================================
    // Reference Spectrum
    //==========================================================================
    bool showReferenceSpectrum = false;
    std::vector<float> referenceSpectrumData;
    std::vector<float> referenceYValues;
    
    //==========================================================================
    // NEU: Match/Correction Curve
    //==========================================================================
    bool showMatchCurve = false;
    std::vector<float> matchCurveData;
    std::vector<float> matchCurveYValues;
    juce::Path matchCurvePath;

    //==========================================================================
    // Soothe/Suppressor Visualization
    //==========================================================================
    bool showSootheCurve = false;
    juce::Path sootheCurvePath;
    std::vector<float> sootheCurvePoints;  // x -> gainReduction in dB

    //==========================================================================
    // Interne Methoden
    //==========================================================================
    void allocateBuffers(int width);
    void updatePaths();
    void updateSinglePath(FFTAnalyzer* fft, juce::Path& path, std::vector<float>& yValues);
    void updateReferenceSpectrumPath();
    void updateMatchCurvePath();  // NEU
    void detectPeaks();

    void drawGrid(juce::Graphics& g);
    void drawSpectrum(juce::Graphics& g, const juce::Path& path, juce::Colour colour, bool isPre = false);
    void drawMatchCurve(juce::Graphics& g);  // NEU: Zeichnet Match-Korrekturkurve
    void drawSootheCurve(juce::Graphics& g); // NEU: Zeichnet Soothe Gain-Reduktion
    void drawHoverInfo(juce::Graphics& g);
    void drawPeakLabels(juce::Graphics& g);
    void drawDualScales(juce::Graphics& g);
    void drawLegend(juce::Graphics& g);

    // Hilfsfunktionen
    juce::String formatFrequency(float freq) const;
    juce::String formatDb(float db) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
