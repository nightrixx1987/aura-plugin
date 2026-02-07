#pragma once

#include <JuceHeader.h>
#include "BiquadFilter.h"
#include "SVFFilter.h"
#include "../Parameters/ParameterIDs.h"

/**
 * EQ-Band: Verwaltet einen einzelnen EQ-Punkt mit allen zugehörigen Parametern
 * und Filtern für Stereo/Mid-Side-Verarbeitung.
 */
class EQBand
{
public:
    EQBand();
    ~EQBand() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // Parameter setzen
    void setFrequency(float frequency);
    void setGain(float gainDB);
    void setQ(float Q);
    void setType(ParameterIDs::FilterType type);
    void setChannelMode(ParameterIDs::ChannelMode mode);
    void setBypassed(bool bypassed);
    void setSlope(int slopeDB);  // 6, 12, 18, 24, 48 dB/Oct
    
    // Dynamic EQ Parameters (NEW)
    void setDynamicMode(bool enabled);
    void setThreshold(float thresholdDB);
    void setRatio(float ratio);
    void setAttack(float attackMs);
    void setRelease(float releaseMs);

    // Alle Parameter auf einmal setzen
    void setParameters(float frequency, float gainDB, float Q, 
                       ParameterIDs::FilterType type,
                       ParameterIDs::ChannelMode channelMode = ParameterIDs::ChannelMode::Stereo,
                       bool bypassed = false);

    // Audio verarbeiten (Stereo)
    void processBlock(juce::AudioBuffer<float>& buffer);

    // Frequenzantwort für GUI
    float getMagnitudeForFrequency(float frequency) const;

    // Getter
    float getFrequency() const { return frequency; }
    float getGain() const { return gain; }
    float getQ() const { return q; }
    ParameterIDs::FilterType getType() const { return filterType; }
    ParameterIDs::ChannelMode getChannelMode() const { return channelMode; }
    bool isBypassed() const { return bypassed; }
    bool isActive() const { return active; }
    void setActive(bool isActive) { active = isActive; }
    
    // Dynamic EQ Getters (NEW)
    bool isDynamicMode() const { return dynamicMode; }
    float getThreshold() const { return threshold; }
    float getRatio() const { return ratio; }
    float getAttack() const { return attack; }
    float getRelease() const { return release; }
    float getDynamicGainReduction() const { return dynamicGainReduction; }  // Für Metering
    
    // Signal-Level in dB (für GUI: Level-Indikator am Handle)
    float getEnvelopeLevelDB() const
    {
        float env = juce::jmax(envelopeLeft, envelopeRight);
        if (env < 1e-10f) return -100.0f;
        return 10.0f * std::log10(env + 1e-10f);
    }

private:
    // Filter für jeden Kanal (L, R oder Mid, Side)
    // Mehrere Filter für höhere Slopes (kaskadiert)
    static constexpr int MAX_CASCADE = 8;  // Bis zu 96 dB/Oct (8 x 12 dB)
    static constexpr int DYNAMIC_UPDATE_INTERVAL = 32;  // Koeffizienten-Update alle N Samples (Performance!)
    
    std::array<BiquadFilter, MAX_CASCADE> filtersLeft;
    std::array<BiquadFilter, MAX_CASCADE> filtersRight;
    
    // SVF-Filter für Dynamic EQ (modulationsstabil — kein Zipper-Rauschen)
    SVFFilter svfLeft;
    SVFFilter svfRight;
    
    // Parameter
    float frequency = 1000.0f;
    float gain = 0.0f;
    float q = 0.71f;
    ParameterIDs::FilterType filterType = ParameterIDs::FilterType::Bell;
    ParameterIDs::ChannelMode channelMode = ParameterIDs::ChannelMode::Stereo;
    int slope = 12;  // dB/Oktave
    bool bypassed = false;
    bool active = true;
    
    // Dynamic EQ Parameters (NEW)
    bool dynamicMode = false;
    float threshold = 0.0f;       // dBFS
    float ratio = 1.0f;           // 1:1 bis 10:1
    float attack = 10.0f;         // ms
    float release = 100.0f;       // ms
    
    // Envelope Follower für Dynamic EQ
    float envelopeLeft = 0.0f;
    float envelopeRight = 0.0f;
    float dynamicGainReduction = 0.0f;
    
    // OPTIMIERUNG: Gecachte Koeffizienten (berechnet in prepare/setAttack/setRelease)
    float cachedAttackCoeff = 0.0f;
    float cachedReleaseCoeff = 0.0f;
    float lastDynamicGain = 1.0f;  // Für Koeffizienten-Änderungserkennung

    double currentSampleRate = 44100.0;
    int numCascadeStages = 1;

    // Koeffizienten aktualisieren
    void updateFilters();
    void updateEnvelopeCoefficients();  // OPTIMIERUNG: Envelope-Koeffizienten cachen

    // Mid/Side Encoding/Decoding
    void encodeToMidSide(float& left, float& right);
    void decodeFromMidSide(float& mid, float& side);
    
    // Dynamic EQ Processing (NEW)
    float processEnvelope(float input, float& envelope);
    float calculateDynamicGain(float envelope);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQBand)
};
