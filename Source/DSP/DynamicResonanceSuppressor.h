#pragma once

#include <JuceHeader.h>
#include "SVFFilter.h"
#include <vector>
#include <cmath>
#include <array>

/**
 * DynamicResonanceSuppressor: Automatische dynamische Resonanzunterdrückung
 * Inspiriert von soothe2 - unterdrückt nur Resonanzen wenn sie auftreten
 * 
 * Features:
 * - Echtzeit-Resonanzerkennung pro Band
 * - Dynamische Gain-Reduktion mit Attack/Release
 * - Transient-Preservation (schützt Attacks)
 * - Frequenz-spezifische Thresholds
 * - Soft-Knee Kompression
 */
class DynamicResonanceSuppressor
{
public:
    //==========================================================================
    // Einstellungen
    //==========================================================================
    struct Settings
    {
        float depth = 0.5f;           // 0-1, wie stark unterdrückt wird
        float speed = 0.5f;           // 0-1, Attack/Release Geschwindigkeit  
        float selectivity = 0.5f;     // 0-1, wie schmal die Detektion ist
        float sharpness = 0.5f;       // 0-1, Fokus auf scharfe Resonanzen
        
        // Frequenzbereich
        float lowFreq = 200.0f;
        float highFreq = 8000.0f;
        
        // Dynamics
        float threshold = -20.0f;     // dB über lokalem Durchschnitt
        float ratio = 4.0f;           // Kompressionsrate
        float attackMs = 5.0f;        // Attack in ms
        float releaseMs = 50.0f;      // Release in ms
        float kneeWidth = 6.0f;       // Soft-knee in dB
        
        // Transient Protection
        bool transientProtection = true;
        float transientThreshold = 0.3f;  // 0-1
    };
    
    //==========================================================================
    // Band-Status für Visualisierung
    //==========================================================================
    struct BandStatus
    {
        float frequency = 0.0f;
        float gainReduction = 0.0f;   // In dB (negativ = Reduktion)
        float inputLevel = 0.0f;      // Input dB
        float threshold = 0.0f;       // Aktueller Threshold
        bool isActive = false;        // Unterdrückung aktiv?
    };
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    DynamicResonanceSuppressor()
    {
        reset();
    }
    
    void prepare(double newSampleRate, int newBlockSize = 512)
    {
        sampleRate = newSampleRate;
        blockSize = newBlockSize;
        
        // Envelope Follower Koeffizienten berechnen
        updateEnvelopeCoefficients();
        
        // SVF Filterbank initialisieren
        initializeFilters();
        
        reset();
    }
    
    // FFT-Größe extern setzen (muss vor process() aufgerufen werden)
    void setFFTSize(int newFftSize)
    {
        fftSize = newFftSize;
        numBins = fftSize / 2 + 1;
    }
    
    void reset()
    {
        std::fill(envelopeStates.begin(), envelopeStates.end(), -100.0f);
        std::fill(gainReductionStates.begin(), gainReductionStates.end(), 0.0f);
        std::fill(transientDetectorStates.begin(), transientDetectorStates.end(), 0.0f);
        std::fill(cachedGainReductions.begin(), cachedGainReductions.end(), 0.0f);
        std::fill(cachedLocalAverages.begin(), cachedLocalAverages.end(), -60.0f);
        std::fill(smoothingTempBuffer.begin(), smoothingTempBuffer.end(), 0.0f);
        std::fill(previousMagnitudes.begin(), previousMagnitudes.end(), 0.0f);
        previousMagnitudesSize = 0;
        currentNumBins = 0;
        
        // SVF Filter-States zurücksetzen
        for (int ch = 0; ch < MAX_PROCESS_CHANNELS; ++ch)
            for (int b = 0; b < NUM_SUPPRESS_BANDS; ++b)
                suppressionFilters[ch][b].reset();
        std::fill(std::begin(currentBandGainDB), std::end(currentBandGainDB), 0.0f);
    }
    
    //==========================================================================
    // Hauptverarbeitung
    //==========================================================================
    
