#pragma once

#include <JuceHeader.h>
#include "SmartAnalyzer.h"
#include "EQProcessor.h"
#include "SpectralMatcher.h"
#include "../Parameters/ParameterIDs.h"
#include <array>
#include <atomic>
// OPTIMIERUNG: std::chrono entfernt - verwende Sample-Counter statt Systemzeit

/**
 * LiveSmartEQ: Echtzeit-automatische EQ-Anpassung basierend auf SmartAnalyzer.
 * 
 * Features:
 * - Kontinuierliche Spektralanalyse mit automatischer EQ-Korrektur
 * - Smooth Parameter-Interpolation für sanfte Übergänge
 * - Attack/Release-Envelope für natürliches Verhalten
 * - Tiefe-Kontrolle (wie stark eingegriffen wird)
 * - Transient-Protection zum Schutz von Attacks
 * - Pro-Band Gain-Reduction Metering für Visualisierung
 * - RT-Safe: Parameter-Änderungen werden über AsyncUpdater an den Message-Thread delegiert
 * 
 * Inspiriert von: iZotope Neutron, Sonible smart:EQ, Oeksound Soothe2
 */

/**
 * PendingParamChange: Lock-free Queue-Eintrag für Audio→Message-Thread Parameter-Delegation.
 * Wird vom Audio-Thread in einen Ring-Buffer geschrieben und vom Message-Thread gelesen/angewendet.
 */
struct PendingParamChange
{
    int bandIndex = -1;           // EQ-Band Index (0-11)
    float gain = 0.0f;            // Gain in dB (normalisiert beim Anwenden)
    float frequency = 1000.0f;    // Frequenz in Hz
    float q = 1.0f;               // Q-Faktor
    int channelMode = 0;          // Channel-Mode (0=Stereo, 3=Mid, 4=Side)
    bool activate = false;        // Band aktivieren?
    bool deactivate = false;      // Band deaktivieren?
    bool setFreqAndQ = false;     // Frequenz/Q setzen (initial)?
    bool updateFreqAndQ = false;  // Frequenz/Q updaten (kontinuierlich)?
    bool setChannelMode = false;  // Channel-Mode setzen?
    bool valid = false;           // Gültiger Eintrag?
};

class LiveSmartEQ
{
public:
    //==========================================================================
    // Einstellungen
    //==========================================================================
    struct Settings
    {
        bool enabled = false;           // Live-Modus aktiv?
        float depth = 0.5f;             // 0-1: Wie stark eingegriffen wird
        float attackMs = 20.0f;         // Schnelle Reaktion (10-100ms)
        float releaseMs = 200.0f;       // Langsamer Release (50-1000ms)
        float threshold = 4.0f;         // dB Abweichung für Trigger
        float maxGainReduction = -12.0f; // Maximale Reduktion in dB
        float maxGainBoost = 6.0f;      // Maximaler Boost in dB (NEU!)
        bool transientProtection = true; // Schützt Transienten
        float transientSensitivity = 0.5f; // 0-1
        float updateIntervalMs = 100.0f;  // Update-Intervall (50-500ms) - einstellbar!
        
        // Frequenzbereich für Live-EQ
        float lowFreqLimit = 60.0f;     // Untere Grenze
        float highFreqLimit = 16000.0f; // Obere Grenze
        
        // Reference-Track als Ziel verwenden
        bool useReferenceAsTarget = false;
        
        // NEU: Mid/Side Smart EQ Mode
        // 0 = Stereo (default), 1 = Mid only, 2 = Side only, 3 = Auto M/S
        int midSideMode = 0;
        
        // NEU: Profil-Name für Zielkurve (leer = kein Profil)
        juce::String profileName;
    };
    
    //==========================================================================
    // Band-Status für Visualisierung
    //==========================================================================
    struct BandState
    {
        float frequency = 0.0f;         // Aktuelle Zielfrequenz
        float currentGain = 0.0f;       // Aktueller Gain (nach Smoothing)
        float targetGain = 0.0f;        // Ziel-Gain
        float gainReduction = 0.0f;     // Aktuelle Gain-Änderung (negativ=Cut, positiv=Boost)
        float q = 1.0f;                 // Q-Faktor
        bool active = false;            // Band wird verwendet?
        bool isBoost = false;           // True wenn Boost, False wenn Cut
        SmartAnalyzer::ProblemCategory category = SmartAnalyzer::ProblemCategory::None;
    };
    
    //==========================================================================
    // Preset-Modi
    //==========================================================================
    enum class Mode
    {
        Gentle,     // Sanft: Längere Attack/Release, weniger Tiefe
        Normal,     // Standard: Ausgewogen
        Aggressive, // Aggressiv: Schneller, tiefere Eingriffe
        Custom      // Benutzerdefiniert
    };
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    LiveSmartEQ() = default;
    ~LiveSmartEQ() = default;
    
    //==========================================================================
    // Initialisierung
    //==========================================================================
    void prepare(double newSampleRate, int newBlockSize)
    {
        sampleRate = newSampleRate;
        blockSize = newBlockSize;
        
        // Envelope-Koeffizienten berechnen
        updateEnvelopeCoefficients();
        
        // SpectralMatcher initialisieren
        spectralMatcher.prepare(sampleRate, 4096);  // 4096 FFT-Größe
        
        // SmoothedValues initialisieren - längere Rampen für glättere Übergänge
        for (int i = 0; i < maxBands; ++i)
        {
            gainSmoothed[i].reset(sampleRate, 0.15);  // 150ms für sanften Gain-Übergang
            freqSmoothed[i].reset(sampleRate, 0.05);  // 50ms für Frequenz
            qSmoothed[i].reset(sampleRate, 0.10);     // 100ms für Q
            
            gainSmoothed[i].setCurrentAndTargetValue(0.0f);
            freqSmoothed[i].setCurrentAndTargetValue(1000.0f);
            qSmoothed[i].setCurrentAndTargetValue(1.0f);
        }
        
        reset();
    }
    
    void reset()
    {
        for (int i = 0; i < maxBands; ++i)
        {
            bandStates[i] = BandState();
            envelopeStates[i] = 0.0f;
            bandAllocations[i] = -1;  // Keine Zuordnung
            lastAppliedGains[i] = 0.0f;
            lastBandActive[i] = false;
        }
        transientEnvelope = 0.0f;
        samplesSinceLastUpdate = 0;
    }
    
