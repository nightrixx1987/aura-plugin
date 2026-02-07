#include "PluginProcessor.h"
#include "PluginEditor.h"

AuraAudioProcessor::AuraAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "Parameters", ParameterLayout::createParameterLayout())
{
    // Parameter-Listener für alle Band-Parameter registrieren
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        apvts.addParameterListener(ParameterIDs::getBandFreqID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandGainID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandQID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandTypeID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandBypassID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandChannelID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandSlopeID(i), this);
        // Dynamic EQ Parameter
        apvts.addParameterListener(ParameterIDs::getBandDynEnabledID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandDynThresholdID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandDynRatioID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandDynAttackID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandDynReleaseID(i), this);
        apvts.addParameterListener(ParameterIDs::getBandSoloID(i), this);
    }
    
    apvts.addParameterListener(ParameterIDs::OUTPUT_GAIN, this);
    apvts.addParameterListener(ParameterIDs::WET_DRY_MIX, this);
    apvts.addParameterListener(ParameterIDs::OVERSAMPLING_FACTOR, this);
    apvts.addParameterListener(ParameterIDs::DELTA_MODE, this);
    apvts.addParameterListener(ParameterIDs::SUPPRESSOR_ENABLED, this);
    apvts.addParameterListener(ParameterIDs::SUPPRESSOR_DEPTH, this);
    apvts.addParameterListener(ParameterIDs::SUPPRESSOR_SPEED, this);
    apvts.addParameterListener(ParameterIDs::SUPPRESSOR_SELECTIVITY, this);
}

AuraAudioProcessor::~AuraAudioProcessor()
{
    // Parameter-Listener entfernen
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        apvts.removeParameterListener(ParameterIDs::getBandFreqID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandGainID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandQID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandTypeID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandBypassID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandChannelID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandSlopeID(i), this);  // FIX: Slope-Parameter
        // Dynamic EQ Parameter
        apvts.removeParameterListener(ParameterIDs::getBandDynEnabledID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandDynThresholdID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandDynRatioID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandDynAttackID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandDynReleaseID(i), this);
        apvts.removeParameterListener(ParameterIDs::getBandSoloID(i), this);
    }
    
    apvts.removeParameterListener(ParameterIDs::OUTPUT_GAIN, this);
    apvts.removeParameterListener(ParameterIDs::WET_DRY_MIX, this);
    apvts.removeParameterListener(ParameterIDs::OVERSAMPLING_FACTOR, this);
    apvts.removeParameterListener(ParameterIDs::DELTA_MODE, this);
    apvts.removeParameterListener(ParameterIDs::SUPPRESSOR_ENABLED, this);
    apvts.removeParameterListener(ParameterIDs::SUPPRESSOR_DEPTH, this);
    apvts.removeParameterListener(ParameterIDs::SUPPRESSOR_SPEED, this);
    apvts.removeParameterListener(ParameterIDs::SUPPRESSOR_SELECTIVITY, this);
}

const juce::String AuraAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AuraAudioProcessor::acceptsMidi() const
{
    return false;
}

bool AuraAudioProcessor::producesMidi() const
{
    return false;
}

bool AuraAudioProcessor::isMidiEffect() const
{
    return false;
}

double AuraAudioProcessor::getTailLengthSeconds() const
{
    // Tail-Länge berücksichtigt:
    // 1. Linear Phase EQ FFT-Latenz (wenn aktiviert)
    // 2. Oversampler FIR-Filter-Latenz
    // 3. Zusätzliche Nachklingzeit der IIR-Filter (~50ms konservativ)
    
    double tailSeconds = 0.0;
    
    if (baseSampleRate > 0.0)
    {
        // Linear Phase EQ Latenz
        auto* linearPhaseParam = apvts.getRawParameterValue(ParameterIDs::LINEAR_PHASE_MODE);
        if (linearPhaseParam != nullptr && linearPhaseParam->load() > 0.5f)
        {
            tailSeconds += static_cast<double>(linearPhaseEQ.getLatencyInSamples()) / baseSampleRate;
        }
        else
        {
            // IIR-Filter Nachklingzeit (~50ms für typische EQ-Filter)
            tailSeconds += 0.05;
        }
        
        // Oversampler Latenz
        tailSeconds += static_cast<double>(oversampler.getLatencyInSamples()) / baseSampleRate;
    }
    
    return tailSeconds;
}

int AuraAudioProcessor::getNumPrograms()
{
    return 1;
}

int AuraAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AuraAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String AuraAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void AuraAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void AuraAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Basis-Samplerate speichern (fuer Oversampling-Berechnung)
    baseSampleRate = sampleRate;
    baseBlockSize = samplesPerBlock;
    
    // EQ-Processor mit oversampled Rate vorbereiten
    double osSampleRate = sampleRate * static_cast<double>(oversampler.getFactorAsInt());
    int osBlockSize = samplesPerBlock * oversampler.getFactorAsInt();
    eqProcessor.prepare(osSampleRate, osBlockSize);
    
    // FFT-Analyzer vorbereiten (immer bei Basis-Rate)
    preAnalyzer.prepare(sampleRate);
    postAnalyzer.prepare(sampleRate);
    
    // Smart-Analyzer vorbereiten
    smartAnalyzer.prepare(sampleRate);
    
    // Live SmartEQ vorbereiten
    liveSmartEQ.prepare(sampleRate, samplesPerBlock);
    liveSmartEqWasActive.store(false);  // Reset tracking
    
    // Reset anfordern - wird im Message-Thread vom Editor ausgeführt
    liveSmartEQ.requestReset();
    
    // A/B-Vergleich initialisieren
    abComparison.prepare(sampleRate, samplesPerBlock);
    
    // Auto-Gain initialisieren
    autoGain.prepare(sampleRate, samplesPerBlock);
    
    // Reference-Player vorbereiten
    referencePlayer.prepare(sampleRate, samplesPerBlock);
    
    // NEU: Oversampler vorbereiten
    oversampler.prepare(sampleRate, samplesPerBlock, 2);
    
    // NEU: Resonance Suppressor vorbereiten
    resonanceSuppressor.prepare(sampleRate, samplesPerBlock);
    
    // NEU: Linear Phase EQ vorbereiten
    linearPhaseEQ.prepare(sampleRate, samplesPerBlock, 2);
    
    // NEU: Dry-Buffer für Wet/Dry Mix allokieren
    dryBuffer.setSize(2, samplesPerBlock);
    dryBuffer.clear();
    
    // NEU: Preset-Crossfade Buffer allokieren (~20ms)
    presetFadeTotalSamples = static_cast<int>(sampleRate * 0.02);  // 20ms
    presetFadeBuffer.setSize(2, samplesPerBlock);
    presetFadeBuffer.clear();
    presetFadeSamplesRemaining.store(0);
    
    // Lizenz-Enforcement initialisieren (3 unabhaengige Checkpoints)
    // CP1: Noise-Burst alle ~45 Sekunden
    noiseInjectionInterval = static_cast<int>(sampleRate * 45.0);
    noiseInjectionBurstLength = static_cast<int>(sampleRate * 0.3);
    noiseInjectionCounter = noiseInjectionInterval;
    noiseInjectionBurstRemaining = 0;
    // CP2: Subtile Quantisierung (wird in processBlock aktiviert)
    outputDitherDepth = 0.0f;
    enforcementCheckCounter = 0;
    // CP3: Langsamer Tremolo-LFO bei 0.08 Hz
    compensationPhase = 0.0f;
    compensationRate = static_cast<float>(0.08 * juce::MathConstants<double>::twoPi / sampleRate);
    
    // Alle Bänder mit aktuellen Parametern initialisieren
    updateAllBandsFromParameters();
}

void AuraAudioProcessor::releaseResources()
{
    eqProcessor.reset();
    preAnalyzer.reset();
    postAnalyzer.reset();
    autoGain.reset();
    liveSmartEQ.reset();
}

