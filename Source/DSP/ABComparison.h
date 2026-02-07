#pragma once

#include <JuceHeader.h>
#include <vector>
#include <deque>
#include <memory>
#include "../Parameters/ParameterIDs.h"

/**
 * ABComparison: A/B Vergleich und Snapshot-System für Smart EQ
 * 
 * Features:
 * - A/B Toggle zwischen Original und bearbeitetem Signal
 * - Delta-Listen (nur die EQ-Änderungen hören)
 * - Snapshots speichern und vergleichen
 * - History der Änderungen mit Undo/Redo
 * - Automatisches Gain-Matching für fairen Vergleich
 */
class ABComparison
{
public:
    //==========================================================================
    // Snapshot-Struktur
    //==========================================================================
    struct Snapshot
    {
        juce::String name;
        juce::Time timestamp;
        
        // EQ-Band Einstellungen
        struct BandSettings
        {
            float frequency = 1000.0f;
            float gain = 0.0f;
            float q = 1.0f;
            int filterType = 0;
            bool active = false;
            bool bypassed = false;
        };
        std::array<BandSettings, ParameterIDs::MAX_BANDS> bands;
        
        // Globale Einstellungen
        float inputGain = 0.0f;
        float outputGain = 0.0f;
        bool midSideMode = false;
        
        // Metriken zum Zeitpunkt des Snapshots
        float lufs = -23.0f;
        float peakDb = -6.0f;
    };
    
    //==========================================================================
    // Vergleichs-Modi
    //==========================================================================
    enum class CompareMode
    {
        Normal,         // Normaler Betrieb (Processed)
        Bypass,         // A = Original (Bypass)
        Delta,          // Nur die Differenz hören
        A,              // Snapshot A
        B               // Snapshot B
    };
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    ABComparison()
    {
        currentMode = CompareMode::Normal;
        autoGainMatch = true;
    }
    
    void prepare(double newSampleRate, int newBlockSize)
    {
        sampleRate = newSampleRate;
        blockSize = newBlockSize;
        
        // Delay-Buffer für Delta-Berechnung
        originalBuffer.setSize(2, blockSize);
        originalBuffer.clear();
    }
    
    //==========================================================================
    // Snapshot Management
    //==========================================================================
    
    /**
     * Speichert aktuellen Zustand als Snapshot
     */
    void saveSnapshot(const juce::String& name, const juce::AudioProcessorValueTreeState& apvts)
    {
        Snapshot snap;
        snap.name = name;
        snap.timestamp = juce::Time::getCurrentTime();
        
        // EQ-Bänder auslesen (0-indiziert, Format: "bandN_param")
        for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
        {
            if (auto* param = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(i)))
                snap.bands[static_cast<size_t>(i)].frequency = param->load();
            if (auto* param = apvts.getRawParameterValue(ParameterIDs::getBandGainID(i)))
                snap.bands[static_cast<size_t>(i)].gain = param->load();
            if (auto* param = apvts.getRawParameterValue(ParameterIDs::getBandQID(i)))
                snap.bands[static_cast<size_t>(i)].q = param->load();
            if (auto* param = apvts.getRawParameterValue(ParameterIDs::getBandTypeID(i)))
                snap.bands[static_cast<size_t>(i)].filterType = static_cast<int>(param->load());
            if (auto* param = apvts.getRawParameterValue(ParameterIDs::getBandActiveID(i)))
                snap.bands[static_cast<size_t>(i)].active = param->load() > 0.5f;
            if (auto* param = apvts.getRawParameterValue(ParameterIDs::getBandBypassID(i)))
                snap.bands[static_cast<size_t>(i)].bypassed = param->load() > 0.5f;
        }
        
        // Globale Parameter
        if (auto* param = apvts.getRawParameterValue(ParameterIDs::INPUT_GAIN))
            snap.inputGain = param->load();
        if (auto* param = apvts.getRawParameterValue(ParameterIDs::OUTPUT_GAIN))
            snap.outputGain = param->load();
        
        // Zur History hinzufügen
        history.push_back(snap);
        