    // EQ-Bänder zurücksetzen (für Smart Mode Deaktivierung)
    // WICHTIG: Diese Funktion sollte NICHT im Audio-Thread aufgerufen werden!
    // Stattdessen wird sie jetzt über einen Flag gesteuert und im Message-Thread ausgeführt
    void resetEQBands(juce::AudioProcessorValueTreeState& apvts)
    {
        // Rate-Limiting: Nur einmal pro 100ms
        juce::int64 now = juce::Time::currentTimeMillis();
        if (now - lastResetTime < 100)
            return;
        lastResetTime = now;
        
        // Bänder 5-12 (Index 4-11) zurücksetzen
        for (int i = 0; i < maxLiveBands; ++i)
        {
            int eqBandIndex = 4 + i;  // Bänder 5-12
            if (eqBandIndex >= ParameterIDs::MAX_BANDS) continue;
            
            // Nur ändern wenn nötig
            if (lastBandActive[i] || std::abs(lastAppliedGains[i]) > 0.1f)
            {
                // Gain auf 0 - sanft über Parameter
                if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(eqBandIndex)))
                {
                    auto range = param->getNormalisableRange();
                    float currentValue = param->getValue();
                    float targetValue = range.convertTo0to1(0.0f);
                    
                    // Nur setzen wenn wirklich unterschiedlich
                    if (std::abs(currentValue - targetValue) > 0.001f)
                    {
                        param->setValueNotifyingHost(targetValue);
                    }
                }
                
                // Band deaktivieren
                if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(eqBandIndex)))
                {
                    if (param->getValue() > 0.5f)
                    {
                        param->setValueNotifyingHost(0.0f);
                    }
                }
            }
            
            lastAppliedGains[i] = 0.0f;
            lastBandActive[i] = false;
            bandStates[i].active = false;
            bandStates[i].gainReduction = 0.0f;
        }
    }
    
    // Flag für asynchrones Zurücksetzen (atomic für Thread-Safety Audio↔Message)
    std::atomic<bool> needsReset { false };
    
    void requestReset() { needsReset.store(true); }
    bool shouldReset() const { return needsReset.load(); }
    void clearResetFlag() { needsReset.store(false); }
    
    //==========================================================================
    // Einstellungen
    //==========================================================================
    void setSettings(const Settings& newSettings)
    {
        const juce::SpinLock::ScopedLockType lock(settingsLock);
        settings = newSettings;
        updateEnvelopeCoefficients();
    }
    
    Settings getSettingsCopy() const
    {
        const juce::SpinLock::ScopedLockType lock(settingsLock);
        return settings;
    }
    
    const Settings& getSettings() const { return settings; }
    
    void setEnabled(bool enabled) { settings.enabled = enabled; }
    bool isEnabled() const { return settings.enabled; }
    
    void setDepth(float depth) { settings.depth = juce::jlimit(0.0f, 1.0f, depth); }
    float getDepth() const { return settings.depth; }
    
    void setAttackMs(float ms) 
    { 
        settings.attackMs = juce::jlimit(1.0f, 100.0f, ms);
        updateEnvelopeCoefficients();
    }
    
    void setReleaseMs(float ms) 
    { 
        settings.releaseMs = juce::jlimit(50.0f, 1000.0f, ms);
        updateEnvelopeCoefficients();
    }
    
    void setThreshold(float dB) { settings.threshold = juce::jlimit(1.0f, 12.0f, dB); }
    
    // Update-Intervall (50-500ms) - beeinflusst wie oft EQ-Parameter geändert werden
    void setUpdateIntervalMs(float ms) { settings.updateIntervalMs = juce::jlimit(50.0f, 500.0f, ms); }
    float getUpdateIntervalMs() const { return settings.updateIntervalMs; }
    
    //==========================================================================
    // Preset-Modi
    //==========================================================================
    void setMode(Mode mode)
    {
        currentMode = mode;
        
        switch (mode)
        {
            case Mode::Gentle:
                settings.depth = 0.3f;
                settings.attackMs = 50.0f;
                settings.releaseMs = 500.0f;
                settings.threshold = 1.5f;
                settings.maxGainReduction = -6.0f;
                break;
                
            case Mode::Normal:
                settings.depth = 0.5f;
                settings.attackMs = 20.0f;
                settings.releaseMs = 200.0f;
                settings.threshold = 1.0f;
                settings.maxGainReduction = -12.0f;
                break;
                
            case Mode::Aggressive:
                settings.depth = 0.8f;
                settings.attackMs = 10.0f;
                settings.releaseMs = 100.0f;
                settings.threshold = 0.5f;
                settings.maxGainReduction = -18.0f;
                break;
                
            case Mode::Custom:
                // Keine Änderung - Benutzereinstellungen behalten
                break;
        }
        
        updateEnvelopeCoefficients();
    }
    
    Mode getMode() const { return currentMode; }
    
    //==========================================================================
    // Hauptverarbeitung - wird pro Block aufgerufen
    // AUTOMATISCHE EQ-ANPASSUNG: Wendet die erkannten Probleme auf die EQ-Bänder an!
    //==========================================================================
    void process(SmartAnalyzer& analyzer, EQProcessor& /*eqProcessor*/,
                 juce::AudioProcessorValueTreeState& apvts,
                 const juce::AudioBuffer<float>& buffer,
                 const FFTAnalyzer* fftAnalyzer = nullptr)
    {
        if (!settings.enabled)
        {
            // Sanftes Ausblenden und EQ-Bänder zurücksetzen (rate-limited)
            if (shouldUpdateParameters())
            {
                fadeOutAndResetEQ(apvts);
            }
            return;
        }
        
        // NEU: Profil-Synchronisation mit SmartAnalyzer
        if (settings.profileName.isNotEmpty() 
            && settings.profileName != analyzer.getCurrentProfileName())
        {
            analyzer.setInstrumentProfile(settings.profileName);
        }
        
        // Transient Detection
        bool isTransient = false;
        if (settings.transientProtection)
        {
            isTransient = detectTransient(buffer);
        }
        
        // NEU: Wenn Reference-Matching aktiv ist, Match-Punkte vom SpectralMatcher verwenden
        if (hasReferenceSpectrum && settings.useReferenceAsTarget)
        {
            // NEU: Input-Spektrum an SpectralMatcher übergeben (wichtig!)
            if (fftAnalyzer != nullptr)
            {
                spectralMatcher.updateInputSpectrum(fftAnalyzer->getMagnitudes());
            }
            
            // Match-Punkte vom SpectralMatcher holen und auf EQ-Bänder anwenden
            assignMatchPointsToBands();
        }
        else
        {
            // Standard: Probleme vom SmartAnalyzer holen (RT-safe: Array statt vector-Kopie)
            const auto& problemsArray = analyzer.getDetectedProblemsArray();
            int problemsCount = analyzer.getDetectedProblemsCount();
            
            // Probleme zu internen Bändern zuordnen (OHNE sofortige EQ-Änderung)
            assignProblemsForVisualizationFromArray(problemsArray, problemsCount);
            
            // NEU: Profil-Zielkurve auf Bänder anwenden (addiert Target-Offset)
            if (settings.profileName.isNotEmpty())
            {
                applyProfileTargetCurve(analyzer.getCurrentProfile());
            }
        }
        
        // Envelope Following und Smoothing
        for (int i = 0; i < maxBands; ++i)
        {
            auto& state = bandStates[i];
            
            if (state.active)
            {
                // Ziel-Gain berechnen (mit Depth-Skalierung)
                float targetGain = state.targetGain * settings.depth;
                
                // Prüfe ob es ein Boost oder Cut ist
                state.isBoost = targetGain > 0.0f;
                
                // Gain-Limits anwenden (unterschiedlich für Boost und Cut)
                if (state.isBoost)
                {
                    targetGain = std::min(targetGain, settings.maxGainBoost);
                }
                else
                {
                    targetGain = std::max(targetGain, settings.maxGainReduction);
                }
                
                // Transient Protection: Weniger Eingriff bei Transienten (nur für Cuts)
                if (isTransient && settings.transientProtection && !state.isBoost)
                {
                    targetGain *= (1.0f - settings.transientSensitivity * 0.7f);
                }
                
                // Envelope Following mit frequenzabhängigem Attack/Release
                float& envelope = envelopeStates[i];
                float bandAttack = perBandAttackCoeff[i];
                float bandRelease = perBandReleaseCoeff[i];
                
                if (state.isBoost)
                {
                    // Für Boosts: Attack wenn Gain steigt, Release wenn er fällt
                    if (targetGain > envelope)
                    {
                        envelope = bandAttack * envelope + (1.0f - bandAttack) * targetGain;
                    }
                    else
                    {
                        envelope = bandRelease * envelope + (1.0f - bandRelease) * targetGain;
                    }
                }
                else
                {
                    // Für Cuts: Attack wenn Gain fällt (tiefer), Release wenn er steigt (zurück zu 0)
                    if (targetGain < envelope)
                    {
                        envelope = bandAttack * envelope + (1.0f - bandAttack) * targetGain;
                    }
                    else
                    {
                        envelope = bandRelease * envelope + (1.0f - bandRelease) * targetGain;
                    }
                }
                
                // SmoothedValue updaten
                gainSmoothed[i].setTargetValue(envelope);
                
                // Aktuelle Werte für Anzeige
                state.currentGain = gainSmoothed[i].getNextValue();
                state.gainReduction = state.currentGain;
            }
            else
            {
                // Inaktive Bänder sanft auf 0 bringen
                envelopeStates[i] = releaseCoeff * envelopeStates[i];
                gainSmoothed[i].setTargetValue(0.0f);
                state.currentGain = gainSmoothed[i].getNextValue();
                state.gainReduction = state.currentGain;
            }
        }
        
        // EQ-Parameter nur rate-limited aktualisieren (alle ~50ms)
        if (shouldUpdateParameters())
        {
            // Auto-Gain Compensation: Kurven-basiert berechnen
            updateAutoGainCompensation();
            
            updateEQParameters(apvts);
        }
    }
    
    //==========================================================================
    // Getter für UI
    //==========================================================================
    const BandState& getBandState(int index) const
    {
        return bandStates[juce::jlimit(0, maxBands - 1, index)];
    }
    
    int getMaxBands() const { return maxBands; }
    
    // Auto-Gain Compensation Wert für UI-Anzeige
    float getAutoGainCompensationDb() const { return autoGainCompensationDb; }
    
    // Gesamte Gain-Reduktion für Visualisierung
    float getTotalGainReduction() const
    {
        float total = 0.0f;
        for (int i = 0; i < maxBands; ++i)
        {
            if (bandStates[i].active)
                total += bandStates[i].gainReduction;
        }
        return total;
    }
    
    // Anzahl aktiver Bänder
    int getActiveBandCount() const
    {
        int count = 0;
        for (int i = 0; i < maxBands; ++i)
        {
            if (bandStates[i].active)
                count++;
        }
        return count;
    }
    
