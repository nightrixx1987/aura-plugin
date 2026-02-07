#pragma once

#include <JuceHeader.h>
#include "../DSP/EQProcessor.h"

/**
 * Spectrum Grab Tool - Klicken Sie auf Spektrum-Peaks, um automatisch EQ-Bänder zu erstellen
 * Inspiriert von FabFilter Pro-Q 4's Spectrum Grab Feature
 * 
 * Features:
 * - Einfaches Klicken auf Problem-Frequenzen
 * - Automatische Frequenz-, Gain- und Q-Erkennung
 * - Intelligente Filtertyp-Auswahl
 * - Peak-Breiten-Analyse für optimale Q-Werte
 */
class SpectrumGrabTool : public juce::Component
{
public:
    SpectrumGrabTool(EQProcessor& processor);
    ~SpectrumGrabTool() override = default;

    void paint(juce::Graphics& g) override;
    
    // Mouse-Interaktion
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    
    // Aktivierung
    void setGrabMode(bool enabled);
    bool isGrabModeActive() const { return grabMode; }
    
    // Spektrum-Daten für Analyse
    void updateSpectrumData(const std::vector<float>& magnitudes, 
                           float minFreq, float maxFreq);
    
    // Optionen
    void setAutoDetectFilterType(bool shouldAuto);
    void setDefaultBoostGain(float gain);  // Standard-Gain für Boost
    void setDefaultCutGain(float gain);    // Standard-Gain für Cut
    void setIntelligentQMode(bool enabled);  // Q basierend auf Peak-Breite

    // Callback fuer Band-Erstellung (wird im Editor gesetzt, nutzt APVTS)
    std::function<void(int bandIndex, float frequency, float gain, float q, int filterType)> onBandGrabbed;

private:
    EQProcessor& eqProcessor;
    
    // Status
    bool grabMode = false;
    juce::Point<float> mousePosition;
    
    // Spektrum-Daten
    std::vector<float> spectrumMagnitudes;
    float spectrumMinFreq = 20.0f;
    float spectrumMaxFreq = 20000.0f;
    
    // Optionen
    bool autoDetectFilterType = true;
    bool intelligentQMode = true;
    float defaultBoostGain = 3.0f;
    float defaultCutGain = -6.0f;
    
    // Hilfsfunktionen
    float frequencyToX(float frequency) const;
    float xToFrequency(float x) const;
    
    struct PeakInfo
    {
        float frequency;
        float magnitude;
        float qFactor;
        bool isBoost;  // true = Boost, false = Cut
    };
    
    PeakInfo analyzePeakAtPosition(const juce::Point<float>& pos);
    float calculateQFromPeakWidth(int peakIndex);
    int findNearestPeakIndex(float targetFreq);
    int findInactiveBand();
    
    // Visuals
    juce::Colour grabCursorColour = juce::Colours::orange;
    float cursorSize = 12.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumGrabTool)
};