bool AuraAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Mono oder Stereo erlauben
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input und Output müssen gleich sein
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void AuraAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Unbenutzte Output-Kanäle löschen
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Input Gain anwenden
    auto* inputGainParam = apvts.getRawParameterValue(ParameterIDs::INPUT_GAIN);
    if (inputGainParam != nullptr)
    {
        float inputGainDB = inputGainParam->load();
        if (std::abs(inputGainDB) > 0.01f)
        {
            float inputGainLinear = juce::Decibels::decibelsToGain(inputGainDB);
            buffer.applyGain(inputGainLinear);
        }
    }
    
    // NEU: System Audio Capture (nur wenn aktiviert - für Standalone)
    // Ersetzt den normalen Input mit dem System-Audio-Output
    if (systemAudioCapture.isCapturing())
    {
        int numSamples = systemAudioCapture.getLatestSamples(buffer);
        (void)numSamples;  // Für Debug-Zwecke
    }

    // Pre-EQ Analyse (für Spektrum-Anzeige)
    auto* analyzerOnParam = apvts.getRawParameterValue(ParameterIDs::ANALYZER_ON);
    bool analyzerOn = analyzerOnParam != nullptr && analyzerOnParam->load() > 0.5f;
    
    if (analyzerOn)
    {
        preAnalyzer.pushBuffer(buffer);
    }
    
    // A/B-Vergleich: Original-Signal speichern für Delta-Listen
    abComparison.captureOriginal(buffer);
    
    // Auto-Gain: Input-Level messen
    autoGain.measureInput(buffer);

    // ===== NEU: Wet/Dry Mix - Dry-Signal vor EQ kopieren =====
    auto* wetDryParam = apvts.getRawParameterValue(ParameterIDs::WET_DRY_MIX);
    float wetDryMix = (wetDryParam != nullptr) ? wetDryParam->load() / 100.0f : 1.0f;
    
    bool needsDryBlend = wetDryMix < 0.99f;
    if (needsDryBlend)
    {
        // Dry-Signal kopieren bevor EQ angewendet wird
        if (dryBuffer.getNumSamples() < buffer.getNumSamples())
            dryBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }

    // EQ-Verarbeitung (abhängig vom A/B-Modus)
    ABComparison::CompareMode mode = abComparison.getMode();
    bool shouldProcess = (mode == ABComparison::CompareMode::Normal || 
                          mode == ABComparison::CompareMode::Delta ||
                          mode == ABComparison::CompareMode::A ||
                          mode == ABComparison::CompareMode::B);
    
    // ===== Global Mid/Side Encoding =====
    auto* midSideParam = apvts.getRawParameterValue(ParameterIDs::MID_SIDE_MODE);
    bool globalMidSide = (midSideParam != nullptr && midSideParam->load() > 0.5f);
    
    if (globalMidSide && buffer.getNumChannels() >= 2 && shouldProcess)
    {
        // L/R → M/S: Mid = (L+R)/2, Side = (L-R)/2
        auto* leftData = buffer.getWritePointer(0);
        auto* rightData = buffer.getWritePointer(1);
        const int numSamples = buffer.getNumSamples();
        
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = (leftData[i] + rightData[i]) * 0.5f;
            const float side = (leftData[i] - rightData[i]) * 0.5f;
            leftData[i] = mid;
            rightData[i] = side;
        }
    }
    
    if (shouldProcess)
    {
        // ===== Linear Phase Mode Check =====
        auto* linearPhaseParam = apvts.getRawParameterValue(ParameterIDs::LINEAR_PHASE_MODE);
        bool linearPhaseEnabled = (linearPhaseParam != nullptr && linearPhaseParam->load() > 0.5f);
        
        if (linearPhaseEnabled)
        {
            // ===== Linear Phase EQ (FFT-basiert, Zero-Phase) =====
            linearPhaseEQ.setEnabled(true);
            linearPhaseEQ.processBlock(buffer);
            // Latenz melden
            setLatencySamples(linearPhaseEQ.getLatencyInSamples());
        }
        else
        {
            linearPhaseEQ.setEnabled(false);
            
            // ===== NEU: Per-Band Solo Check =====
            // FIX: Unterstützt nun mehrere gleichzeitig gesolote Bänder
            anyBandSoloed.store(false);
            std::array<bool, ParameterIDs::MAX_BANDS> soloedBands{};
            int soloCount = 0;
            for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
            {
                auto* soloParam = apvts.getRawParameterValue(ParameterIDs::getBandSoloID(i));
                if (soloParam != nullptr && soloParam->load() > 0.5f)
                {
                    soloedBands[i] = true;
                    ++soloCount;
                }
            }
            if (soloCount > 0)
                anyBandSoloed.store(true);
            
            // ===== Oversampling-Wrapper um EQ =====
            auto osFactor = oversampler.getOversamplingFactor();
            bool useOversampling = (osFactor != HighQualityOversampler::Factor::x1);
            
            if (useOversampling)
            {
                const int numSamples = buffer.getNumSamples();
                const int numCh = juce::jmin(buffer.getNumChannels(), 2);  // Max Stereo für Oversampler
                
                // Upsample alle Kanäle
                for (int ch = 0; ch < numCh; ++ch)
                    oversampler.upsample(buffer.getReadPointer(ch), numSamples, ch);
                
                // Oversampled Buffer in temporären AudioBuffer wrappen
                int osSize = oversampler.getOversampledSize();
                // Nutze float* direkt aus dem Oversampler (zero-copy)
                float* osChannels[2] = { nullptr, nullptr };
                for (int ch = 0; ch < numCh && ch < 2; ++ch)
                    osChannels[ch] = oversampler.getOversampledBuffer(ch);
                
                juce::AudioBuffer<float> osBuffer(osChannels, numCh, osSize);
                
                // EQ bei oversampled Rate verarbeiten
                if (anyBandSoloed.load())
                {
                    // Mehrere Solo-Bänder gleichzeitig verarbeiten
                    for (int b = 0; b < ParameterIDs::MAX_BANDS; ++b)
                    {
                        if (soloedBands[b])
                        {
                            auto& band = eqProcessor.getBand(b);
                            if (band.isActive() && !band.isBypassed())
                                band.processBlock(osBuffer);
                        }
                    }
                }
                else
                {
                    eqProcessor.processBlock(osBuffer);
                }
                
                // Downsample zurück in Original-Buffer
                for (int ch = 0; ch < numCh; ++ch)
                    oversampler.downsample(buffer.getWritePointer(ch), numSamples, ch);
            }
            else
            {
                // Kein Oversampling - direkter EQ
                if (anyBandSoloed.load())
                {
                    // Mehrere Solo-Bänder gleichzeitig verarbeiten
                    for (int b = 0; b < ParameterIDs::MAX_BANDS; ++b)
                    {
                        if (soloedBands[b])
                        {
                            auto& band = eqProcessor.getBand(b);
                            if (band.isActive() && !band.isBypassed())
                                band.processBlock(buffer);
                        }
                    }
                }
                else
                {
                    eqProcessor.processBlock(buffer);
                }
            }
            
            // Latenz für IIR-Modus: nur Oversampler-Latenz
            setLatencySamples(oversampler.getLatencyInSamples());
        }
    }
    
    // ===== Global Mid/Side Decoding =====
    if (globalMidSide && buffer.getNumChannels() >= 2 && shouldProcess)
    {
        // M/S → L/R: Left = Mid+Side, Right = Mid-Side
        auto* leftData = buffer.getWritePointer(0);
        auto* rightData = buffer.getWritePointer(1);
        const int numSamples = buffer.getNumSamples();
        
        for (int i = 0; i < numSamples; ++i)
        {
            const float left  = leftData[i] + rightData[i];
            const float right = leftData[i] - rightData[i];
            leftData[i] = left;
            rightData[i] = right;
        }
    }
    
    // ===== NEU: Wet/Dry Mix anwenden =====
    if (needsDryBlend)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* wet = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);
            const int numSamples = buffer.getNumSamples();
            
            // SIMD-optimiert: wet = dry * (1-mix) + wet * mix
            juce::FloatVectorOperations::multiply(wet, wetDryMix, numSamples);
            juce::FloatVectorOperations::addWithMultiply(wet, dry, 1.0f - wetDryMix, numSamples);
        }
    }

    // Post-EQ Analyse - VOR dem Suppressor ausfuehren, damit der Suppressor
    // das aktuelle Post-EQ Signal analysiert (kein Feedback-Loop!)
    postAnalyzer.pushBuffer(buffer);

    // ===== NEU: Resonance Suppressor (Soothe-Style) =====
    auto* suppressorEnabledParam = apvts.getRawParameterValue(ParameterIDs::SUPPRESSOR_ENABLED);
    bool suppressorEnabled = (suppressorEnabledParam != nullptr && suppressorEnabledParam->load() > 0.5f);
    
    if (suppressorEnabled)
    {
        // Suppressor-Einstellungen aktualisieren
        auto* depthParam = apvts.getRawParameterValue(ParameterIDs::SUPPRESSOR_DEPTH);
        auto* speedParam = apvts.getRawParameterValue(ParameterIDs::SUPPRESSOR_SPEED);
        auto* selectivityParam = apvts.getRawParameterValue(ParameterIDs::SUPPRESSOR_SELECTIVITY);
        
        if (depthParam) resonanceSuppressor.setDepth(depthParam->load());
        if (speedParam) resonanceSuppressor.setSpeed(speedParam->load());
        if (selectivityParam) resonanceSuppressor.setSelectivity(selectivityParam->load());
        
        // Spektrum-Daten vom Post-Analyzer fuer Suppressor nutzen (jetzt aktuell!)
        const auto& magnitudes = postAnalyzer.getMagnitudes();
        if (!magnitudes.empty())
        {
            // Analyse: berechnet per-Bin Gain-Reduktionen (RT-safe, kein Heap)
            resonanceSuppressor.process(magnitudes);
            
            // Per-Frequenz gewichtete Gain-Reduktion anwenden
            resonanceSuppressor.applyToBuffer(buffer, postAnalyzer.getCurrentFFTSize());
        }
    }
    
    // Reference Track EQ-Matching: Input-Spektrum wird jetzt in LiveSmartEQ.process() aktualisiert
    // (nicht mehr hier, da es sonst doppelt aktualisiert wird)
    
    // SmartAnalyzer Enabled-Status aus Parameter lesen
    auto* smartModeParam = apvts.getRawParameterValue(ParameterIDs::SMART_MODE_ENABLED);
    bool smartModeEnabled = (smartModeParam != nullptr && smartModeParam->load() > 0.5f);
    smartAnalyzer.setEnabled(smartModeEnabled);
    
    // SmartAnalyzer aktualisieren (für Live SmartEQ und Smart Mode)
    smartAnalyzer.analyze(postAnalyzer);
    
    // Live SmartEQ verarbeiten (NUR wenn Smart Mode UND Live EQ aktiviert sind)
    auto* liveEqEnabledParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_ENABLED);
    bool liveEqEnabled = (liveEqEnabledParam != nullptr && liveEqEnabledParam->load() > 0.5f);
    
    // Prüfe ob Live EQ aktiv sein soll
    bool shouldBeActive = smartModeEnabled && liveEqEnabled;
    
    if (shouldBeActive)
    {
        liveSmartEqWasActive.store(true);
        // Einstellungen aus Parametern aktualisieren
        updateLiveSmartEQFromParameters();
        
        // Live SmartEQ verarbeiten
        liveSmartEQ.process(smartAnalyzer, eqProcessor, apvts, buffer, &postAnalyzer);
    }
    else if (liveSmartEqWasActive.load())
    {
        // War aktiv, jetzt nicht mehr - Flag setzen für asynchrones Zurücksetzen
        liveSmartEqWasActive.store(false);
        liveSmartEQ.setEnabled(false);
        liveSmartEQ.requestReset();  // Asynchrones Reset anfordern
    }
    
    // Asynchrones Reset im Message-Thread ausführen (falls angefordert)
    if (liveSmartEQ.shouldReset())
    {
        // Nur ausführen wenn wir NICHT im Audio-Thread sind
        // (Dies wird vom Timer im Editor aufgerufen)
    }
    
    // Pre-EQ Analyse nur wenn visueller Analyzer an ist
    if (analyzerOn)
    {
        // Pre-analyzer ist bereits oben gefüttert
    }
    
    // Auto-Gain anwenden (wenn aktiviert)
    if (autoGain.isEnabled())
    {
        autoGain.measureOutputAndCompensate(buffer);
    }
    
    // A/B-Vergleich: Modus anwenden (Bypass, Delta)
    // NEU: Delta-Modus aus Parameter lesen
    auto* deltaModeParam = apvts.getRawParameterValue(ParameterIDs::DELTA_MODE);
    if (deltaModeParam != nullptr)
    {
        bool deltaEnabled = deltaModeParam->load() > 0.5f;
        if (deltaEnabled && abComparison.getMode() != ABComparison::CompareMode::Delta)
            abComparison.setMode(ABComparison::CompareMode::Delta);
        else if (!deltaEnabled && abComparison.getMode() == ABComparison::CompareMode::Delta)
            abComparison.setMode(ABComparison::CompareMode::Normal);
    }
    abComparison.processCompare(buffer, true);
    
    // WICHTIG: Bei System Audio Capture den Output MUTEN um Feedback zu vermeiden!
    // Der Sound kommt ja bereits aus Windows - Aura soll nur analysieren/EQen
    // aber nicht nochmal ausgeben (sonst Rückkopplung!)
    if (systemAudioCapture.isCapturing())
    {
        buffer.clear();  // Output stummschalten
    }
    
    // ===== NEU: Smooth Preset-Crossfade =====
    int fadeRemaining = presetFadeSamplesRemaining.load();
    if (fadeRemaining > 0)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        const int fadeTotal = presetFadeTotalSamples;
        
        for (int i = 0; i < numSamples; ++i)
        {
            if (fadeRemaining <= 0)
                break;
            
            // Sinusförmiger Crossfade (klingt natürlicher als linear)
            float fadeRatio = static_cast<float>(fadeRemaining) / static_cast<float>(fadeTotal);
            // fadeRatio geht von 1.0 → 0.0 (1.0 = Anfang des Fades, 0.0 = Ende)
            // Gain: 0.0 am Anfang → 1.0 am Ende (fade-in des neuen Signals)
            float gain = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * (1.0f - fadeRatio));
            
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer(ch)[i] *= gain;
            
            --fadeRemaining;
        }
        
        presetFadeSamplesRemaining.store(std::max(0, fadeRemaining));
    }
    
    // ===== Checkpoint 1: Offensichtliche Enforcement =====
    // (Ablenkung: Cracker finden und patchen dies, uebersehen CP2+CP3)
    // Cache LicenseManager Status einmal pro Block (vermeidet wiederholte Singleton-Aufrufe)
    auto& licMgr = LicenseManager::getInstance();
    const bool isLicensed = licMgr.isFullyLicensed();
    
    if (!isLicensed)
    {
        auto licStatus = licMgr.getLicenseStatus();
        
        if (licStatus == LicenseManager::LicenseStatus::TrialExpired)
        {
            const int numSamples = buffer.getNumSamples();
            const int numCh = buffer.getNumChannels();
            
            constexpr float expiredGain = 0.25f;
            buffer.applyGain(expiredGain);
            
            for (int i = 0; i < numSamples; ++i)
            {
                if (noiseInjectionBurstRemaining > 0)
                {
                    float burstProgress = 1.0f - static_cast<float>(noiseInjectionBurstRemaining) 
                                                / static_cast<float>(noiseInjectionBurstLength);
                    float envelope = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * burstProgress);
                    float noiseLevel = 0.08f * envelope;
                    
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float noise = noiseRandom.nextFloat() * 2.0f - 1.0f;
                        buffer.getWritePointer(ch)[i] += noise * noiseLevel;
                    }
                    --noiseInjectionBurstRemaining;
                }
                else
                {
                    --noiseInjectionCounter;
                    if (noiseInjectionCounter <= 0)
                    {
                        noiseInjectionBurstRemaining = noiseInjectionBurstLength;
                        noiseInjectionCounter = noiseInjectionInterval;
                    }
                }
            }
        }
    }
    
    // ===== Checkpoint 2: Subtile Bit-Quantisierung =====
    // Getarnt als Output-Dither (sieht in Disassembly harmlos aus)
    // Reduziert Aufloesung auf ~14 Bit wenn Trial abgelaufen
    {
        if (++enforcementCheckCounter >= 8192)
        {
            enforcementCheckCounter = 0;
            float ef = licMgr.getEnforcementFactor();
            outputDitherDepth = (ef < 0.5f) ? (1.0f / 8192.0f) : 0.0f;
        }
        
        if (outputDitherDepth > 0.0f)
        {
            const float quantStep = outputDitherDepth;
            const float invQuantStep = 1.0f / quantStep;  // Division einmal statt pro Sample
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    data[i] = std::round(data[i] * invQuantStep) * quantStep;
            }
        }
    }
    
    // ===== Checkpoint 3: Langsamer Amplitude-LFO =====
    // Getarnt als "Auto-Gain Compensation" — klingt wie pumpendes Audio
    // 0.08 Hz Sinus-Modulation, ~3 dB Tiefe
    {
        float ef = licMgr.getEnforcementFactor();
        if (ef < 0.5f)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float mod = 1.0f - 0.3f * (0.5f + 0.5f * std::sin(compensationPhase));
                compensationPhase += compensationRate;
                if (compensationPhase > juce::MathConstants<float>::twoPi)
                    compensationPhase -= juce::MathConstants<float>::twoPi;
                
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.getWritePointer(ch)[i] *= mod;
            }
        }
    }
    
    // Output-Level für Level Meter berechnen (getrennt für L/R)
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;
    
    if (buffer.getNumChannels() >= 1)
    {
        leftLevel = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    }
    
    if (buffer.getNumChannels() >= 2)
    {
        rightLevel = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
    }
    else
    {
        // Mono: Beide Kanäle zeigen dasselbe
        rightLevel = leftLevel;
    }
    
    lastOutputLevelLeft.store(20.0f * std::log10(std::max(leftLevel, 1e-10f)));
    lastOutputLevelRight.store(20.0f * std::log10(std::max(rightLevel, 1e-10f)));
}