private:
    //==========================================================================
    // Private Hilfsfunktionen
    //==========================================================================
    
    void updateEnvelopeCoefficients()
    {
        if (sampleRate <= 0) return;
        
        // Koeffizienten für Block-basierte Verarbeitung
        float blocksPerSecond = static_cast<float>(sampleRate) / static_cast<float>(blockSize);
        
        float attackBlocks = (settings.attackMs / 1000.0f) * blocksPerSecond;
        float releaseBlocks = (settings.releaseMs / 1000.0f) * blocksPerSecond;
        
        attackCoeff = std::exp(-1.0f / std::max(1.0f, attackBlocks));
        releaseCoeff = std::exp(-1.0f / std::max(1.0f, releaseBlocks));
        
        // NEU: Per-Band frequenzabhängige Koeffizienten berechnen
        for (int i = 0; i < maxBands; ++i)
        {
            float freq = bandStates[i].frequency;
            float freqFactor = getFrequencyTimeFactor(freq);
            
            float bandAttackBlocks = (settings.attackMs * freqFactor / 1000.0f) * blocksPerSecond;
            float bandReleaseBlocks = (settings.releaseMs * freqFactor / 1000.0f) * blocksPerSecond;
            
            perBandAttackCoeff[i] = std::exp(-1.0f / std::max(1.0f, bandAttackBlocks));
            perBandReleaseCoeff[i] = std::exp(-1.0f / std::max(1.0f, bandReleaseBlocks));
        }
    }
    
    /**
     * Frequenzabhängiger Zeitfaktor für Attack/Release.
     * Tiefe Frequenzen (<200Hz) → langsamer (Faktor 2x), weniger Artefakte
     * Mittlere Frequenzen (200-5kHz) → normal (1x)
     * Hohe Frequenzen (>5kHz) → schneller (0.7x), schnelle Reaktion
     */
    static float getFrequencyTimeFactor(float freq)
    {
        if (freq < 100.0f)  return 2.5f;
        if (freq < 200.0f)  return 2.0f;
        if (freq < 500.0f)  return 1.5f;
        if (freq < 5000.0f) return 1.0f;
        if (freq < 10000.0f) return 0.8f;
        return 0.6f;
    }
    
    // Rate-Limiting für Parameter-Updates (verhindert Knackser)
    // OPTIMIERUNG: Sample-Counter statt std::chrono für Realtime-Safety
    bool shouldUpdateParameters()
    {
        samplesSinceLastUpdate += blockSize;
        
        // Berechne benötigte Samples für das eingestellte Intervall
        int samplesNeeded = static_cast<int>((settings.updateIntervalMs / 1000.0f) * sampleRate);
        
        if (samplesSinceLastUpdate >= samplesNeeded)
        {
            samplesSinceLastUpdate = 0;
            return true;
        }
        return false;
    }
    
    // EQ-Parameter RT-safe in Queue schreiben (Audio-Thread → Message-Thread)
    void updateEQParameters(juce::AudioProcessorValueTreeState& /*apvts*/)
    {
        for (int i = 0; i < maxLiveBands; ++i)
        {
            const int eqBandIndex = 4 + i;  // Bänder 5-12 für Live-EQ
            if (eqBandIndex >= ParameterIDs::MAX_BANDS) continue;
            
            auto& state = bandStates[i];
            float currentGain = state.currentGain;
            float lastGain = lastAppliedGains[i];
            
            // Prüfe ob sich Gain, Frequenz oder Q signifikant geändert haben
            bool significantGainChange = std::abs(currentGain - lastGain) > 0.3f;
            bool becameActive = state.active && !lastBandActive[i];
            bool becameInactive = !state.active && lastBandActive[i];
            
            // Freq/Q Änderungen UNABHÄNGIG von Gain-Änderung prüfen
            float freqDiff = lastAppliedFreqs[i] > 0.01f 
                ? std::abs(std::log2(state.frequency / lastAppliedFreqs[i]))
                : 10.0f;  // Erste Zuweisung
            float qDiff = std::abs(state.q - lastAppliedQ[i]);
            bool significantFreqQChange = (freqDiff > 0.05f) || (qDiff > 0.2f);
            
            if (state.active && (significantGainChange || becameActive || significantFreqQChange))
            {
                PendingParamChange change;
                change.bandIndex = eqBandIndex;
                change.gain = currentGain;
                change.frequency = state.frequency;
                change.q = state.q;
                change.activate = becameActive;
                change.setFreqAndQ = becameActive;
                
                // Freq/Q kontinuierlich updaten bei jeder signifikanter Änderung
                if (!becameActive)
                {
                    change.updateFreqAndQ = significantFreqQChange;
                }
                
                change.valid = true;
                
                // Mid/Side Channel-Mode bestimmen
                if (becameActive)
                {
                    change.setChannelMode = true;
                    change.channelMode = computeChannelModeForBand(state.frequency);
                }
                
                pushPendingChange(change);
                
                lastAppliedGains[i] = currentGain;
                lastAppliedFreqs[i] = state.frequency;
                lastAppliedQ[i] = state.q;
                lastBandActive[i] = true;
            }
            else if (becameInactive || (!state.active && std::abs(lastGain) > 0.1f))
            {
                PendingParamChange change;
                change.bandIndex = eqBandIndex;
                change.gain = 0.0f;
                change.deactivate = (std::abs(lastGain) < 0.1f);
                change.valid = true;
                
                // Bei Deaktivierung Channel zurück auf Stereo setzen
                if (change.deactivate)
                {
                    change.setChannelMode = true;
                    change.channelMode = static_cast<int>(ParameterIDs::ChannelMode::Stereo);
                }
                
                pushPendingChange(change);
                
                lastAppliedGains[i] = 0.0f;
                
                if (std::abs(lastGain) < 0.1f)
                    lastBandActive[i] = false;
            }
        }
    }
    
    /**
     * Berechnet den Channel-Mode für ein Band basierend auf midSideMode-Setting.
     * 0=Stereo → alle Bänder Stereo
     * 1=Mid → alle Bänder Mid
     * 2=Side → alle Bänder Side
     * 3=Auto M/S → Bass(<300Hz)=Mid, Höhen=Side (intelligente Verteilung)
     */
    int computeChannelModeForBand(float frequency) const
    {
        switch (settings.midSideMode)
        {
            case 1: return static_cast<int>(ParameterIDs::ChannelMode::Mid);
            case 2: return static_cast<int>(ParameterIDs::ChannelMode::Side);
            case 3: // Auto M/S: Bass → Mid, Höhen → Side
                return (frequency < 300.0f)
                    ? static_cast<int>(ParameterIDs::ChannelMode::Mid)
                    : static_cast<int>(ParameterIDs::ChannelMode::Side);
            default: return static_cast<int>(ParameterIDs::ChannelMode::Stereo);
        }
    }
    
    /**
     * Wendet Profil-Zielkurve auf aktive Bänder an.
     * Addiert den frequenzabhängigen Target-Offset des Profils zum Ziel-Gain.
     * Beispiel: Vocals-Profil hat presence=+2dB → Bänder im 5-10kHz Bereich
     * bekommen +2dB zum bestehenden Target-Gain addiert.
     */
    void applyProfileTargetCurve(const InstrumentProfiles::Profile& profile)
    {
        for (int i = 0; i < maxBands; ++i)
        {
            auto& state = bandStates[i];
            if (!state.active) continue;
            
            // Frequenzbereich → Zielkurven-Offset zuordnen
            float offset = getTargetCurveOffsetForFrequency(state.frequency, profile.targetCurve);
            
            // Offset zum existierenden Target-Gain addieren (max ±6dB Profil-Einfluss)
            state.targetGain += juce::jlimit(-6.0f, 6.0f, offset);
        }
    }
    
    /**
     * Gibt den Zielkurven-Offset (in dB) für eine bestimmte Frequenz zurück.
     * Interpoliert zwischen den 7 Profil-Bändern (Sub, Bass, LowMid, Mid, HighMid, Presence, Air).
     */
    static float getTargetCurveOffsetForFrequency(float freq, 
                                                    const InstrumentProfiles::Profile::TargetCurve& curve)
    {
        // Frequenzgrenzen der Profil-Bänder (Mittenfrequenzen für Interpolation)
        // Sub: 20-60Hz (40Hz), Bass: 60-200Hz (110Hz), LowMid: 200-500Hz (315Hz)
        // Mid: 500-2000Hz (1000Hz), HighMid: 2-5kHz (3150Hz), Presence: 5-10kHz (7000Hz), Air: 10-20kHz (14000Hz)
        
        if (freq < 60.0f) return curve.sub;
        if (freq < 200.0f)
        {
            float t = (freq - 60.0f) / 140.0f;
            return curve.sub * (1.0f - t) + curve.bass * t;
        }
        if (freq < 500.0f)
        {
            float t = (freq - 200.0f) / 300.0f;
            return curve.bass * (1.0f - t) + curve.lowMid * t;
        }
        if (freq < 2000.0f)
        {
            float t = (freq - 500.0f) / 1500.0f;
            return curve.lowMid * (1.0f - t) + curve.mid * t;
        }
        if (freq < 5000.0f)
        {
            float t = (freq - 2000.0f) / 3000.0f;
            return curve.mid * (1.0f - t) + curve.highMid * t;
        }
        if (freq < 10000.0f)
        {
            float t = (freq - 5000.0f) / 5000.0f;
            return curve.highMid * (1.0f - t) + curve.presence * t;
        }
        {
            float t = juce::jlimit(0.0f, 1.0f, (freq - 10000.0f) / 10000.0f);
            return curve.presence * (1.0f - t) + curve.air * t;
        }
    }
    
    // Lock-free ring buffer für pending parameter changes (Audio→Message Thread)
    static constexpr int kPendingQueueSize = 32;
    std::array<PendingParamChange, kPendingQueueSize> pendingChanges;
    std::atomic<int> pendingWriteIndex { 0 };
    std::atomic<int> pendingReadIndex { 0 };
    
    void pushPendingChange(const PendingParamChange& change)
    {
        int writeIdx = pendingWriteIndex.load(std::memory_order_relaxed);
        int nextWriteIdx = (writeIdx + 1) % kPendingQueueSize;
        // Wenn Queue voll, ältesten Eintrag überschreiben (non-blocking)
        if (nextWriteIdx == pendingReadIndex.load(std::memory_order_acquire))
        {
            pendingReadIndex.store((nextWriteIdx + 1) % kPendingQueueSize, std::memory_order_release);
        }
        pendingChanges[static_cast<size_t>(writeIdx)] = change;
        pendingWriteIndex.store(nextWriteIdx, std::memory_order_release);
    }
    
