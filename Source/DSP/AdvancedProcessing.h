#pragma once

#include <JuceHeader.h>

/**
 * MidSideProcessor: Ermöglicht Mid/Side EQ-Verarbeitung.
 * Wichtige Mastering-Funktion für Stereo-Kontrolle.
 * 
 * Mid = L+R, Side = L-R
 * Ermöglicht separate EQ-Behandlung der Center- und Stereo-Information
 */
class MidSideProcessor
{
public:
    MidSideProcessor();

    enum class ProcessingMode
    {
        Stereo,    // Normale L/R Verarbeitung
        MidSide,   // Separate Mid/Side Verarbeitung
        MidOnly,   // Nur Mid-Kanal
        SideOnly   // Nur Side-Kanäle
    };

    void setMode(ProcessingMode newMode);
    ProcessingMode getMode() const { return mode; }

    // Mid/Side-Kodierung
    void encodeToMidSide(float& left, float& right) const;
    void decodeFromMidSide(float& mid, float& side) const;

    // Stereo-Verarbeitung mit Mid/Side Option
    void processMidSide(juce::AudioBuffer<float>& buffer);

    void prepare(double sampleRate, int blockSize);
    void reset();

private:
    ProcessingMode mode = ProcessingMode::Stereo;
    double sampleRate = 44100.0;
};

/**
 * SpectralShaper: Einfache spektrale Bearbeitung für bessere Frequenz-Kontrolle.
 * Basiert auf Band-orientierten Filterbank.
 */
class SpectralShaper
{
public:
    struct SpectralBand
    {
        float centerFreq = 1000.0f;
        float gain = 0.0f;
        float bandwidth = 1.0f;
        bool active = false;
    };

    SpectralShaper();

    // Spektrale Bänder setzen (vereinfachte Spektral-EQ)
    void setBand(int bandIndex, const SpectralBand& band);
    const SpectralBand& getBand(int bandIndex) const;

    void processBock(juce::AudioBuffer<float>& buffer);
    void prepare(double sampleRate, int blockSize);

    static constexpr int NUM_SPECTRAL_BANDS = 8;  // Mehr als Standard-EQ

private:
    std::array<SpectralBand, NUM_SPECTRAL_BANDS> bands;
    double sampleRate = 44100.0;
};

/**
 * TransientPreserver: Erhält Transienten bei aggressiver Filterung.
 * Wichtig für Drums und perkussive Instrumente.
 */
class TransientPreserver
{
public:
    TransientPreserver();

    void setAmount(float amount); // 0.0 = aus, 1.0 = 100% Transient-Erhalt
    float getAmount() const { return preserveAmount; }

    void process(juce::AudioBuffer<float>& buffer);
    void prepare(double sampleRate, int blockSize);
    void reset();

private:
    float preserveAmount = 0.0f;
    double sampleRate = 44100.0;

    // Transient-Detektion basierend auf Amplituden-Derivaten
    float detectTransient(float sample);
    float lastSample = 0.0f;
    float transientThreshold = 0.1f;
};

/**
 * ParallelProcessing: Ermöglicht parallele Verarbeitung (Wet/Dry Blending)
 * Wichtig für subtile EQ-Anwendungen
 */
class ParallelProcessor
{
public:
    ParallelProcessor();

    void setWetDryMix(float wetAmount); // 0.0 = 100% Dry, 1.0 = 100% Wet
    float getWetDryMix() const { return wetAmount; }

    void processParallel(juce::AudioBuffer<float>& dryBuffer,
                        const juce::AudioBuffer<float>& wetBuffer);

    void prepare(double sampleRate, int blockSize);

private:
    float wetAmount = 0.5f;  // 50% Wet/Dry Mix Standard
    double sampleRate = 44100.0;
};

/**
 * Stereo-Linking: Synchronisiert EQ-Parameter zwischen L/R für konsistente Breite
 */
class StereoLinker
{
public:
    enum class LinkMode
    {
        Unlinked,   // Keine Synchronisation
        Linked,     // L und R zusammen
        Symmetrical // L und R gespiegelt
    };

    void setLinkMode(LinkMode mode);
    LinkMode getLinkMode() const { return linkMode; }

    void synchronizeParameters(float& leftFreq, float& rightFreq,
                               float& leftGain, float& rightGain);

private:
    LinkMode linkMode = LinkMode::Unlinked;
};