bool AuraAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AuraAudioProcessor::createEditor()
{
    return new AuraAudioProcessorEditor(*this);
}

void AuraAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AuraAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    try
    {
        std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
        
        if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        {
            beginPresetCrossfade();  // Sanfter Übergang beim State-Laden
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            updateAllBandsFromParameters();
        }
    }
    catch (const std::exception& e)
    {
        juce::ignoreUnused(e);
        DBG("setStateInformation failed: " + juce::String(e.what()));
    }
}

void AuraAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Output Gain
    if (parameterID == ParameterIDs::OUTPUT_GAIN)
    {
        eqProcessor.setOutputGain(newValue);
        return;
    }
    
    // NEU: Oversampling-Faktor ändern
    if (parameterID == ParameterIDs::OVERSAMPLING_FACTOR)
    {
        int factorIdx = static_cast<int>(newValue);
        switch (factorIdx)
        {
            case 0: oversampler.setOversamplingFactor(HighQualityOversampler::Factor::x1); break;
            case 1: oversampler.setOversamplingFactor(HighQualityOversampler::Factor::x2); break;
            case 2: oversampler.setOversamplingFactor(HighQualityOversampler::Factor::x4); break;
            default: break;
        }
        setLatencySamples(oversampler.getLatencyInSamples());
        
        // EQ-Processor mit neuer oversampled Rate re-preparen
        double osSampleRate = baseSampleRate * static_cast<double>(oversampler.getFactorAsInt());
        int osBlockSize = baseBlockSize * oversampler.getFactorAsInt();
        eqProcessor.prepare(osSampleRate, osBlockSize);
        return;
    }
    
    // NEU: Wet/Dry, Delta, Suppressor Parameter werden direkt in processBlock gelesen
    if (parameterID == ParameterIDs::WET_DRY_MIX ||
        parameterID == ParameterIDs::DELTA_MODE ||
        parameterID == ParameterIDs::SUPPRESSOR_ENABLED ||
        parameterID == ParameterIDs::SUPPRESSOR_DEPTH ||
        parameterID == ParameterIDs::SUPPRESSOR_SPEED ||
        parameterID == ParameterIDs::SUPPRESSOR_SELECTIVITY)
    {
        return;  // Direkt in processBlock gelesen
    }

    // Band-Parameter identifizieren
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        if (parameterID == ParameterIDs::getBandFreqID(i) ||
            parameterID == ParameterIDs::getBandGainID(i) ||
            parameterID == ParameterIDs::getBandQID(i) ||
            parameterID == ParameterIDs::getBandTypeID(i) ||
            parameterID == ParameterIDs::getBandBypassID(i) ||
            parameterID == ParameterIDs::getBandChannelID(i) ||
            parameterID == ParameterIDs::getBandSlopeID(i) ||
            parameterID == ParameterIDs::getBandDynEnabledID(i) ||
            parameterID == ParameterIDs::getBandDynThresholdID(i) ||
            parameterID == ParameterIDs::getBandDynRatioID(i) ||
            parameterID == ParameterIDs::getBandDynAttackID(i) ||
            parameterID == ParameterIDs::getBandDynReleaseID(i) ||
            parameterID == ParameterIDs::getBandSoloID(i))  // Dynamic EQ + Solo Parameter
        {
            updateBandFromParameters(i);
            return;
        }
    }
}