    /**
     * Analysiert das Spektrum und berechnet Gain-Reduktionen
     * @param magnitudesDb FFT-Magnitudes in dB
     * Ergebnis wird intern in cachedGainReductions gespeichert (RT-safe, keine Allokation)
     */
    void process(const std::vector<float>& magnitudesDb)
    {
        const size_t inputNumBins = magnitudesDb.size();
        
        if (inputNumBins < 10 || inputNumBins > MAX_BINS)
            return;
        
        // Ergebnis-Array auf aktuelle Größe setzen (kein resize, nur Länge merken)
        currentNumBins = static_cast<int>(inputNumBins);
        
        // Zurücksetzen
        for (size_t i = 0; i < inputNumBins; ++i)
            cachedGainReductions[i] = 0.0f;
        
        // Lokalen Durchschnitt für jeden Bin berechnen
        calculateLocalAverages(magnitudesDb);
        
        // Transient Detection
        bool isTransient = false;
        if (settings.transientProtection)
        {
            isTransient = detectTransient(magnitudesDb);
        }
        
        // Für jeden Bin: Resonanz-Check und dynamische Unterdrückung
        for (size_t i = 1; i < inputNumBins; ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            
            // Frequenzbereich prüfen
            if (freq < settings.lowFreq || freq > settings.highFreq)
                continue;
            
            float input = magnitudesDb[i];
            float localAvg = cachedLocalAverages[i];
            float deviation = input - localAvg;
            
            // Envelope Following (Peak Detection) - sicherer Index
            size_t envIdx = i % envelopeStates.size();
            float& envelope = envelopeStates[envIdx];
            if (deviation > envelope)
            {
                envelope = attackCoeff * envelope + (1.0f - attackCoeff) * deviation;
            }
            else
            {
                envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * deviation;
            }
            
            // Threshold berechnen (dynamisch basierend auf Settings)
            float threshold = settings.threshold * (1.0f - settings.selectivity * 0.5f);
            
            // Resonanz-Detektion
            size_t grIdx = i % gainReductionStates.size();
            float& grState = gainReductionStates[grIdx];
            
            if (envelope > threshold)
            {
                // Kompression berechnen (Soft-Knee)
                float overThreshold = envelope - threshold;
                float gainRedDb = calculateCompression(overThreshold);
                
                // Depth-Skalierung
                gainRedDb *= settings.depth;
                
                // Transient-Schutz
                if (isTransient && settings.transientProtection)
                {
                    gainRedDb *= (1.0f - settings.transientThreshold);
                }
                
                // Smoothing der Gain-Reduktion (samplerate-abhängig)
                if (gainRedDb < grState)
                    grState = grState * grAttackCoeff + gainRedDb * (1.0f - grAttackCoeff);
                else
                    grState = grState * grReleaseCoeff + gainRedDb * (1.0f - grReleaseCoeff);
                
                cachedGainReductions[i] = grState;
            }
            else
            {
                // Release der Gain-Reduktion (samplerate-abhängig)
                grState *= grReleaseCoeff;
                cachedGainReductions[i] = grState;
            }
        }
        
        // Optional: Gain-Reduktionen glätten über benachbarte Bins
        smoothGainReductions();
    }
    