        // Max 50 Snapshots
        if (history.size() > 50)
            history.pop_front();
        
        // Als A oder B setzen
        if (!snapshotA.has_value())
            snapshotA = snap;
        else if (!snapshotB.has_value())
            snapshotB = snap;
    }
    
    /**
     * Lädt Snapshot in die Parameter
     */
    void loadSnapshot(const Snapshot& snap, juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
        {
            if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(i)))
                param->setValueNotifyingHost(param->convertTo0to1(snap.bands[static_cast<size_t>(i)].frequency));
            if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(i)))
                param->setValueNotifyingHost(param->convertTo0to1(snap.bands[static_cast<size_t>(i)].gain));
            if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(i)))
                param->setValueNotifyingHost(param->convertTo0to1(snap.bands[static_cast<size_t>(i)].q));
            if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(i)))
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(snap.bands[static_cast<size_t>(i)].filterType)));
            if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(i)))
                param->setValueNotifyingHost(snap.bands[static_cast<size_t>(i)].active ? 1.0f : 0.0f);
            if (auto* param = apvts.getParameter(ParameterIDs::getBandBypassID(i)))
                param->setValueNotifyingHost(snap.bands[static_cast<size_t>(i)].bypassed ? 1.0f : 0.0f);
        }
        
        if (auto* param = apvts.getParameter(ParameterIDs::INPUT_GAIN))
            param->setValueNotifyingHost(param->convertTo0to1(snap.inputGain));
        if (auto* param = apvts.getParameter(ParameterIDs::OUTPUT_GAIN))
            param->setValueNotifyingHost(param->convertTo0to1(snap.outputGain));
    }
    
    //==========================================================================
    // A/B Comparison
    //==========================================================================
    
    void setSnapshotA(const Snapshot& snap) { snapshotA = snap; }
    void setSnapshotB(const Snapshot& snap) { snapshotB = snap; }
    
    const std::optional<Snapshot>& getSnapshotA() const { return snapshotA; }
    const std::optional<Snapshot>& getSnapshotB() const { return snapshotB; }
    
    void swapAB()
    {
        std::swap(snapshotA, snapshotB);
    }
    
    /**
     * Toggle zwischen A und B
     */
    void toggleAB(juce::AudioProcessorValueTreeState& apvts)
    {
        if (currentMode == CompareMode::A && snapshotB.has_value())
        {
            currentMode = CompareMode::B;
            loadSnapshot(*snapshotB, apvts);
        }
        else if (snapshotA.has_value())
        {
            currentMode = CompareMode::A;
            loadSnapshot(*snapshotA, apvts);
        }
    }
    
    //==========================================================================
    // Audio Processing
    //==========================================================================
    
    /**
     * Speichert Original-Audio für Delta-Vergleich
     */
    void captureOriginal(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() >= 2 && buffer.getNumSamples() <= originalBuffer.getNumSamples())
        {
            for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
            {
                originalBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }
        }
    }
    
    /**
     * Verarbeitet Audio basierend auf aktuellem Modus
     * @param processedBuffer Das verarbeitete Signal
     * @param originalCaptured True wenn captureOriginal aufgerufen wurde
     */
    void processCompare(juce::AudioBuffer<float>& processedBuffer, bool originalCaptured = true)
    {
        switch (currentMode)
        {
            case CompareMode::Normal:
                // Nichts tun - processed Output
                break;
                
            case CompareMode::Bypass:
                // Original wiederherstellen
                if (originalCaptured)
                {
                    for (int ch = 0; ch < juce::jmin(2, processedBuffer.getNumChannels()); ++ch)
                    {
                        processedBuffer.copyFrom(ch, 0, originalBuffer, ch, 0, 
                                                  juce::jmin(processedBuffer.getNumSamples(), 
                                                             originalBuffer.getNumSamples()));
                    }
                }
                break;
                
            case CompareMode::Delta:
                // Differenz berechnen: Processed - Original
                if (originalCaptured)
                {
                    for (int ch = 0; ch < juce::jmin(2, processedBuffer.getNumChannels()); ++ch)
                    {
                        float* proc = processedBuffer.getWritePointer(ch);
                        const float* orig = originalBuffer.getReadPointer(ch);
                        
                        for (int i = 0; i < juce::jmin(processedBuffer.getNumSamples(), 
                                                       originalBuffer.getNumSamples()); ++i)
                        {
                            proc[i] = proc[i] - orig[i];
                            
                            // Delta-Boost für bessere Hörbarkeit
                            proc[i] *= deltaBoost;
                        }
                    }
                }
                break;
                
            case CompareMode::A:
            case CompareMode::B:
                // Bereits durch Snapshot-Loading behandelt
                break;
        }
        
        // Auto Gain Matching
        if (autoGainMatch && currentMode != CompareMode::Normal)
        {
            applyGainMatch(processedBuffer);
        }
    }
    
    //==========================================================================
    // Mode Control
    //==========================================================================
    
    void setMode(CompareMode mode) { currentMode = mode; }
    CompareMode getMode() const { return currentMode; }
    
    void toggleBypass()
    {
        if (currentMode == CompareMode::Bypass)
            currentMode = CompareMode::Normal;
        else
            currentMode = CompareMode::Bypass;
    }
    
    void toggleDelta()
    {
        if (currentMode == CompareMode::Delta)
            currentMode = CompareMode::Normal;
        else
            currentMode = CompareMode::Delta;
    }
    
    bool isBypassed() const { return currentMode == CompareMode::Bypass; }
    bool isDeltaMode() const { return currentMode == CompareMode::Delta; }
    
    //==========================================================================
    // Settings
    //==========================================================================
    
    void setAutoGainMatch(bool enabled) { autoGainMatch = enabled; }
    bool isAutoGainMatchEnabled() const { return autoGainMatch; }
    
    void setDeltaBoost(float boost) { deltaBoost = juce::jlimit(1.0f, 20.0f, boost); }
    float getDeltaBoost() const { return deltaBoost; }
    
    //==========================================================================
    // History
    //==========================================================================
    
    const std::deque<Snapshot>& getHistory() const { return history; }
    
    void clearHistory() { history.clear(); }
    
    bool canUndo() const { return history.size() > 1 && historyIndex > 0; }
    bool canRedo() const { return historyIndex < history.size() - 1; }
    
    void undo(juce::AudioProcessorValueTreeState& apvts)
    {
        if (canUndo())
        {
            --historyIndex;
            loadSnapshot(history[historyIndex], apvts);
        }
    }
    
    void redo(juce::AudioProcessorValueTreeState& apvts)
    {
        if (canRedo())
        {
            ++historyIndex;
            loadSnapshot(history[historyIndex], apvts);
        }
    }
    