public:
    /**
     * Muss vom Message-Thread aufgerufen werden (z.B. via Timer oder AsyncUpdater).
     * Wendet gepufferte Parameter-Änderungen sicher auf APVTS an.
     */
    void applyPendingParameterChanges(juce::AudioProcessorValueTreeState& apvts)
    {
        int readIdx = pendingReadIndex.load(std::memory_order_acquire);
        int writeIdx = pendingWriteIndex.load(std::memory_order_acquire);
        
        while (readIdx != writeIdx)
        {
            const auto& change = pendingChanges[static_cast<size_t>(readIdx)];
            if (change.valid)
            {
                if (change.activate)
                {
                    if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(change.bandIndex)))
                        param->setValueNotifyingHost(1.0f);
                    
                    if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(change.bandIndex)))
                        param->setValueNotifyingHost(0.0f);  // Bell
                    
                    // Dynamic EQ explizit deaktivieren - Smart EQ hat eigenes Envelope-Processing
                    if (auto* param = apvts.getParameter(ParameterIDs::getBandDynEnabledID(change.bandIndex)))
                        param->setValueNotifyingHost(0.0f);
                }
                
                // Mid/Side Channel-Mode setzen
                if (change.setChannelMode)
                {
                    if (auto* param = apvts.getParameter(ParameterIDs::getBandChannelID(change.bandIndex)))
                    {
                        auto range = param->getNormalisableRange();
                        param->setValueNotifyingHost(range.convertTo0to1(static_cast<float>(change.channelMode)));
                    }
                }
                
                if (change.setFreqAndQ || change.updateFreqAndQ)
                {
                    if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(change.bandIndex)))
                    {
                        auto range = param->getNormalisableRange();
                        param->setValueNotifyingHost(range.convertTo0to1(change.frequency));
                    }
                    if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(change.bandIndex)))
                    {
                        auto range = param->getNormalisableRange();
                        param->setValueNotifyingHost(range.convertTo0to1(change.q));
                    }
                }
                
                // Gain setzen
                if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(change.bandIndex)))
                {
                    auto range = param->getNormalisableRange();
                    param->setValueNotifyingHost(range.convertTo0to1(change.gain));
                }
                
                if (change.deactivate)
                {
                    if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(change.bandIndex)))
                        param->setValueNotifyingHost(0.0f);
                }
            }
            
            readIdx = (readIdx + 1) % kPendingQueueSize;
        }
        pendingReadIndex.store(readIdx, std::memory_order_release);
    }
    
