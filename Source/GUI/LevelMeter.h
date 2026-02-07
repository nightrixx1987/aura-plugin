#pragma once

#include <JuceHeader.h>

/**
 * LevelMeter: Audio-Pegel-Anzeige (Pro-Q4 Style)
 * Vertikal schmalles Levelometer mit Farbcodierung
 * 
 * Features:
 * - Echtzeit-Pegelmessung
 * - Farbcodierung: Grün (normal), Gelb (warnung), Orange/Rot (Clipping)
 * - Peak-Hold mit Decay
 * - dB-Skalierung (-∞ bis +12 dB)
 * - Pro-Audio-Design (FabFilter Pro-Q4 inspiriert)
 */
class LevelMeter : public juce::Component,
                   public juce::Timer
{
public:
    //==========================================================================
    // Pegel-Bereiche und Farben (Pro-Q4 Style)
    //==========================================================================
    static constexpr float MIN_DB = -80.0f;
    static constexpr float MAX_DB = 12.0f;
    static constexpr float GREEN_LIMIT = -18.0f;      // Grün bis -18 dB
    static constexpr float YELLOW_LIMIT = -6.0f;      // Gelb bis -6 dB
    static constexpr float ORANGE_LIMIT = 0.0f;       // Orange bis 0 dB
    static constexpr float RED_LIMIT = 6.0f;          // Rot bis +6 dB
    
    LevelMeter();
    ~LevelMeter() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    //==========================================================================
    // Pegel aktualisieren
    //==========================================================================
    void setLevel(float leftDB, float rightDB);
    void updateLevelFromBuffer(const juce::AudioBuffer<float>& buffer);
    
    //==========================================================================
    // Getter
    //==========================================================================
    float getCurrentLevelLeft() const { return currentLevelLeftDB; }
    float getCurrentLevelRight() const { return currentLevelRightDB; }
    float getPeakLevelLeft() const { return peakLevelLeftDB; }
    float getPeakLevelRight() const { return peakLevelRightDB; }

private:
    //==========================================================================
    // Timer für Decay und Rendering
    //==========================================================================
    void timerCallback() override;
    
    //==========================================================================
    // Hilfsfunktionen
    //==========================================================================
    float calculateRMS(const float* data, int numSamples);
    juce::Colour getColorForLevel(float levelDB);
    int dbToPixels(float levelDB) const;
    void drawChannel(juce::Graphics& g, juce::Rectangle<int> channelBounds, float levelDB, float peakDB);
    juce::String formatLevelText(float levelDB);
    
    //==========================================================================
    // Member-Variablen
    //==========================================================================
    float currentLevelLeftDB = -80.0f;
    float currentLevelRightDB = -80.0f;
    float peakLevelLeftDB = -80.0f;
    float peakLevelRightDB = -80.0f;
    double peakHoldTimeRemainingLeft = 0.0;
    double peakHoldTimeRemainingRight = 0.0;
    
    // Einstellungen
    double peakHoldDuration = 2.0;  // 2 Sekunden
    float decayRate = 40.0f;         // 40 dB pro Sekunde
    
    // Farben (Pro-Q4 Style)
    const juce::Colour colorGreen = juce::Colour(0xff4caf50);     // Grün
    const juce::Colour colorYellow = juce::Colour(0xfffdd835);    // Gelb
    const juce::Colour colorOrange = juce::Colour(0xffffa726);    // Orange
    const juce::Colour colorRed = juce::Colour(0xffe53935);       // Rot (Clipping)
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
