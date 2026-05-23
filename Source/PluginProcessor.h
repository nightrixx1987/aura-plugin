#pragma once

#include <JuceHeader.h>
#include "DSP/EQProcessor.h"
#include "DSP/FFTAnalyzer.h"
#include "DSP/SmartAnalyzer.h"
#include "DSP/ABComparison.h"
#include "DSP/AutoGainCompensation.h"
#include "DSP/LiveSmartEQ.h"
#include "DSP/ReferenceAudioPlayer.h"
#include "DSP/SpectralMatcher.h"
#include "DSP/HighQualityOversampler.h"
#include "DSP/DynamicResonanceSuppressor.h"
#include "DSP/LinearPhaseEQ.h"
#include "DSP/MixedPhaseEQ.h"
#include "DSP/CharacterProcessor.h"
#include "DSP/SpectralDynamicsProcessor.h"
#include "DSP/CrossChannelManager.h"
#include "DSP/TransientDetector.h"
#include "DSP/LUFSAutoGain.h"
#include "DSP/ParallelProcessor.h"
#include "DSP/GenreMorphSlider.h"
#include "DSP/CustomProfileManager.h"
#include "DSP/MultiReferenceManager.h"
#include "DSP/AIExplanationEngine.h"
#include "DSP/TemporalPatternDetector.h"
#include "DSP/WebSearchEngine.h"
#include "DSP/SidechainDynamicEQ.h"
#include "GUI/EQSketchTool.h"
#include "Utils/WASAPILoopbackCapture.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"
#include "Licensing/LicenseManager.h"
#include <limits>

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
    int  getBandConflictCount() const { return bandConflictCount.load(); }
    
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
    
    // NEU: Mixed Phase EQ Zugriff
    MixedPhaseEQ& getMixedPhaseEQ() { return mixedPhaseEQ; }
    
    // NEU: Per-Band Solo Status
    bool isAnyBandSoloed() const { return anyBandSoloed.load(); }
    
    // NEU: Auto-Listen (isoliert die Frequenz des gezogenen Bandes)
    void setAutoListen(bool enabled, float frequency = 1000.0f, float q = 4.0f)
    {
        autoListenFreq.store(frequency);
        autoListenQ.store(q);
        autoListenEnabled.store(enabled);
    }
    bool isAutoListenEnabled() const { return autoListenEnabled.load(); }
    
    // ===== v2.0: Neue Module =====
    SpectralDynamicsProcessor& getSpectralDynamics() { return spectralDynamics; }
    CrossChannelManager& getCrossChannel() { return crossChannelManager; }
    TransientDetector& getTransientDetector() { return transientDetector; }
    LUFSAutoGain& getLUFSAutoGain() { return lufsAutoGain; }
    ParallelProcessor& getParallelProcessor() { return parallelProcessor; }
    GenreMorphSlider& getGenreMorphSlider() { return genreMorphSlider; }
    CustomProfileManager& getCustomProfileManager() { return customProfileManager; }
    MultiReferenceManager& getMultiReferenceManager() { return multiReferenceManager; }
    AIExplanationEngine& getAIExplanationEngine() { return aiExplanationEngine; }
    TemporalPatternDetector& getTemporalPatternDetector() { return temporalPatternDetector; }
    EQSketchTool& getEQSketchTool() { return eqSketchTool; }
    SidechainDynamicEQ& getSidechainDynamicEQ() { return sidechainDynamicEQ; }
    WebSearchEngine& getWebSearchEngine() { return webSearchEngine; }

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
    
    // NEU: Mixed Phase EQ (IIR-Low + LinPhase-High)
    MixedPhaseEQ mixedPhaseEQ;
    
    // NEU: Character Processor (Analog Saturation)
    CharacterProcessor characterProcessor;
    
    // ===== v2.0: Neue DSP Module =====
    SpectralDynamicsProcessor spectralDynamics;
    CrossChannelManager crossChannelManager;
    TransientDetector transientDetector;
    LUFSAutoGain lufsAutoGain;
    ParallelProcessor parallelProcessor;
    GenreMorphSlider genreMorphSlider;
    CustomProfileManager customProfileManager;
    MultiReferenceManager multiReferenceManager;
    AIExplanationEngine aiExplanationEngine;
    TemporalPatternDetector temporalPatternDetector;
    EQSketchTool eqSketchTool;
    SidechainDynamicEQ sidechainDynamicEQ;
    WebSearchEngine webSearchEngine;
    
    // v2.0: O(1) parameterChanged Lookup (statt O(n) Loop über 12 Bänder × 13 IDs)
    std::unordered_map<juce::String, int> paramIdToBandIndex_;
    
    // v2.0: Pre-allozierter Buffer für Spectral Dynamics (vermeidet Heap-Allokation im Audio-Thread)
    std::vector<float> spectralDynMagnitudeBuffer_;
    
    // v2.0: Dirty-Flag für EQ-Parameter (steuert updateMagnitudeResponse in Linear/Mixed Phase)
    std::atomic<bool> eqParamsDirty_ { true };    
    // NEU: CPU Eco-Mode Frame Skipping (reduziert Last bei aufwendigen Analyzern)
    int spectralDynamicsFrameCounter_ = 0;
    int resonanceSuppressorFrameCounter_ = 0;
    static constexpr int kSpectralDynamicsSkipRate = 2;  // Nur jeden 2. Block
    static constexpr int kResonanceSuppressorSkipRate = 2;  // Nur jeden 2. Block    
    // NEU: Dry-Buffer für Wet/Dry-Mix (pre-alloziert in prepareToPlay, NIEMALS im Audio-Thread resizen!)
    juce::AudioBuffer<float> dryBuffer;
    int maxExpectedBlockSize = 0;  // Für sichere Dry-Buffer-Nutzung
    
    // NEU: Pre-allozierter Temp-Buffer für M/S Encoding/Decoding
    // Verhindert HeapBlock-Allocation (malloc) im Audio-Thread!
    juce::AudioBuffer<float> midSideTempBuffer;
    
    // NEU: Gecachte Parameter-Pointer (vermeidet HashMap-Lookup pro Block)
    std::atomic<float>* cachedInputGainParam = nullptr;
    std::atomic<float>* cachedAnalyzerOnParam = nullptr;
    std::atomic<float>* cachedWetDryParam = nullptr;
    std::atomic<float>* cachedMidSideParam = nullptr;
    std::atomic<float>* cachedPhaseModeParam = nullptr;
    std::atomic<float>* cachedLinearPhaseParam = nullptr;
    std::atomic<float>* cachedCharacterParam = nullptr;
    std::atomic<float>* cachedSuppressorEnabledParam = nullptr;
    std::atomic<float>* cachedSuppressorDepthParam = nullptr;
    std::atomic<float>* cachedSuppressorSpeedParam = nullptr;
    std::atomic<float>* cachedSuppressorSelectivityParam = nullptr;
    std::atomic<float>* cachedSmartModeParam = nullptr;
    std::atomic<float>* cachedLiveEqEnabledParam = nullptr;
    std::atomic<float>* cachedLiveEqDepthParam = nullptr;
    std::atomic<float>* cachedLiveEqAttackParam = nullptr;
    std::atomic<float>* cachedLiveEqReleaseParam = nullptr;
    std::atomic<float>* cachedLiveEqThresholdParam = nullptr;
    std::atomic<float>* cachedLiveEqModeParam = nullptr;
    std::atomic<float>* cachedLiveEqMaxReductionParam = nullptr;
    std::atomic<float>* cachedLiveEqTransientParam = nullptr;
    std::atomic<float>* cachedLiveEqMsModeParam = nullptr;
    std::atomic<float>* cachedLiveEqProfileParam = nullptr;
    std::atomic<float>* cachedDeltaModeParam = nullptr;
    std::atomic<float>* cachedCrossoverParam = nullptr;

    struct BandParamPointerCache
    {
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* channel = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* dynEnabled = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
        std::atomic<float>* dynRatio = nullptr;
        std::atomic<float>* dynAttack = nullptr;
        std::atomic<float>* dynRelease = nullptr;
        std::atomic<float>* active = nullptr;
        std::atomic<float>* solo = nullptr;
    };

    std::array<BandParamPointerCache, ParameterIDs::MAX_BANDS> cachedBandParamPointers_{};
    std::array<std::atomic<float>*, ParameterIDs::MAX_BANDS> cachedBandSoloParams{};
    juce::AudioParameterChoice* cachedLiveEqProfileChoiceParam_ = nullptr;

    // Hotpath-Caches: vermeidet redundante Setter/Umrechnungen pro Block
    float lastInputGainDB_ = std::numeric_limits<float>::quiet_NaN();
    float cachedInputGainLinear_ = 1.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSmoother_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wetDryMixSmoother_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> autoListenMixSmoother_;

    bool hasSuppressorParamCache_ = false;
    bool hasSuppressorEnabledState_ = false;
    bool lastSuppressorEnabledState_ = false;
    float lastSuppressorDepth_ = std::numeric_limits<float>::quiet_NaN();
    float lastSuppressorSpeed_ = std::numeric_limits<float>::quiet_NaN();
    float lastSuppressorSelectivity_ = std::numeric_limits<float>::quiet_NaN();

    bool hasSmartAnalyzerEnabledState_ = false;
    bool lastSmartAnalyzerEnabledState_ = false;

    bool hasSidechainAvailabilityState_ = false;
    bool lastSidechainAvailable_ = false;

    bool hasBandSoloState_ = false;
    bool lastAnyBandSoloedState_ = false;

    bool hasCharacterModeCache_ = false;
    int lastCharacterMode_ = -1;

    bool hasResolvedPhaseModeState_ = false;
    int lastResolvedPhaseMode_ = -1;
    bool hasMidSideState_ = false;
    bool lastMidSideState_ = false;

    bool hasLiveSmartEqParamCache_ = false;
    bool lastLiveSmartEqShouldBeActive_ = false;
    float lastLiveEqDepth_ = std::numeric_limits<float>::quiet_NaN();
    float lastLiveEqAttackMs_ = std::numeric_limits<float>::quiet_NaN();
    float lastLiveEqReleaseMs_ = std::numeric_limits<float>::quiet_NaN();
    float lastLiveEqThreshold_ = std::numeric_limits<float>::quiet_NaN();
    int lastLiveEqMode_ = -1;
    float lastLiveEqMaxReduction_ = std::numeric_limits<float>::quiet_NaN();
    bool lastLiveEqTransientProtect_ = false;
    int lastLiveEqMsMode_ = -1;
    int lastLiveEqProfileIndex_ = -1;
    
    // Latenz-Cache (vermeidet setLatencySamples bei jedem Block)
    int cachedLatencySamples = 0;
    
    // NEU: Deferred Oversampling-Wechsel (Race-Condition-Fix)
    // Oversampling-Faktor wird im GUI-Thread gesetzt, aber erst im Audio-Thread angewendet
    std::atomic<int> pendingOversamplingFactor { -1 };  // -1 = kein Wechsel pending
    
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

    // C.2: Anzahl Inter-Band-Konflikte (entgegengesetzte Gains innerhalb 1 Oktave)
    std::atomic<int> bandConflictCount{0};
    
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
    
    // processBlock Sub-Methoden (extrahiert für Wartbarkeit)
    bool applyDeferredOversamplingSwitch();
    void applyInputGain(juce::AudioBuffer<float>& buffer);
    void captureSystemAudio(juce::AudioBuffer<float>& buffer);
    void encodeMidSide(juce::AudioBuffer<float>& buffer);
    void decodeMidSide(juce::AudioBuffer<float>& buffer);
    int getEffectivePhaseMode() const noexcept;
    int getLatencyForPhaseMode(int phaseMode) const noexcept;
    void updateReportedLatencyForPhaseMode(int phaseMode);
    void processLinearPhaseEQ(juce::AudioBuffer<float>& buffer);
    void processMixedPhaseEQ(juce::AudioBuffer<float>& buffer);
    void processIIR_EQ(juce::AudioBuffer<float>& buffer);
    void applyWetDryMix(juce::AudioBuffer<float>& buffer, float wetDryMix);
    void processResonanceSuppressor(juce::AudioBuffer<float>& buffer);
    void processSmartAnalyzerAndLiveEQ(juce::AudioBuffer<float>& buffer);
    void detectBandConflicts();
    void applyLicenseEnforcement(juce::AudioBuffer<float>& buffer);
    void measureOutputLevels(const juce::AudioBuffer<float>& buffer);
    
    // Shared State zwischen processBlock-Sub-Methoden (pro Block gültig)
    bool globalMidSide_ = false;
    bool shouldProcess_ = true;
    bool needsDryBlend_ = false;
    float wetDryMix_ = 1.0f;
    std::array<float, 2> lastSystemCaptureSamples_ { 0.0f, 0.0f };
    
    // Auto-Listen State (atomics für GUI↔Audio Thread-Safety)
    std::atomic<bool> autoListenEnabled { false };
    std::atomic<float> autoListenFreq { 1000.0f };
    std::atomic<float> autoListenQ { 4.0f };
    // Auto-Listen Bandpass-Filter (TDF-II Biquad, 2 Kanäle)
    double alZ1[2] = {}, alZ2[2] = {};
    double alB0 = 0, alB1 = 0, alB2 = 0, alA1 = 0, alA2 = 0;
    // v2.1 FIX: Dirty-Flag für Auto-Listen Koeffizienten
    float prevAutoListenFreq_ = -1.0f;
    float prevAutoListenQ_ = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessor)
};