    /**
     * Wendet die berechneten Gain-Reduktionen frequenzselektiv auf einen Audio-Buffer an.
     * Nutzt eine 16-Band SVF-Filterbank (Cytomic TPT) für echte per-Frequenz Unterdrückung.
     * Jedes Band wird dynamisch nur so weit gedämpft, wie die Resonanzerkennung es vorgibt.
     * SVF-Topologie ist modulationsstabil — keine Clicks bei schnellen Gain-Änderungen.
     */
    void applyToBuffer(juce::AudioBuffer<float>& buffer, int /*fftSize*/)
    {
        if (currentNumBins < 10 || !filtersInitialized)
            return;
        
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(buffer.getNumChannels(), MAX_PROCESS_CHANNELS);
        
        if (numChannels == 0 || numSamples == 0)
            return;
        
        // Bandfrequenzen aktualisieren falls sich der Frequenzbereich geändert hat
        updateBandFrequencies();
        
        // Per-Band Gain-Reduktion aus den per-Bin Daten berechnen
        for (int b = 0; b < NUM_SUPPRESS_BANDS; ++b)
        {
            int binStart = frequencyToBin(bandFreqEdges[b]);
            int binEnd = frequencyToBin(bandFreqEdges[b + 1]);
            binStart = juce::jlimit(0, currentNumBins - 1, binStart);
            binEnd = juce::jlimit(binStart + 1, currentNumBins, binEnd);
            
            float sum = 0.0f;
            float minGR = 0.0f;
            int count = 0;
            for (int i = binStart; i < binEnd; ++i)
            {
                sum += cachedGainReductions[static_cast<size_t>(i)];
                minGR = std::min(minGR, cachedGainReductions[static_cast<size_t>(i)]);
                ++count;
            }
            
            // Mischung aus Durchschnitt und Minimum für natürlichere Unterdrückung
            float avg = count > 0 ? sum / static_cast<float>(count) : 0.0f;
            float bandGainDB = (avg + minGR) * 0.5f;
            
            // Nur signifikante Reduktionen anwenden
            if (bandGainDB > -0.1f)
                bandGainDB = 0.0f;
            
            // Smoothing der Band-Gains für click-freie Übergänge
            currentBandGainDB[b] += (bandGainDB - currentBandGainDB[b]) * 0.3f;
            
            // Filter-Gain für alle Kanäle aktualisieren (nutzt effizientes updateGainOnly)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                suppressionFilters[ch][b].updateGainOnly(currentBandGainDB[b]);
            }
        }
        
        // Audio-Buffer durch die SVF-Filterbank verarbeiten
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int b = 0; b < NUM_SUPPRESS_BANDS; ++b)
            {
                // Nur aktive Bänder verarbeiten (spart CPU)
                if (currentBandGainDB[b] < -0.1f)
                {
                    suppressionFilters[ch][b].processBlock(data, numSamples);
                }
            }
        }
    }
    
    /**
     * Gibt die gecachten Gain-Reduktionen zurück (für Visualisierung)
     */
    const float* getGainReductions() const { return cachedGainReductions.data(); }
    int getNumBins() const { return currentNumBins; }
    
    /**
     * Liefert Status für alle überwachten Bänder (für Visualisierung)
     * Gibt die Anzahl der gültigen Einträge zurück.
     */
    static constexpr int NUM_STATUS_FREQS = 14;
    
    int getBandStatus(std::array<BandStatus, NUM_STATUS_FREQS>& status) const
    {
        // Typische Frequenzen für Visualisierung
        static const float freqs[NUM_STATUS_FREQS] = { 
            100, 200, 300, 400, 500, 700, 1000, 1500, 
            2000, 3000, 4000, 5000, 7000, 10000 
        };
        
        int count = 0;
        for (int f = 0; f < NUM_STATUS_FREQS; ++f)
        {
            float freq = freqs[f];
            if (freq >= settings.lowFreq && freq <= settings.highFreq)
            {
                int bin = frequencyToBin(freq);
                if (bin >= 0 && static_cast<size_t>(bin) < gainReductionStates.size())
                {
                    status[count].frequency = freq;
                    status[count].gainReduction = gainReductionStates[static_cast<size_t>(bin)];
                    status[count].isActive = status[count].gainReduction < -0.5f;
                    ++count;
                }
            }
        }
        
        return count;
    }
    
    //==========================================================================
    // Getter/Setter
    //==========================================================================
    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }
    
    void setSettings(const Settings& newSettings)
    {
        bool freqChanged = (std::abs(settings.lowFreq - newSettings.lowFreq) > 0.1f ||
                            std::abs(settings.highFreq - newSettings.highFreq) > 0.1f);
        settings = newSettings;
        updateEnvelopeCoefficients();
        if (freqChanged && filtersInitialized)
            updateBandFrequencies();
    }
    
    void setDepth(float depth) { settings.depth = juce::jlimit(0.0f, 1.0f, depth); }
    void setSpeed(float speed) 
    { 
        settings.speed = juce::jlimit(0.0f, 1.0f, speed);
        updateEnvelopeCoefficients();
    }
    void setSelectivity(float sel) { settings.selectivity = juce::jlimit(0.0f, 1.0f, sel); }
    void setFrequencyRange(float low, float high) 
    { 
        settings.lowFreq = low; 
        settings.highFreq = high;
        if (filtersInitialized)
            updateBandFrequencies();
    }
    
    float getTotalGainReduction() const
    {
        float total = 0.0f;
        for (const auto& gr : gainReductionStates)
        {
            if (gr < 0.0f)
                total += gr;
        }
        return total / static_cast<float>(gainReductionStates.size());
    }
    