void AuraAudioProcessor::updateBandFromParameters(int bandIndex)
{
    auto& band = eqProcessor.getBand(bandIndex);
    
    // Sichere Parameter-Zugriffe
    auto* freqParam = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(bandIndex));
    auto* gainParam = apvts.getRawParameterValue(ParameterIDs::getBandGainID(bandIndex));
    auto* qParam = apvts.getRawParameterValue(ParameterIDs::getBandQID(bandIndex));
    auto* typeParam = apvts.getRawParameterValue(ParameterIDs::getBandTypeID(bandIndex));
    auto* bypassParam = apvts.getRawParameterValue(ParameterIDs::getBandBypassID(bandIndex));
    auto* channelParam = apvts.getRawParameterValue(ParameterIDs::getBandChannelID(bandIndex));
    auto* slopeParam = apvts.getRawParameterValue(ParameterIDs::getBandSlopeID(bandIndex));
    
    if (freqParam == nullptr || gainParam == nullptr || qParam == nullptr ||
        typeParam == nullptr || bypassParam == nullptr || channelParam == nullptr ||
        slopeParam == nullptr)
        return;
    
    float freq = freqParam->load();
    float gain = gainParam->load();
    float q = qParam->load();
    int type = static_cast<int>(typeParam->load());
    bool bypass = bypassParam->load() > 0.5f;
    int channel = static_cast<int>(channelParam->load());
    float slopeValue = slopeParam->load();
    
    band.setParameters(freq, gain, q,
                       static_cast<ParameterIDs::FilterType>(type),
                       static_cast<ParameterIDs::ChannelMode>(channel),
                       bypass);
    
    // Slope direkt setzen (der Wert ist bereits 6, 12, 18, 24, 36, 48, 72 oder 96)
    band.setSlope(static_cast<int>(slopeValue));
    
    // Dynamic EQ Parameter setzen
    auto* dynEnabledParam = apvts.getRawParameterValue(ParameterIDs::getBandDynEnabledID(bandIndex));
    auto* dynThresholdParam = apvts.getRawParameterValue(ParameterIDs::getBandDynThresholdID(bandIndex));
    auto* dynRatioParam = apvts.getRawParameterValue(ParameterIDs::getBandDynRatioID(bandIndex));
    auto* dynAttackParam = apvts.getRawParameterValue(ParameterIDs::getBandDynAttackID(bandIndex));
    auto* dynReleaseParam = apvts.getRawParameterValue(ParameterIDs::getBandDynReleaseID(bandIndex));
    
    if (dynEnabledParam) band.setDynamicMode(dynEnabledParam->load() > 0.5f);
    if (dynThresholdParam) band.setThreshold(dynThresholdParam->load());
    if (dynRatioParam) band.setRatio(dynRatioParam->load());
    if (dynAttackParam) band.setAttack(dynAttackParam->load());
    if (dynReleaseParam) band.setRelease(dynReleaseParam->load());
    
    // Band aktivieren: APVTS Active-Flag ODER Gain/Filtertyp beruecksichtigen
    auto* activeParam = apvts.getRawParameterValue(ParameterIDs::getBandActiveID(bandIndex));
    bool activeFlag = (activeParam != nullptr && activeParam->load() > 0.5f);
    bool hasSignificantSettings = std::abs(gain) > 0.01f || 
                                  type == static_cast<int>(ParameterIDs::FilterType::LowCut) ||
                                  type == static_cast<int>(ParameterIDs::FilterType::HighCut) ||
                                  type == static_cast<int>(ParameterIDs::FilterType::Notch);
    band.setActive(activeFlag || hasSignificantSettings);
}

