#pragma once

#include <JuceHeader.h>
#include "DSP/EQProcessor.h"
#include "DSP/FFTAnalyzer.h"
#include "DSP/AdvancedProcessing.h"
#include "DSP/SmartAnalyzer.h"
#include "DSP/ABComparison.h"
#include "DSP/AutoGainCompensation.h"
#include "DSP/LiveSmartEQ.h"
#include "DSP/ReferenceAudioPlayer.h"
#include "DSP/SpectralMatcher.h"
#include "DSP/HighQualityOversampler.h"
#include "DSP/DynamicResonanceSuppressor.h"
#include "DSP/LinearPhaseEQ.h"
#include "Utils/WASAPILoopbackCapture.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"
#include "Licensing/LicenseManager.h"

/**
 * AuraAudioProcessor: Hauptklasse für die Audio-Verarbeitung.
 */
class AuraAudioProcessor : public juce::AudioProcessor,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    AuraAudioProcessor();
    ~AuraAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter-Listener
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Zugriff auf Komponenten
    EQProcessor& getEQProcessor() { return eqProcessor; }
    FFTAnalyzer& getPreAnalyzer() { return preAnalyzer; }
    FFTAnalyzer& getPostAnalyzer() { return postAnalyzer; }
    SmartAnalyzer& getSmartAnalyzer() { return smartAnalyzer; }
    ABComparison& getABComparison() { return abComparison; }
    AutoGainCompensation& getAutoGain() { return autoGain; }
    LiveSmartEQ& getLiveSmartEQ() { return liveSmartEQ; }
    ReferenceAudioPlayer& getReferencePlayer() { return referencePlayer; }
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    // NEU: System Audio Capture für Standalone
    SystemAudioCapture& getSystemAudioCapture() { return systemAudioCapture; }
    
    // NEU: Spectral Matching für Reference Track EQ
    SpectralMatcher& getSpectralMatcher() { return liveSmartEQ.getSpectralMatcher(); }
    
    /**
     * Lädt ein Reference-Spektrum für EQ-Matching
     * Wird automatisch geglättet und für Vergleich vorbereitet
     */
    void loadReferenceForMatching()
    {
        if (referencePlayer.isLoaded())
        {
            liveSmartEQ.loadReferenceForMatching(referencePlayer.getSpectrumMagnitudes());
        }
    }
    
    // Überladung mit explizitem Spektrum
    void loadReferenceForMatching(const std::vector<float>& spectrum)
    {
        liveSmartEQ.loadReferenceForMatching(spectrum);
    }
    
    /**
     * Wendet die Match-Kurve auf alle verfügbaren EQ-Bänder an
     * One-Click EQ Matching wie FabFilter Pro-Q4
     */
    void applyEQMatch(bool enabled = true)
    {
        matchingEnabled.store(enabled);
        if (enabled)
        {
            liveSmartEQ.applyMatchToEQ(apvts, 0);  // Ab Band 1 anwenden
        }
    }
    
    bool isMatchingEnabled() const { return matchingEnabled.load(); }
    
    // Lizenz-Zugriff
    LicenseManager& getLicenseManager() { return LicenseManager::getInstance(); }
    
    // Pegel-Daten für Level Meter (Stereo)
    float getOutputLevelLeft() const { return lastOutputLevelLeft.load(); }
    float getOutputLevelRight() const { return lastOutputLevelRight.load(); }
    
    // Reset alle EQ-Bänder auf Standardwerte
    void resetAllBands();
    
    // NEU: Undo/Redo System
    juce::UndoManager& getUndoManager() { return undoManager; }
    
    // NEU: Smooth Preset-Wechsel starten (kurzer Output-Crossfade)
    void beginPresetCrossfade();
    
    // NEU: Resonance Suppressor Zugriff
    DynamicResonanceSuppressor& getResonanceSuppressor() { return resonanceSuppressor; }
    
    // NEU: Linear Phase EQ Zugriff
    LinearPhaseEQ& getLinearPhaseEQ() { return linearPhaseEQ; }
    
    // NEU: Per-Band Solo Status
    bool isAnyBandSoloed() const { return anyBandSoloed.load(); }

private:
    // NEU: Undo/Redo Manager (muss vor apvts deklariert werden!)
    juce::UndoManager undoManager { 30000, 50 };  // 30KB max, 50 Transaktionen
    
    // Parameter Value Tree State
    juce::AudioProcessorValueTreeState apvts;

    // DSP
    EQProcessor eqProcessor;
    FFTAnalyzer preAnalyzer;
    FFTAnalyzer postAnalyzer;
    SmartAnalyzer smartAnalyzer;
    ABComparison abComparison;
    AutoGainCompensation autoGain;
    LiveSmartEQ liveSmartEQ;
    ReferenceAudioPlayer referencePlayer;
    
    // NEU: System Audio Capture (WASAPI Loopback für Standalone)
    SystemAudioCapture systemAudioCapture;
    
    // NEU: Oversampler (HQ-Filter bei hohen Frequenzen)
    HighQualityOversampler oversampler;
    double baseSampleRate = 44100.0;
    int baseBlockSize = 512;
    
    // NEU: Resonance Suppressor (Soothe-Style)
    DynamicResonanceSuppressor resonanceSuppressor;
    
    // NEU: Linear Phase EQ (FFT-basiert für Mastering)
    LinearPhaseEQ linearPhaseEQ;
    
    // NEU: Dry-Buffer für Wet/Dry-Mix
    juce::AudioBuffer<float> dryBuffer;
    
    // NEU: Smooth Preset-Wechsel (Crossfade)
    juce::AudioBuffer<float> presetFadeBuffer;
    std::atomic<int> presetFadeSamplesRemaining { 0 };
    int presetFadeTotalSamples = 0;  // Anzahl Samples für den Crossfade
    
    // NEU: Per-Band Solo Tracking (atomic für Audio→GUI Thread-Safety)
    std::atomic<bool> anyBandSoloed { false };
    
    // Level-Messung (Stereo) - atomic für Audio→GUI Thread-Safety
    std::atomic<float> lastOutputLevelLeft { -60.0f };
    std::atomic<float> lastOutputLevelRight { -60.0f };
    
    // Tracking für Live Smart EQ (atomic für Safety bei prepareToPlay)
    std::atomic<bool> liveSmartEqWasActive { false };
    
    // NEU: EQ Matching aktiviert (atomic für GUI↔Audio Thread-Safety)
    std::atomic<bool> matchingEnabled { false };
    
    // Lizenz-Enforcement (Audio-Degradierung bei abgelaufener Trial)
    // Checkpoint 1: Offensichtliche Noise-Injection (ablenkend)
    int noiseInjectionCounter = 0;
    int noiseInjectionInterval = 0;
    int noiseInjectionBurstLength = 0;
    int noiseInjectionBurstRemaining = 0;
    juce::Random noiseRandom;
    
    // Checkpoint 2: Subtile Bit-Quantisierung (schwer zu entdecken)
    // Sieht aus wie normaler Dither-Code
    float outputDitherDepth = 0.0f;  // 0.0 = kein Dither, >0 = Quantisierung
    int enforcementCheckCounter = 0;
    
    // Checkpoint 3: Langsamer Tremolo-LFO (getarnt als Gain-Compensation)
    // 0.1 Hz Modulation ~2dB — klingt wie pumpendes Audio
    float compensationPhase = 0.0f;
    float compensationRate = 0.0f;   // Phase-Increment pro Sample

    // Hilfsfunktionen
    void updateBandFromParameters(int bandIndex);
    void updateAllBandsFromParameters();
    void updateLiveSmartEQFromParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessor)
};