private:
    Settings settings;
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    int fftSize = 2048;
    int numBins = 1025;
    int currentNumBins = 0;
    
    // Maximale Bin-Anzahl (8192/2+1 = 4097 für Maximum FFT)
    static constexpr size_t MAX_BINS = 4097;
    
    // Envelope Follower States (groß genug für alle FFT-Größen)
    std::array<float, MAX_BINS> envelopeStates;
    std::array<float, MAX_BINS> gainReductionStates;
    std::array<float, MAX_BINS> transientDetectorStates;
    
    // Pre-allokierte Ergebnis-Caches (RT-safe, keine Heap-Allokation)
    std::array<float, MAX_BINS> cachedGainReductions;
    std::array<float, MAX_BINS> cachedLocalAverages;
    
    // Separater Temp-Buffer für smoothGainReductions (nicht cachedLocalAverages missbrauchen)
    std::array<float, MAX_BINS> smoothingTempBuffer;
    
    // Vorheriges Spektrum für Transient-Detection (fixed-size, RT-safe)
    std::array<float, MAX_BINS> previousMagnitudes;
    int previousMagnitudesSize = 0;
    
    float attackCoeff = 0.9f;
    float releaseCoeff = 0.99f;
    
    // Gain-Reduction Smoothing (samplerate-abhängig)
    float grAttackCoeff = 0.9f;
    float grReleaseCoeff = 0.99f;
    
    // =========================================================================
    // SVF Filterbank für frequenzselektive Gain-Reduktion
    // =========================================================================
    static constexpr int NUM_SUPPRESS_BANDS = 16;
    static constexpr int MAX_PROCESS_CHANNELS = 2;
    SVFFilter suppressionFilters[MAX_PROCESS_CHANNELS][NUM_SUPPRESS_BANDS];
    float bandCenterFreqs[NUM_SUPPRESS_BANDS] = {};
    float bandQValues[NUM_SUPPRESS_BANDS] = {};
    float bandFreqEdges[NUM_SUPPRESS_BANDS + 1] = {};
    float currentBandGainDB[NUM_SUPPRESS_BANDS] = {};
    bool filtersInitialized = false;
    float lastLowFreq = 0.0f;
    float lastHighFreq = 0.0f;
    
    /**
     * Initialisiert die SVF-Filterbank mit logarithmisch verteilten Bändern.
     */
    void initializeFilters()
    {
        lastLowFreq = 0.0f;  // Force update
        lastHighFreq = 0.0f;
        updateBandFrequencies();
        
        for (int ch = 0; ch < MAX_PROCESS_CHANNELS; ++ch)
        {
            for (int b = 0; b < NUM_SUPPRESS_BANDS; ++b)
            {
                suppressionFilters[ch][b].prepare(sampleRate, 0);
                suppressionFilters[ch][b].setParameters(
                    ParameterIDs::FilterType::Bell,
                    bandCenterFreqs[b], 0.0f, bandQValues[b]);
            }
        }
        std::fill(std::begin(currentBandGainDB), std::end(currentBandGainDB), 0.0f);
        filtersInitialized = true;
    }
    
    /**
     * Aktualisiert Bandfrequenzen wenn sich der Frequenzbereich ändert.
     * Nutzt logarithmische Verteilung für perceptuell gleichmäßige Bänder.
     */
    void updateBandFrequencies()
    {
        if (std::abs(lastLowFreq - settings.lowFreq) < 0.1f &&
            std::abs(lastHighFreq - settings.highFreq) < 0.1f)
            return;
        
        lastLowFreq = settings.lowFreq;
        lastHighFreq = settings.highFreq;
        
        const float logMin = std::log2(std::max(settings.lowFreq, 20.0f));
        const float logMax = std::log2(std::min(settings.highFreq, 20000.0f));
        
        // Bandkanten berechnen
        for (int b = 0; b <= NUM_SUPPRESS_BANDS; ++b)
        {
            float logF = logMin + (logMax - logMin) * static_cast<float>(b) / NUM_SUPPRESS_BANDS;
            bandFreqEdges[b] = std::pow(2.0f, logF);
        }
        
        // Constant-Q: Q = 1/(2*sinh(ln2/2 * BW_octaves))
        float octavesPerBand = (logMax - logMin) / static_cast<float>(NUM_SUPPRESS_BANDS);
        float q = (octavesPerBand > 0.01f)
            ? 1.0f / (2.0f * std::sinh(std::log(2.0f) / 2.0f * octavesPerBand))
            : 4.0f;
        
        for (int b = 0; b < NUM_SUPPRESS_BANDS; ++b)
        {
            // Geometrisches Mittel der Bandkanten = Center-Frequenz
            bandCenterFreqs[b] = std::sqrt(bandFreqEdges[b] * bandFreqEdges[b + 1]);
            bandQValues[b] = q;
        }
        
        // Filter aktualisieren falls bereits initialisiert
        if (filtersInitialized)
        {
            for (int ch = 0; ch < MAX_PROCESS_CHANNELS; ++ch)
            {
                for (int b = 0; b < NUM_SUPPRESS_BANDS; ++b)
                {
                    suppressionFilters[ch][b].setParameters(
                        ParameterIDs::FilterType::Bell,
                        bandCenterFreqs[b], currentBandGainDB[b], bandQValues[b]);
                }
            }
        }
    }

    float binToFrequency(int bin) const
    {
        return static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    }
    
    int frequencyToBin(float freq) const
    {
        return static_cast<int>(std::round(freq * static_cast<float>(fftSize) / static_cast<float>(sampleRate)));
    }
    
    void updateEnvelopeCoefficients()
    {
        // Speed -> Attack/Release Zeiten
        float attackMs = juce::jmap(settings.speed, 20.0f, 1.0f);
        float releaseMs = juce::jmap(settings.speed, 200.0f, 20.0f);
        
        // Koeffizienten für Blockweise Verarbeitung
        float attackSamples = static_cast<float>(sampleRate) * attackMs / 1000.0f;
        float releaseSamples = static_cast<float>(sampleRate) * releaseMs / 1000.0f;
        
        attackCoeff = std::exp(-1.0f / (attackSamples / static_cast<float>(blockSize)));
        releaseCoeff = std::exp(-1.0f / (releaseSamples / static_cast<float>(blockSize)));
        
        // Gain-Reduction Smoothing: 5ms Attack, 50ms Release (samplerate-abhängig)
        float grAttackSamples = static_cast<float>(sampleRate) * 0.005f;  // 5ms
        float grReleaseSamples = static_cast<float>(sampleRate) * 0.05f;  // 50ms
        grAttackCoeff = std::exp(-1.0f / (grAttackSamples / static_cast<float>(blockSize)));
        grReleaseCoeff = std::exp(-1.0f / (grReleaseSamples / static_cast<float>(blockSize)));
    }
    
    /**
     * Berechnet lokalen Durchschnitt für jeden Bin (RT-safe, nutzt cachedLocalAverages).
     * Optimiert mit Prefix-Sum für O(n) statt O(n*windowSize).
     */
    void calculateLocalAverages(const std::vector<float>& mags)
    {
        const size_t numBinsLocal = mags.size();
        
        if (numBinsLocal == 0 || numBinsLocal > MAX_BINS)
            return;
        
        // Fensterbreite basierend auf Selectivity
        int windowSize = static_cast<int>(21 + (1.0f - settings.selectivity) * 30);
        windowSize |= 1;  // Ungerade machen
        int halfWindow = windowSize / 2;
        
        // Prefix-Sum berechnen für O(1) Bereichssummen
        // Nutze smoothingTempBuffer als Temp-Speicher (wird erst später in smoothGainReductions gebraucht)
        auto& prefix = smoothingTempBuffer;
        prefix[0] = mags[0];
        for (size_t i = 1; i < numBinsLocal; ++i)
            prefix[i] = prefix[i - 1] + mags[i];
        
        // Lokalen Durchschnitt pro Bin in O(1) berechnen
        for (size_t i = 0; i < numBinsLocal; ++i)
        {
            int start = std::max(0, static_cast<int>(i) - halfWindow);
            int end = std::min(static_cast<int>(numBinsLocal) - 1, static_cast<int>(i) + halfWindow);
            
            // Bereichssumme über Prefix-Sum: sum[start..end] = prefix[end] - prefix[start-1]
            float windowSum = prefix[static_cast<size_t>(end)] 
                            - (start > 0 ? prefix[static_cast<size_t>(start - 1)] : 0.0f);
            
            // Center-Bin abziehen (Durchschnitt der Nachbarn, ohne sich selbst)
            float centerVal = mags[i];
            int count = end - start;  // Elemente im Fenster minus 1 (Center ausgeschlossen)
            
            cachedLocalAverages[i] = count > 0 ? (windowSum - centerVal) / static_cast<float>(count) : -60.0f;
        }
    }
    
    /**
     * Soft-Knee Kompression berechnen
     */
    float calculateCompression(float overThresholdDb) const
    {
        if (overThresholdDb <= 0.0f)
            return 0.0f;
        
        float kneeHalf = settings.kneeWidth / 2.0f;
        
        if (overThresholdDb < kneeHalf)
        {
            // Soft-knee Region
            float x = overThresholdDb + kneeHalf;
            float compression = (1.0f / settings.ratio - 1.0f) * x * x / (4.0f * kneeHalf);
            return compression;
        }
        else
        {
            // Hard-knee (über Knee)
            float compression = (1.0f / settings.ratio - 1.0f) * (overThresholdDb - kneeHalf) 
                              + (1.0f / settings.ratio - 1.0f) * kneeHalf / 2.0f;
            return compression;
        }
    }
    
    /**
     * Transient-Detektion (schnelle Energie-Änderung)
     */
    bool detectTransient(const std::vector<float>& mags)
    {
        if (previousMagnitudesSize != static_cast<int>(mags.size()))
        {
            size_t copySize = std::min(mags.size(), static_cast<size_t>(MAX_BINS));
            std::copy(mags.begin(), mags.begin() + static_cast<std::ptrdiff_t>(copySize), previousMagnitudes.begin());
            previousMagnitudesSize = static_cast<int>(copySize);
            return false;
        }
        
        float flux = 0.0f;
        int count = 0;
        
        for (size_t i = 1; i < mags.size() && i < MAX_BINS; ++i)
        {
            float diff = mags[i] - previousMagnitudes[i];
            if (diff > 0.0f)  // Nur positive Änderungen (Onsets)
            {
                flux += diff;
                ++count;
            }
        }
        
        size_t copySize = std::min(mags.size(), static_cast<size_t>(MAX_BINS));
        std::copy(mags.begin(), mags.begin() + static_cast<std::ptrdiff_t>(copySize), previousMagnitudes.begin());
        previousMagnitudesSize = static_cast<int>(copySize);
        
        float avgFlux = count > 0 ? flux / static_cast<float>(count) : 0.0f;
        
        // Threshold für Transient (in dB Änderung)
        return avgFlux > (10.0f * settings.transientThreshold);
    }
    
    /**
     * Glättet Gain-Reduktionen über benachbarte Bins (in-place, RT-safe)
     */
    void smoothGainReductions()
    {
        if (currentNumBins < 5)
            return;
        
        // Nutze separaten Temp-Buffer (nicht cachedLocalAverages missbrauchen!)
        auto& smoothed = smoothingTempBuffer;
        const size_t n = static_cast<size_t>(currentNumBins);
        
        for (size_t i = 2; i < n - 2; ++i)
        {
            smoothed[i] = (cachedGainReductions[i-2] * 0.1f + cachedGainReductions[i-1] * 0.2f 
                         + cachedGainReductions[i] * 0.4f 
                         + cachedGainReductions[i+1] * 0.2f + cachedGainReductions[i+2] * 0.1f);
        }
        
        smoothed[0] = cachedGainReductions[0];
        smoothed[1] = cachedGainReductions[1];
        smoothed[n-2] = cachedGainReductions[n-2];
        smoothed[n-1] = cachedGainReductions[n-1];
        
        // Ergebnis zurückkopieren
        for (size_t i = 0; i < n; ++i)
            cachedGainReductions[i] = smoothed[i];
    }
};
