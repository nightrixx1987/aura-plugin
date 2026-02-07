#pragma once

#include <JuceHeader.h>
#include "../Parameters/ParameterIDs.h"

// Anti-Denormal Protection (verhindert CPU-Spikes bei sehr kleinen Werten)
#define ANTI_DENORMAL 1e-20f

/**
 * Biquad-Filter Implementierung basierend auf Robert Bristow-Johnson's Audio EQ Cookbook.
 * Unterstützt alle gängigen Filtertypen für einen parametrischen EQ.
 * 
 * Differenzgleichung:
 * y[n] = (b0/a0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2] - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]
 */
class BiquadFilter
{
public:
    BiquadFilter();
    ~BiquadFilter() = default;

    // Initialisierung mit Sample-Rate
    void prepare(double sampleRate, int samplesPerBlock);
    
    // Filter zurücksetzen (z.B. bei Transport-Stop)
    void reset();

    // Koeffizienten berechnen basierend auf Filtertyp und Parametern
    void updateCoefficients(ParameterIDs::FilterType type, 
                           float frequency, 
                           float gainDB, 
                           float Q,
                           int slope = 12);  // Slope in dB/Oktave (6, 12, 18, 24, 48)

    // Audio verarbeiten (einzelner Sample)
    float processSample(float input) noexcept;
    
    // Audio verarbeiten (Block)
    void processBlock(float* data, int numSamples) noexcept;

    // Frequenzantwort berechnen für GUI-Darstellung
    // Gibt Magnitude in dB zurück für eine gegebene Frequenz
    float getMagnitudeForFrequency(float frequency) const noexcept;
    
    // Phase in Radiant für eine gegebene Frequenz
    float getPhaseForFrequency(float frequency) const noexcept;

    // Getter für aktuelle Einstellungen
    float getFrequency() const { return currentFrequency; }
    float getGain() const { return currentGain; }
    float getQ() const { return currentQ; }
    ParameterIDs::FilterType getType() const { return currentType; }

private:
    // Koeffizienten
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;
    
    // Normalisierte Koeffizienten (dividiert durch a0)
    double nb0 = 1.0, nb1 = 0.0, nb2 = 0.0;
    double na1 = 0.0, na2 = 0.0;

    // Delay-Elemente (Transposed Direct Form II)
    double z1 = 0.0, z2 = 0.0;

    // Parameter Smoothing (verhindert Clicks bei Änderungen)
    double smoothedB0 = 1.0, smoothedB1 = 0.0, smoothedB2 = 0.0;
    double smoothedA1 = 0.0, smoothedA2 = 0.0;
    static constexpr double smoothingCoeff = 0.999; // 99.9% alt, 0.1% neu
    
    // OPTIMIERUNG: Conditional Smoothing - nur smoothen wenn nötig
    bool needsSmoothing = false;
    static constexpr double smoothingEpsilon = 1e-8;  // Schwellwert für Konvergenz
    
    // Flag um zu wissen ob die smoothed Koeffizienten initialisiert sind
    bool coefficientsInitialized = false;

    // Sample-Rate
    double sampleRate = 44100.0;

    // Aktuelle Parameter
    float currentFrequency = 1000.0f;
    float currentGain = 0.0f;
    float currentQ = 0.71f;
    ParameterIDs::FilterType currentType = ParameterIDs::FilterType::Bell;

    // Koeffizienten-Berechnung für verschiedene Filtertypen
    void calculateBellCoefficients(float frequency, float gainDB, float Q);
    void calculateLowShelfCoefficients(float frequency, float gainDB, float Q);
    void calculateHighShelfCoefficients(float frequency, float gainDB, float Q);
    void calculateLowCutCoefficients(float frequency, float Q);
    void calculateHighCutCoefficients(float frequency, float Q);
    void calculateNotchCoefficients(float frequency, float Q);
    void calculateBandPassCoefficients(float frequency, float Q);
    void calculateTiltShelfCoefficients(float frequency, float gainDB);
    void calculateAllPassCoefficients(float frequency, float Q);
    void calculateFlatTiltCoefficients(float frequency, float gainDB);

    // Hilfsfunktion: Koeffizienten normalisieren
    void normalizeCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BiquadFilter)
};