private:
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    CompareMode currentMode;
    
    std::optional<Snapshot> snapshotA;
    std::optional<Snapshot> snapshotB;
    
    std::deque<Snapshot> history;
    size_t historyIndex = 0;
    
    juce::AudioBuffer<float> originalBuffer;
    
    bool autoGainMatch = true;
    float deltaBoost = 6.0f;  // Delta um 6dB boosten für Hörbarkeit
    
    float referenceLevel = -18.0f;  // Referenz-Pegel für Gain Matching
    
    //==========================================================================
    // Hilfsfunktionen
    //==========================================================================
    
    void applyGainMatch(juce::AudioBuffer<float>& buffer)
    {
        // RMS des Buffers berechnen
        float rms = 0.0f;
        int totalSamples = 0;
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                rms += data[i] * data[i];
                ++totalSamples;
            }
        }
        
        if (totalSamples > 0 && rms > 0.0f)
        {
            rms = std::sqrt(rms / static_cast<float>(totalSamples));
            float currentDb = 20.0f * std::log10(rms + 1e-10f);
            
            // Gain anpassen um auf Referenzpegel zu kommen
            float gainDb = referenceLevel - currentDb;
            gainDb = juce::jlimit(-12.0f, 12.0f, gainDb);
            
            float gainLinear = std::pow(10.0f, gainDb / 20.0f);
            
            buffer.applyGain(gainLinear);
        }
    }
};
