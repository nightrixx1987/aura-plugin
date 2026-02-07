#pragma once

#include <JuceHeader.h>
#include "EQBand.h"
#include "../Parameters/ParameterIDs.h"

/**
 * EQProcessor: Hauptklasse für die komplette EQ-Verarbeitung.
 * Verwaltet alle EQ-Bänder und koordiniert die Audio-Verarbeitung.
 */
class EQProcessor
{
public:
    EQProcessor();
    ~EQProcessor() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // Audio verarbeiten
    void processBlock(juce::AudioBuffer<float>& buffer);

    // Zugriff auf einzelne Bänder
    EQBand& getBand(int index);
    const EQBand& getBand(int index) const;
    int getNumBands() const { return ParameterIDs::MAX_BANDS; }

    // Gesamte Frequenzantwort berechnen
    float getTotalMagnitudeForFrequency(float frequency) const;
    
    // Frequenzantwort für ein Array von Frequenzen berechnen (effizienter)
    void getMagnitudeResponse(const float* frequencies, float* magnitudes, int numPoints) const;

    // Output Gain
    void setOutputGain(float gainDB);
    float getOutputGain() const { return outputGainDB; }

    // Linear Phase Mode (für Mastering)
    void setLinearPhaseEnabled(bool enabled);
    bool isLinearPhaseEnabled() const { return linearPhaseEnabled; }

    // Band Copy/Paste für Workflow
    void copyBandSettings(int sourceBandIndex);
    void pasteBandSettings(int targetBandIndex);

    // Input Gain
    void setInputGain(float gainDB);
    float getInputGain() const { return inputGainDB; }

private:
    std::array<EQBand, ParameterIDs::MAX_BANDS> bands;
    
    float outputGainDB = 0.0f;
    float outputGainLinear = 1.0f;
    float inputGainDB = 0.0f;
    float inputGainLinear = 1.0f;
    
    bool linearPhaseEnabled = false;
    
    // Für Band Copy/Paste
    struct BandSnapshot
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 0.71f;
        ParameterIDs::FilterType type = ParameterIDs::FilterType::Bell;
    };
    BandSnapshot copiedBandData;
    
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQProcessor)
};