void AuraAudioProcessor::updateAllBandsFromParameters()
{
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        updateBandFromParameters(i);
    }
    
    auto* outputGainParam = apvts.getRawParameterValue(ParameterIDs::OUTPUT_GAIN);
    if (outputGainParam != nullptr)
    {
        float outputGain = outputGainParam->load();
        eqProcessor.setOutputGain(outputGain);
    }
}

void AuraAudioProcessor::updateLiveSmartEQFromParameters()
{
    // WICHTIG: Bestehende Settings holen, um useReferenceAsTarget zu bewahren!
    LiveSmartEQ::Settings settings = liveSmartEQ.getSettings();
    
    // Prüfe zuerst ob Smart Mode überhaupt aktiviert ist
    auto* smartModeParam = apvts.getRawParameterValue(ParameterIDs::SMART_MODE_ENABLED);
    bool smartModeEnabled = (smartModeParam != nullptr && smartModeParam->load() > 0.5f);
    
    auto* enabledParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_ENABLED);
    auto* depthParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_DEPTH);
    auto* attackParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_ATTACK);
    auto* releaseParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_RELEASE);
    auto* thresholdParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_THRESHOLD);
    auto* modeParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_MODE);
    auto* maxReductionParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_MAX_REDUCTION);
    auto* transientParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_TRANSIENT_PROTECT);
    auto* msModeParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_MS_MODE);
    auto* profileParam = apvts.getRawParameterValue(ParameterIDs::LIVE_SMART_EQ_PROFILE);
    
    // Live EQ ist nur aktiviert wenn BEIDE Checkboxen aktiv sind
    if (enabledParam) settings.enabled = smartModeEnabled && (enabledParam->load() > 0.5f);
    if (depthParam) settings.depth = depthParam->load();
    if (attackParam) settings.attackMs = attackParam->load();
    if (releaseParam) settings.releaseMs = releaseParam->load();
    // Threshold wird NUR bei Custom-Mode vom Knob gelesen.
    // Bei Gentle/Normal/Aggressive setzt setMode() den richtigen Wert.
    if (maxReductionParam) settings.maxGainReduction = maxReductionParam->load();
    if (transientParam) settings.transientProtection = transientParam->load() > 0.5f;
    if (msModeParam) settings.midSideMode = static_cast<int>(msModeParam->load());
    
    // Profil-Name aus Index ermitteln
    if (profileParam)
    {
        // Profil-Namen direkt aus dem Parameter-Choice holen
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIDs::LIVE_SMART_EQ_PROFILE)))
        {
            settings.profileName = choiceParam->getCurrentChoiceName();
            if (settings.profileName == "Default")
                settings.profileName = "";  // Leerer Name = kein Profil
        }
    }
    
    // HINWEIS: useReferenceAsTarget wird NICHT überschrieben, weil wir die bestehenden Settings behalten!
    
    liveSmartEQ.setSettings(settings);
    
    // Modus setzen (wenn nicht Custom) — setzt automatisch den korrekten Threshold
    if (modeParam)
    {
        int modeIndex = static_cast<int>(modeParam->load());
        auto mode = static_cast<LiveSmartEQ::Mode>(modeIndex);
        if (mode != LiveSmartEQ::Mode::Custom)
        {
            liveSmartEQ.setMode(mode);
        }
        else
        {
            // Custom Mode: Threshold vom Knob lesen
            if (thresholdParam) 
            {
                auto s = liveSmartEQ.getSettings();
                s.threshold = thresholdParam->load();
                liveSmartEQ.setSettings(s);
            }
        }
    }
}