private:
    
    bool detectTransient(const juce::AudioBuffer<float>& buffer)
    {
        // Einfache Transient-Erkennung basierend auf RMS-Anstieg
        float rms = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
        }
        rms /= buffer.getNumChannels();
        
        float rmsDb = juce::Decibels::gainToDecibels(rms, -100.0f);
        
        // Schnelles Attack für Transient-Detektion
        float transientAttack = 0.9f;
        float transientRelease = 0.99f;
        
        if (rmsDb > transientEnvelope)
        {
            float rise = rmsDb - transientEnvelope;
            transientEnvelope = transientAttack * transientEnvelope + (1.0f - transientAttack) * rmsDb;
            
            // Starker Anstieg = Transient
            return rise > 6.0f;  // 6dB Anstieg
        }
        else
        {
            transientEnvelope = transientRelease * transientEnvelope + (1.0f - transientRelease) * rmsDb;
            return false;
        }
    }
    
    // Probleme nur für Visualisierung zuordnen (ohne sofortige EQ-Änderung)
    void assignProblemsForVisualization(const std::vector<SmartAnalyzer::FrequencyProblem>& problems)
    {
        // Alle Bänder als inaktiv markieren
        for (int i = 0; i < maxBands; ++i)
        {
            bandStates[i].active = false;
        }
        
        if (problems.empty()) return;
        
        // Probleme nach Severity sortieren (wichtigste zuerst)
        std::vector<size_t> sortedIndices(problems.size());
        std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
        
        std::sort(sortedIndices.begin(), sortedIndices.end(),
            [&problems](size_t a, size_t b) {
                int severityA = static_cast<int>(problems[a].severity);
                int severityB = static_cast<int>(problems[b].severity);
                if (severityA != severityB)
                    return severityA > severityB;
                return problems[a].confidence > problems[b].confidence;
            });
        
        // Probleme auf interne Bänder verteilen (max 4 für Live-EQ)
        int bandIndex = 0;
        for (size_t idx : sortedIndices)
        {
            if (bandIndex >= maxLiveBands) break;
            
            const auto& problem = problems[idx];
            
            // Frequenzbereich prüfen
            if (problem.frequency < settings.lowFreqLimit || 
                problem.frequency > settings.highFreqLimit)
                continue;
            
            // Threshold prüfen
            if (std::abs(problem.deviation) < settings.threshold)
                continue;
            
            auto& state = bandStates[bandIndex];
            state.frequency = problem.frequency;
            state.targetGain = problem.suggestedGain;
            state.q = problem.suggestedQ;
            state.category = problem.category;
            state.active = true;
            state.isBoost = SmartAnalyzer::isBoostCategory(problem.category);
            
            bandAllocations[bandIndex] = static_cast<int>(idx);
            ++bandIndex;
        }
    }
    
    // RT-safe Version: Arbeitet direkt mit dem fixed-size Array vom SmartAnalyzer
    // VERBESSERT: Frequenz-Spacing, Problem-Merging, Perceptual Weighting
    void assignProblemsForVisualizationFromArray(
        const std::array<SmartAnalyzer::FrequencyProblem, SmartAnalyzer::maxDetectedProblems>& problems, 
        int count)
    {
        for (int i = 0; i < maxBands; ++i)
            bandStates[i].active = false;
        
        if (count <= 0) return;
        
        // Sortierung über Index-Array (stack-allokiert, RT-safe)
        std::array<int, SmartAnalyzer::maxDetectedProblems> sortedIndices;
        for (int i = 0; i < count; ++i) sortedIndices[static_cast<size_t>(i)] = i;
        
        // Sortiere nach Severity, dann perceptual-gewichteter Confidence
        std::sort(sortedIndices.begin(), sortedIndices.begin() + count,
            [&problems](int a, int b) {
                const auto& pa = problems[static_cast<size_t>(a)];
                const auto& pb = problems[static_cast<size_t>(b)];
                int sevA = static_cast<int>(pa.severity);
                int sevB = static_cast<int>(pb.severity);
                if (sevA != sevB) return sevA > sevB;
                
                // Perceptual Weighting: 1-5 kHz empfindlicher
                float weightA = pa.confidence * getPerceptualWeight(pa.frequency);
                float weightB = pb.confidence * getPerceptualWeight(pb.frequency);
                return weightA > weightB;
            });
        
        int bandIndex = 0;
        // Array mit zugewiesenen Frequenzen für Spacing-Check
        std::array<float, 12> assignedFreqs{};
        int numAssigned = 0;
        
        for (int si = 0; si < count; ++si)
        {
            if (bandIndex >= maxLiveBands) break;
            const auto& problem = problems[static_cast<size_t>(sortedIndices[static_cast<size_t>(si)])];
            
            if (problem.frequency < settings.lowFreqLimit || problem.frequency > settings.highFreqLimit)
                continue;
            if (std::abs(problem.deviation) < settings.threshold)
                continue;
            
            // Frequenz-Spacing prüfen: Mindestabstand 1/3 Oktave zu bereits zugewiesenen Bändern
            bool tooClose = false;
            for (int j = 0; j < numAssigned; ++j)
            {
                float octaveDistance = std::abs(std::log2(problem.frequency / assignedFreqs[static_cast<size_t>(j)]));
                if (octaveDistance < 0.33f)  // 1/3 Oktave Mindestabstand
                {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;
            
            auto& state = bandStates[bandIndex];
            state.frequency = problem.frequency;
            state.targetGain = problem.suggestedGain;
            state.q = problem.suggestedQ;
            state.category = problem.category;
            state.active = true;
            state.isBoost = SmartAnalyzer::isBoostCategory(problem.category);
            
            assignedFreqs[static_cast<size_t>(numAssigned)] = problem.frequency;
            ++numAssigned;
            bandAllocations[bandIndex] = sortedIndices[static_cast<size_t>(si)];
            ++bandIndex;
        }
    }
    
    // Perceptual Weighting: Ohrenempfindlichkeit nach Fletcher-Munson
    static float getPerceptualWeight(float frequency)
    {
        // Equal-Loudness-Kurve (vereinfacht): 1-5 kHz am empfindlichsten
        if (frequency >= 1000.0f && frequency <= 5000.0f) return 1.5f;
        if (frequency >= 500.0f && frequency <= 8000.0f) return 1.2f;
        if (frequency < 100.0f || frequency > 14000.0f) return 0.6f;
        return 1.0f;
    }
    
    /**
     * NEU: Weist Match-Punkte vom SpectralMatcher den EQ-Bändern zu
     * Wird verwendet wenn Reference-Track als Ziel aktiviert ist
     */
    void assignMatchPointsToBands()
    {
        // Alle Bänder als inaktiv markieren
        for (int i = 0; i < maxBands; ++i)
        {
            bandStates[i].active = false;
        }
        
        // Match-Punkte vom SpectralMatcher holen
        const auto& matchPoints = spectralMatcher.getMatchPoints();
        
        if (matchPoints.empty()) return;
        
        // Match-Punkte auf interne Bänder verteilen (max 4 für Live-EQ)
        int bandIndex = 0;
        for (const auto& point : matchPoints)
        {
            if (bandIndex >= maxLiveBands) break;
            
            // Frequenzbereich prüfen
            if (point.frequency < settings.lowFreqLimit || 
                point.frequency > settings.highFreqLimit)
                continue;
            
            // Gain-Threshold prüfen (kleiner als 1 dB ignorieren)
            if (std::abs(point.gainDB) < 1.0f)
                continue;
            
            auto& state = bandStates[bandIndex];
            state.frequency = point.frequency;
            state.targetGain = point.gainDB;  // Korrektur-Gain vom SpectralMatcher
            state.q = point.q;
            
            // Kategorie basierend auf Frequenzbereich und Gain-Richtung bestimmen
            if (point.gainDB > 0.0f)
            {
                // Boost-Kategorien nach Frequenz
                if (point.frequency < 120.0f)
                    state.category = SmartAnalyzer::ProblemCategory::ThinSound;
                else if (point.frequency < 500.0f)
                    state.category = SmartAnalyzer::ProblemCategory::LackOfWarmth;
                else if (point.frequency < 3000.0f)
                    state.category = SmartAnalyzer::ProblemCategory::LackOfClarity;
                else if (point.frequency < 8000.0f)
                    state.category = SmartAnalyzer::ProblemCategory::LackOfPresence;
                else
                    state.category = SmartAnalyzer::ProblemCategory::LackOfAir;
                state.isBoost = true;
            }
            else
            {
                // Cut-Kategorien nach Frequenz
                if (point.frequency < 80.0f)
                    state.category = SmartAnalyzer::ProblemCategory::Rumble;
                else if (point.frequency < 300.0f)
                    state.category = SmartAnalyzer::ProblemCategory::Mud;
                else if (point.frequency < 700.0f)
                    state.category = SmartAnalyzer::ProblemCategory::Boxiness;
                else if (point.frequency < 5000.0f)
                    state.category = SmartAnalyzer::ProblemCategory::Harshness;
                else
                    state.category = SmartAnalyzer::ProblemCategory::Sibilance;
                state.isBoost = false;
            }
            
            state.active = true;
            bandAllocations[bandIndex] = bandIndex;
            ++bandIndex;
        }
    }
    
    void assignProblemsToBands(const std::vector<SmartAnalyzer::FrequencyProblem>& problems,
                                  juce::AudioProcessorValueTreeState& apvts)
    {
        juce::ignoreUnused(apvts);
        assignProblemsForVisualization(problems);
    }
    
    // Legacy-Funktion - nicht mehr verwendet
    void assignProblemsAndApplyEQ(const std::vector<SmartAnalyzer::FrequencyProblem>& problems,
                                   juce::AudioProcessorValueTreeState& apvts)
    {
        juce::ignoreUnused(apvts);
        assignProblemsForVisualization(problems);
    }
    
    // Sanftes Ausblenden UND EQ-Bänder zurücksetzen
    void fadeOutAndResetEQ(juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < maxBands; ++i)
        {
            envelopeStates[i] = releaseCoeff * envelopeStates[i];
            gainSmoothed[i].setTargetValue(0.0f);
            bandStates[i].currentGain = gainSmoothed[i].getNextValue();
            bandStates[i].gainReduction = bandStates[i].currentGain;
            
            if (std::abs(bandStates[i].currentGain) < 0.01f)
            {
                bandStates[i].active = false;
            }
        }
        
        // EQ-Parameter zurücksetzen (rate-limited über updateEQParameters)
        updateEQParameters(apvts);
    }
    
    //==========================================================================
    // Member-Variablen
    //==========================================================================
    static constexpr int maxBands = ParameterIDs::MAX_BANDS;
    static constexpr int maxLiveBands = 8;  // 8 Bänder für Live-EQ (5-12)
    // Update-Intervall jetzt in Settings einstellbar (settings.updateIntervalMs)
    
    Settings settings;
    mutable juce::SpinLock settingsLock;
    Mode currentMode = Mode::Normal;
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    // Envelope-Koeffizienten
    float attackCoeff = 0.9f;
    float releaseCoeff = 0.99f;
    
    // Per-Band frequenzabhängige Envelope-Koeffizienten (Phase F: Spectral Smoothing)
    std::array<float, maxBands> perBandAttackCoeff = {};
    std::array<float, maxBands> perBandReleaseCoeff = {};
    
    // Pro-Band Zustand
    std::array<BandState, maxBands> bandStates;
    std::array<float, maxBands> envelopeStates = {};
    std::array<int, maxBands> bandAllocations = {};
    
    // SmoothedValues für sanfte Übergänge
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, maxBands> gainSmoothed;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, maxBands> freqSmoothed;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, maxBands> qSmoothed;
    
    // OPTIMIERUNG: Sample-Counter statt std::chrono für Rate-Limiting (RT-safe)
    int samplesSinceLastUpdate = 0;
    std::array<float, maxBands> lastAppliedGains = {};
    std::array<float, maxBands> lastAppliedFreqs = {};
    std::array<float, maxBands> lastAppliedQ = {};
    std::array<bool, maxBands> lastBandActive = {};
    
    // Transient Detection
    float transientEnvelope = -100.0f;
    
    // Auto-Gain Compensation (kurven-basiert)
    float autoGainCompensationDb = 0.0f;   // Aktuelle Kompensation in dB
    float autoGainTargetDb = 0.0f;         // Ziel-Kompensation in dB
    static constexpr float autoGainSmoothCoeff = 0.95f;  // Smoothing
    
    /**
     * Berechnet Auto-Gain Kompensation basierend auf aktuellen Band-Gains.
     * Verwendet frequenzgewichteten Durchschnitt (Mitten stärker gewichtet).
     */
    void updateAutoGainCompensation()
    {
        float weightedDbSum = 0.0f;
        float totalWeight = 0.0f;
        
        for (int i = 0; i < maxBands; ++i)
        {
            auto& state = bandStates[i];
            if (state.active && std::abs(state.currentGain) > 0.1f)
            {
                // Fletcher-Munson-ähnliche Gewichtung
                float weight = getPerceptualWeight(state.frequency);
                weightedDbSum += state.currentGain * weight;
                totalWeight += weight;
            }
        }
        
        if (totalWeight > 0.0f)
        {
            float avgDb = weightedDbSum / totalWeight;
            // Kompensation = negativ des gewichteten Durchschnitts (invertiert)
            autoGainTargetDb = juce::jlimit(-12.0f, 12.0f, -avgDb);
        }
        else
        {
            autoGainTargetDb = 0.0f;
        }
        
        // Smooth zum Ziel
        autoGainCompensationDb = autoGainSmoothCoeff * autoGainCompensationDb 
                                + (1.0f - autoGainSmoothCoeff) * autoGainTargetDb;
    }
    
    // Rate-Limiting & Debug-Counter (war vorher static - nicht multi-instance-safe)
    juce::int64 lastResetTime = 0;
    int processDebugCounter = 0;
    int matchDebugCounter = 0;
    
    // Reference-Track Spektrum für Zielvergleich
    std::vector<float> referenceSpectrum;
    bool hasReferenceSpectrum = false;
    
public:
    //==========================================================================
    // Reference-Track Unterstützung
    //==========================================================================
    void setReferenceSpectrum(const std::vector<float>& newSpectrum)
    {
        referenceSpectrum = newSpectrum;
        hasReferenceSpectrum = !newSpectrum.empty();
    }
    
    void clearReferenceSpectrum()
    {
        spectralMatcher.clearReference();  // SpectralMatcher auch aufräumen!
        hasReferenceSpectrum = false;
        DBG("clearReferenceSpectrum aufgerufen - hasRef=false");
    }
    
    bool hasReference() const { return hasReferenceSpectrum; }
    
    void setUseReferenceAsTarget(bool use) { settings.useReferenceAsTarget = use; }
    bool getUseReferenceAsTarget() const { return settings.useReferenceAsTarget; }
    
    //==========================================================================
    // NEU: Spectral Matcher für Reference Track EQ-Matching
    //==========================================================================
    SpectralMatcher& getSpectralMatcher() { return spectralMatcher; }
    const SpectralMatcher& getSpectralMatcher() const { return spectralMatcher; }
    
    /**
     * Setzt das Reference-Spektrum für EQ-Matching
     * Das Spektrum sollte in dB sein (wie von FFTAnalyzer)
     */
    void loadReferenceForMatching(const std::vector<float>& refSpectrum)
    {
        spectralMatcher.setReferenceSpectrum(refSpectrum);
        hasReferenceSpectrum = !refSpectrum.empty();
        
        // Debug-Info
        DBG("loadReferenceForMatching: " + juce::String(refSpectrum.size()) + 
            " bins geladen, hasRef=" + juce::String(hasReferenceSpectrum ? "true" : "false"));
    }
    
    /**
     * Aktualisiert das Input-Spektrum für Live-Matching
     * Sollte pro Block aufgerufen werden wenn Matching aktiv ist
     */
    void updateInputForMatching(const std::vector<float>& inSpectrum)
    {
        if (!hasReferenceSpectrum || !settings.useReferenceAsTarget)
            return;
        
        spectralMatcher.updateInputSpectrum(inSpectrum);
    }
    
    /**
     * Holt die Match-Punkte für EQ-Anwendung
     * Gibt Frequenz, Gain und Q für jeden erkannten Match-Punkt zurück
     */
    const std::vector<SpectralMatcher::MatchPoint>& getMatchPoints()
    {
        return spectralMatcher.getMatchPoints();
    }
    
    /**
     * Wendet die Match-Kurve auf die EQ-Bänder an
     * One-Click EQ Matching wie FabFilter Pro-Q4
     */
    void applyMatchToEQ(juce::AudioProcessorValueTreeState& apvts, int startBandIndex = 0)
    {
        const auto& matchPoints = spectralMatcher.getMatchPoints();
        
        int numAvailableBands = ParameterIDs::MAX_BANDS - startBandIndex;  // Bänder ab startBandIndex verwenden
        int numPointsToApply = std::min(static_cast<int>(matchPoints.size()), numAvailableBands);
        
        for (int i = 0; i < numPointsToApply; ++i)
        {
            int eqBandIndex = startBandIndex + i;
            const auto& point = matchPoints[static_cast<size_t>(i)];
            
            // Band aktivieren
            if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(eqBandIndex)))
            {
                param->setValueNotifyingHost(1.0f);
            }
            
            // Frequenz setzen
            if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(eqBandIndex)))
            {
                auto range = param->getNormalisableRange();
                param->setValueNotifyingHost(range.convertTo0to1(point.frequency));
            }
            
            // Gain setzen
            if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(eqBandIndex)))
            {
                auto range = param->getNormalisableRange();
                param->setValueNotifyingHost(range.convertTo0to1(point.gainDB));
            }
            
            // Q setzen
            if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(eqBandIndex)))
            {
                auto range = param->getNormalisableRange();
                param->setValueNotifyingHost(range.convertTo0to1(point.q));
            }
            
            // Typ auf Bell setzen
            if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(eqBandIndex)))
            {
                param->setValueNotifyingHost(0.0f);  // Bell
            }
            
            // Dynamic EQ explizit deaktivieren - Smart EQ hat eigenes Envelope-Processing
            if (auto* param = apvts.getParameter(ParameterIDs::getBandDynEnabledID(eqBandIndex)))
            {
                param->setValueNotifyingHost(0.0f);
            }
        }
    }
    
    /**
     * Gibt die Korrektur für eine bestimmte Frequenz zurück
     * Nützlich für GUI-Overlay
     */
    float getMatchCorrectionAtFrequency(float frequency) const
    {
        return spectralMatcher.getCorrectionAtFrequency(frequency);
    }
    
    /**
     * Match-Stärke einstellen (0-1)
     * 0 = keine Korrektur, 1 = volle Korrektur
     */
    void setMatchStrength(float strength)
    {
        spectralMatcher.setMatchStrength(strength);
    }
    
private:
    // Spectral Matcher für Reference Track EQ-Matching
    SpectralMatcher spectralMatcher;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveSmartEQ)
};