void AuraAudioProcessor::beginPresetCrossfade()
{
    // Startet einen Crossfade: Das aktuelle Audio wird kurz ausgeblendet
    // während die neuen Parameter einschwingen
    presetFadeSamplesRemaining.store(presetFadeTotalSamples);
}

void AuraAudioProcessor::resetAllBands()
{
    // Alle Bänder auf Standardwerte zurücksetzen
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        // Band deaktivieren
        if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(i)))
            param->setValueNotifyingHost(0.0f);
        
        // Gain auf 0 dB
        if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(i)))
            param->setValueNotifyingHost(param->convertTo0to1(0.0f));
        
        // Frequenz auf Standard
        if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(i)))
            param->setValueNotifyingHost(param->convertTo0to1(ParameterIDs::DEFAULT_FREQUENCIES[i]));
        
        // Q auf Standard (0.71)
        if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(i)))
            param->setValueNotifyingHost(param->convertTo0to1(ParameterIDs::DEFAULT_Q));
        
        // Filtertyp auf Bell
        if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(i)))
            param->setValueNotifyingHost(0.0f);  // Bell = 0
        
        // Bypass aus
        if (auto* param = apvts.getParameter(ParameterIDs::getBandBypassID(i)))
            param->setValueNotifyingHost(0.0f);
        
        // Kanal auf Stereo
        if (auto* param = apvts.getParameter(ParameterIDs::getBandChannelID(i)))
            param->setValueNotifyingHost(0.0f);  // Stereo = 0
    }
    
    // Output Gain auf 0 dB
    if (auto* param = apvts.getParameter(ParameterIDs::OUTPUT_GAIN))
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
    
    // Input Gain auf 0 dB
    if (auto* param = apvts.getParameter(ParameterIDs::INPUT_GAIN))
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
}

// Plugin-Instanz erstellen
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuraAudioProcessor();
}
