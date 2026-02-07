#include "SmartAnalyzer.h"

SmartAnalyzer::SmartAnalyzer()
{
    // FrequenzbÃ¤nder initialisieren
    frequencyBands = {
        { 20.0f,    80.0f,   ProblemCategory::Rumble,    "Sub-Bass" },
        { 80.0f,    150.0f,  ProblemCategory::Mud,       "Bass" },
        { 150.0f,   300.0f,  ProblemCategory::Mud,       "Low-Mids" },
        { 300.0f,   600.0f,  ProblemCategory::Boxiness,  "Mids" },
        { 600.0f,   2000.0f, ProblemCategory::None,      "Upper-Mids" },
        { 2000.0f,  5000.0f, ProblemCategory::Harshness, "Presence" },
        { 5000.0f,  10000.0f,ProblemCategory::Sibilance, "Brilliance" },
        { 10000.0f, 20000.0f,ProblemCategory::None,      "Air" }
    };
    
    bandAverages.resize(frequencyBands.size(), -60.0f);
}

void SmartAnalyzer::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void SmartAnalyzer::reset()
{
    // OPTIMIERUNG: Fixed-Size Arrays - nur Counter zurÃ¼cksetzen
    detectedProblemsCount = 0;
    previousProblemsCount = 0;
    averageMagnitude = -60.0f;
    standardDeviation = 10.0f;
    std::fill(bandAverages.begin(), bandAverages.end(), -60.0f);
    samplesSinceLastAnalysis = 0;
}

void SmartAnalyzer::analyze(const FFTAnalyzer& fftAnalyzer)
{
    if (!analysisEnabled)
        return;
    
    // RT-safe Rate-Limiting: Sample-Counter statt juce::Time (keine Systemcalls im Audio-Thread)
    samplesSinceLastAnalysis += fftAnalyzer.getCurrentFFTSize();  // Approximation pro Aufruf
    int samplesNeeded = static_cast<int>((settings.analysisIntervalMs / 1000.0) * sampleRate);
    if (samplesSinceLastAnalysis < samplesNeeded)
        return;
    samplesSinceLastAnalysis = 0;
    
    // FFT-Daten holen
    const auto& magnitudes = fftAnalyzer.getMagnitudes();
    
    // Wenn keine FFT-Daten, abbrechen
    if (magnitudes.empty())
        return;
    
    // FFT-Parameter aktualisieren
    fftSize = fftAnalyzer.getCurrentFFTSize();
    numBins = fftAnalyzer.getCurrentNumBins();
    sampleRate = fftAnalyzer.getSampleRate();
    
    if (sampleRate <= 0.0 || numBins <= 0)
        return;
    
    // OPTIMIERUNG: Vorherige Probleme speichern fÃ¼r Smoothing (Fixed-Size Array Kopie)
    previousProblemsCount = detectedProblemsCount;
    for (int i = 0; i < previousProblemsCount; ++i)
        previousProblemsArray[i] = detectedProblemsArray[i];
    detectedProblemsCount = 0;  // Reset statt clear()
    
    // Statistiken berechnen
    calculateStatistics(magnitudes);
    
    // ========================================
    // ========================================
    // HAUPT-PEAK-ERKENNUNG
    // ========================================
    detectSignificantPeaks(magnitudes);
    
    // Zusätzliche spezifische Analysen wenn noch Platz für weitere Probleme
    // OPTIMIERUNG: Verwende detectedProblemsCount statt .size()
    if (detectedProblemsCount < 10)
    {
        detectResonances(magnitudes);
        detectHarshness(magnitudes);
        detectMud(magnitudes);
        detectBoxiness(magnitudes);
        detectSibilance(magnitudes);
        detectRumble(magnitudes);
    }
    
    // BOOST-Erkennung: Fehlende Frequenzbereiche erkennen (immer prüfen)
    detectLackOfAir(magnitudes);
    detectLackOfPresence(magnitudes);
    detectThinSound(magnitudes);
    detectLackOfClarity(magnitudes);
    detectLackOfWarmth(magnitudes);
    
    // Erweiterte Analyse mit neuen DSP-Modulen
    analyzeWithSpectralFeatures(magnitudes);
    detectWithInstrumentProfile();
    
    // Überlappende Probleme zusammenfassen
    consolidateProblems();
    
    // Psychoakustische Gewichtung anwenden
    applyPsychoAcousticWeighting();
    
    // Detektionen glätten für Stabilität
    smoothDetections();
    
    // OPTIMIERUNG: Sortiere Fixed-Size Array
    std::sort(detectedProblemsArray.begin(), detectedProblemsArray.begin() + detectedProblemsCount,
        [](const FrequencyProblem& a, const FrequencyProblem& b) {
            if (a.severity != b.severity)
                return static_cast<int>(a.severity) > static_cast<int>(b.severity);
            return a.confidence > b.confidence;
        });
    
    // Auf maximale Anzahl begrenzen
    if (detectedProblemsCount > settings.maxProblems)
        detectedProblemsCount = settings.maxProblems;
}

void SmartAnalyzer::calculateStatistics(const std::vector<float>& magnitudes)
{
    if (magnitudes.empty())
        return;
    
    // Globaler Durchschnitt (nur relevanter Bereich 20Hz - 20kHz)
    int startBin = frequencyToBin(20.0f);
    int endBin = frequencyToBin(20000.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float sum = 0.0f;
    int count = 0;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)  // Nur valide Werte
        {
            sum += mag;
            ++count;
        }
    }
    
    if (count > 0)
        averageMagnitude = sum / static_cast<float>(count);
    
    // Standardabweichung berechnen
    float varianceSum = 0.0f;
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            float diff = mag - averageMagnitude;
            varianceSum += diff * diff;
        }
    }
    
    if (count > 1)
        standardDeviation = std::sqrt(varianceSum / static_cast<float>(count - 1));
    
    // Band-spezifische Durchschnitte
    for (size_t i = 0; i < frequencyBands.size(); ++i)
    {
        const auto& band = frequencyBands[i];
        int bandStart = frequencyToBin(band.minFreq);
        int bandEnd = frequencyToBin(band.maxFreq);
        bandStart = juce::jlimit(0, numBins - 1, bandStart);
        bandEnd = juce::jlimit(bandStart + 1, numBins, bandEnd);
        
        float bandSum = 0.0f;
        int bandCount = 0;
        
        for (int j = bandStart; j < bandEnd; ++j)
        {
            float mag = magnitudes[static_cast<size_t>(j)];
            if (mag > -120.0f)
            {
                bandSum += mag;
                ++bandCount;
            }
        }
        
        if (bandCount > 0)
            bandAverages[i] = bandSum / static_cast<float>(bandCount);
    }
}

//==============================================================================
// ITERATIVE MULTI-PEAK-ERKENNUNG - Findet bis zu 8 signifikante Peaks
//==============================================================================
void SmartAnalyzer::detectSignificantPeaks(const std::vector<float>& magnitudes)
{
    if (magnitudes.size() < 10)
        return;
    
    const int magSize = static_cast<int>(magnitudes.size());
    const int startBin = std::max(1, frequencyToBin(25.0f));
    const int endBin = std::min(magSize - 1, frequencyToBin(16000.0f));
    if (startBin >= endBin) return;
    
    // Lokalen Durchschnitt pro Bin vorberechnen (RT-safe: stack array)
    // Verwende gleitenden Durchschnitt mit Fenster von ~1/3 Oktave
    constexpr int kMaxBins = 8192;
    std::array<float, kMaxBins> localAvg{};
    
    for (int i = startBin; i < endBin; ++i)
    {
        // Fenstergröße frequenzabhängig: ~1/3 Oktave
        int windowHalf = std::max(5, static_cast<int>(static_cast<float>(i) * 0.12f));
        windowHalf = std::min(windowHalf, 60);
        
        float sum = 0.0f;
        int count = 0;
        int lo = std::max(0, i - windowHalf);
        int hi = std::min(magSize - 1, i + windowHalf);
        for (int j = lo; j <= hi; ++j)
        {
            if (j != i) { sum += magnitudes[static_cast<size_t>(j)]; ++count; }
        }
        localAvg[static_cast<size_t>(i)] = count > 0 ? sum / static_cast<float>(count) : -60.0f;
    }
    
    // Masken-Array: markiert bereits zugewiesene Frequenzbereiche
    std::array<bool, kMaxBins> masked{};
    masked.fill(false);
    
    // Gefundene Peak-Frequenzen speichern für Spacing
    std::array<float, 12> foundFreqs{};
    int foundCount = 0;
    
    // Iterativ bis zu 8 Peaks finden
    constexpr int kMaxPeaks = 8;
    constexpr float kMinOctaveSpacing = 0.25f;  // 1/4 Oktave Mindestabstand
    
    for (int peakIter = 0; peakIter < kMaxPeaks; ++peakIter)
    {
        float bestDeviation = -999.0f;
        int bestBin = -1;
        
        for (int i = startBin; i < endBin; ++i)
        {
            if (masked[static_cast<size_t>(i)]) continue;
            
            float mag = magnitudes[static_cast<size_t>(i)];
            float avg = localAvg[static_cast<size_t>(i)];
            float dev = mag - avg;
            
            // Muss ein lokales Maximum sein (3-Punkt-Check)
            if (i > 0 && i < magSize - 1)
            {
                if (mag < magnitudes[static_cast<size_t>(i - 1)] || 
                    mag < magnitudes[static_cast<size_t>(i + 1)])
                    continue;
            }
            
            // Mindest-Magnitude: nicht im Rauschen
            if (mag < -55.0f) continue;
            
            // Mindest-Deviation: muss über lokalem Durchschnitt liegen
            if (dev < 0.8f) continue;
            
            // Prüfe Spacing zu bereits gefundenen Peaks
            float freq = binToFrequency(i);
            bool tooClose = false;
            for (int k = 0; k < foundCount; ++k)
            {
                float octDist = std::abs(std::log2(freq / foundFreqs[static_cast<size_t>(k)]));
                if (octDist < kMinOctaveSpacing) { tooClose = true; break; }
            }
            if (tooClose) continue;
            
            if (dev > bestDeviation)
            {
                bestDeviation = dev;
                bestBin = i;
            }
        }
        
        if (bestBin < 0 || bestDeviation < 0.8f) break;
        
        float freq = binToFrequency(bestBin);
        float mag = magnitudes[static_cast<size_t>(bestBin)];
        float dev = bestDeviation;
        
        // Maskiere Bereich um diesen Peak (~1/4 Oktave beidseitig)
        float maskLo = freq / std::pow(2.0f, kMinOctaveSpacing);
        float maskHi = freq * std::pow(2.0f, kMinOctaveSpacing);
        int maskBinLo = std::max(0, frequencyToBin(maskLo));
        int maskBinHi = std::min(magSize - 1, frequencyToBin(maskHi));
        for (int m = maskBinLo; m <= maskBinHi; ++m)
            masked[static_cast<size_t>(m)] = true;
        
        // Problem erstellen
        FrequencyProblem problem;
        problem.frequency = freq;
        problem.magnitude = mag;
        problem.deviation = std::max(1.0f, dev);
        
        // Kategorisierung nach Frequenzbereich
        if (freq < 60.0f)
            problem.category = ProblemCategory::Rumble;
        else if (freq < 250.0f)
            problem.category = ProblemCategory::Mud;
        else if (freq < 600.0f)
            problem.category = ProblemCategory::Boxiness;
        else if (freq < 2500.0f)
            problem.category = ProblemCategory::Resonance;
        else if (freq < 6000.0f)
            problem.category = ProblemCategory::Harshness;
        else
            problem.category = ProblemCategory::Sibilance;
        
        problem.severity = dev > 8.0f ? Severity::High : 
                          (dev > 3.0f ? Severity::Medium : Severity::Low);
        problem.confidence = juce::jlimit(0.3f, 1.0f, 0.5f + dev / 20.0f);
        // Confidence sinkt leicht für spätere Peaks
        problem.confidence *= (1.0f - static_cast<float>(peakIter) * 0.05f);
        problem.bandwidth = freq * 0.15f;
        problem.suggestedGain = -std::min(12.0f, dev * 0.7f);
        
        // Q-Faktor: Stärkere Peaks → engerer Q, breitere Probleme → breiterer Q
        // Auch frequenzabhängig: tiefe Frequenzen brauchen breiteren Q
        float baseQ = juce::jlimit(1.5f, 8.0f, dev * 0.8f);
        if (freq < 100.0f) baseQ *= 0.5f;        // Sub-Bass: sehr breit
        else if (freq < 300.0f) baseQ *= 0.7f;   // Bass: breit
        else if (freq > 5000.0f) baseQ *= 1.3f;  // Höhen: enger
        problem.suggestedQ = juce::jlimit(0.8f, 10.0f, baseQ);
        
        addProblem(problem);
        foundFreqs[static_cast<size_t>(foundCount)] = freq;
        ++foundCount;
    }
}

void SmartAnalyzer::detectResonances(const std::vector<float>& magnitudes)
{
    // Resonanzen: Schmale Peaks, die deutlich Ã¼ber dem lokalen Durchschnitt liegen
    // Threshold basiert auf Deviation, nicht absoluter Magnitude
    
    // Sliding window fÃ¼r lokalen Durchschnitt
    const int windowSize = 21;  // ~100Hz bei 44.1kHz/2048
    const int halfWindow = windowSize / 2;
    
    int startBin = frequencyToBin(30.0f);   // Resonanzen ab 30 Hz (auch Sub-Bass!)
    int endBin = frequencyToBin(12000.0f);  // Bis 12 kHz
    startBin = juce::jlimit(halfWindow, numBins - halfWindow - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins - halfWindow, endBin);
    
    for (int i = startBin; i < endBin; ++i)
    {
        float currentMag = magnitudes[static_cast<size_t>(i)];
        
        // Lokalen Durchschnitt berechnen (ohne den aktuellen Bin)
        float localSum = 0.0f;
        for (int j = i - halfWindow; j <= i + halfWindow; ++j)
        {
            if (j != i)
                localSum += magnitudes[static_cast<size_t>(j)];
        }
        float localAvg = localSum / static_cast<float>(windowSize - 1);
        
        float deviation = currentMag - localAvg;
        
        // PrÃ¼fen ob es ein lokales Maximum ist
        bool isPeak = (currentMag > magnitudes[static_cast<size_t>(i - 1)] &&
                       currentMag > magnitudes[static_cast<size_t>(i + 1)]);
        
        // Threshold: 2dB lokale Deviation reicht fÃ¼r Erkennung
        float deviationThreshold = 2.0f * settings.resonanceSensitivity;
        if (isPeak && deviation > deviationThreshold && currentMag > -50.0f)
        {
            FrequencyProblem problem;
            problem.frequency = binToFrequency(i);
            problem.magnitude = currentMag;
            problem.deviation = deviation;
            problem.category = ProblemCategory::Resonance;
            problem.severity = calculateSeverity(deviation, settings.resonanceSensitivity);
            problem.confidence = juce::jlimit(0.0f, 1.0f, deviation / 20.0f);
            problem.bandwidth = calculateBandwidth(magnitudes, i, currentMag - 3.0f);  // -3dB Bandbreite
            problem.suggestedGain = suggestGainReduction(deviation, problem.severity);
            problem.suggestedQ = suggestQFactor(problem.bandwidth, problem.frequency);
            
            addProblem(problem);
        }
    }
}

void SmartAnalyzer::detectHarshness(const std::vector<float>& magnitudes)
{
    // Harshness: ErhÃ¶hte Energie im 2-5 kHz Bereich
    int startBin = frequencyToBin(2000.0f);
    int endBin = frequencyToBin(5000.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    // Durchschnitt in diesem Bereich vs. globaler Durchschnitt
    float bandSum = 0.0f;
    int bandCount = 0;
    float peakMag = -120.0f;
    int peakBin = startBin;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        bandSum += mag;
        ++bandCount;
        
        if (mag > peakMag)
        {
            peakMag = mag;
            peakBin = i;
        }
    }
    
    float bandAvg = bandCount > 0 ? bandSum / static_cast<float>(bandCount) : -60.0f;
    float deviation = bandAvg - averageMagnitude;
    
    if (deviation > settings.minimumDeviation * settings.harshnessSensitivity * 0.5f)
    {
        FrequencyProblem problem;
        problem.frequency = binToFrequency(peakBin);
        problem.magnitude = peakMag;
        problem.deviation = deviation;
        problem.category = ProblemCategory::Harshness;
        problem.severity = calculateSeverity(deviation, settings.harshnessSensitivity);
        problem.confidence = juce::jlimit(0.0f, 1.0f, deviation / 15.0f);
        problem.bandwidth = 2000.0f;  // Breiter Bereich
        problem.suggestedGain = suggestGainReduction(deviation, problem.severity) * 0.7f;  // Weniger aggressiv
        problem.suggestedQ = 0.7f;  // Breite Glocke
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectMud(const std::vector<float>& magnitudes)
{
    // Mud: ÃœbermÃ¤ÃŸige Energie im 100-300 Hz Bereich
    int startBin = frequencyToBin(100.0f);
    int endBin = frequencyToBin(300.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    float peakMag = -120.0f;
    int peakBin = startBin;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        bandSum += mag;
        ++bandCount;
        
        if (mag > peakMag)
        {
            peakMag = mag;
            peakBin = i;
        }
    }
    
    float bandAvg = bandCount > 0 ? bandSum / static_cast<float>(bandCount) : -60.0f;
    float deviation = bandAvg - averageMagnitude;
    
    if (deviation > settings.minimumDeviation * settings.mudSensitivity * 0.6f)
    {
        FrequencyProblem problem;
        problem.frequency = binToFrequency(peakBin);
        problem.magnitude = peakMag;
        problem.deviation = deviation;
        problem.category = ProblemCategory::Mud;
        problem.severity = calculateSeverity(deviation, settings.mudSensitivity);
        problem.confidence = juce::jlimit(0.0f, 1.0f, deviation / 12.0f);
        problem.bandwidth = 150.0f;
        problem.suggestedGain = suggestGainReduction(deviation, problem.severity) * 0.8f;
        problem.suggestedQ = 1.0f;
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectBoxiness(const std::vector<float>& magnitudes)
{
    // Boxiness: Resonanzen im 300-600 Hz Bereich
    int startBin = frequencyToBin(300.0f);
    int endBin = frequencyToBin(600.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    float peakMag = -120.0f;
    int peakBin = startBin;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        bandSum += mag;
        ++bandCount;
        
        if (mag > peakMag)
        {
            peakMag = mag;
            peakBin = i;
        }
    }
    
    float bandAvg = bandCount > 0 ? bandSum / static_cast<float>(bandCount) : -60.0f;
    float deviation = bandAvg - averageMagnitude;
    
    if (deviation > settings.minimumDeviation * settings.boxinessSensitivity * 0.7f)
    {
        FrequencyProblem problem;
        problem.frequency = binToFrequency(peakBin);
        problem.magnitude = peakMag;
        problem.deviation = deviation;
        problem.category = ProblemCategory::Boxiness;
        problem.severity = calculateSeverity(deviation, settings.boxinessSensitivity);
        problem.confidence = juce::jlimit(0.0f, 1.0f, deviation / 12.0f);
        problem.bandwidth = 200.0f;
        problem.suggestedGain = suggestGainReduction(deviation, problem.severity) * 0.75f;
        problem.suggestedQ = 1.5f;
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectSibilance(const std::vector<float>& magnitudes)
{
    // Sibilance: S-Laute im 5-10 kHz Bereich
    int startBin = frequencyToBin(5000.0f);
    int endBin = frequencyToBin(10000.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    float peakMag = -120.0f;
    int peakBin = startBin;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        bandSum += mag;
        ++bandCount;
        
        if (mag > peakMag)
        {
            peakMag = mag;
            peakBin = i;
        }
    }
    
    float bandAvg = bandCount > 0 ? bandSum / static_cast<float>(bandCount) : -60.0f;
    float deviation = bandAvg - averageMagnitude;
    
    if (deviation > settings.minimumDeviation * settings.sibilanceSensitivity * 0.5f)
    {
        FrequencyProblem problem;
        problem.frequency = binToFrequency(peakBin);
        problem.magnitude = peakMag;
        problem.deviation = deviation;
        problem.category = ProblemCategory::Sibilance;
        problem.severity = calculateSeverity(deviation, settings.sibilanceSensitivity);
        problem.confidence = juce::jlimit(0.0f, 1.0f, deviation / 15.0f);
        problem.bandwidth = 3000.0f;
        problem.suggestedGain = suggestGainReduction(deviation, problem.severity) * 0.6f;
        problem.suggestedQ = 0.5f;
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectRumble(const std::vector<float>& magnitudes)
{
    // Rumble: Tieffrequentes Rumpeln unter 80 Hz
    // Erkenne sowohl generellen Sub-Bass-Ãœberschuss als auch einzelne Peaks
    int startBin = frequencyToBin(20.0f);
    int endBin = frequencyToBin(80.0f);
    startBin = juce::jlimit(1, numBins - 2, startBin);
    endBin = juce::jlimit(startBin + 1, numBins - 1, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    float peakMag = -120.0f;
    int peakBin = startBin;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        bandSum += mag;
        ++bandCount;
        
        if (mag > peakMag)
        {
            peakMag = mag;
            peakBin = i;
        }
    }
    
    float bandAvg = bandCount > 0 ? bandSum / static_cast<float>(bandCount) : -60.0f;
    
    // Berechne wie viel der Peak Ã¼ber dem Band-Durchschnitt liegt
    float peakDeviation = peakMag - bandAvg;
    // Und wie viel der Band-Durchschnitt Ã¼ber dem globalen liegt
    float bandDeviation = bandAvg - averageMagnitude;
    
    // Rumble erkennen: entweder starker Peak im Sub-Bass ODER genereller Sub-Bass-Ãœberschuss
    bool hasPeakProblem = peakMag > -30.0f && peakDeviation > 3.0f;
    bool hasBandProblem = bandDeviation > 2.0f * settings.rumbleSensitivity && peakMag > -40.0f;
    
    if (hasPeakProblem || hasBandProblem)
    {
        FrequencyProblem problem;
        problem.frequency = binToFrequency(peakBin);
        problem.magnitude = peakMag;
        problem.deviation = hasPeakProblem ? peakDeviation : bandDeviation;
        problem.category = ProblemCategory::Rumble;
        problem.severity = hasPeakProblem ? 
            calculateSeverity(peakDeviation, settings.rumbleSensitivity) :
            calculateSeverity(bandDeviation, settings.rumbleSensitivity);
        problem.confidence = juce::jlimit(0.0f, 1.0f, problem.deviation / 15.0f);
        problem.bandwidth = 40.0f;
        problem.suggestedGain = -12.0f;  // High-Pass empfohlen
        problem.suggestedQ = 0.7f;
        
        addProblem(problem);
    }
}

void SmartAnalyzer::consolidateProblems()
{
    // OPTIMIERUNG: Fixed-Size Array Version
    // Überlappende Probleme zusammenfassen - aggressiver als vorher
    if (detectedProblemsCount < 2)
        return;
    
    std::array<FrequencyProblem, maxDetectedProblems> consolidated;
    std::array<bool, maxDetectedProblems> merged = {};
    int consolidatedCount = 0;
    
    // Zuerst nach Frequenz sortieren für bessere Konsolidierung
    std::sort(detectedProblemsArray.begin(), detectedProblemsArray.begin() + detectedProblemsCount,
        [](const FrequencyProblem& a, const FrequencyProblem& b) {
            return a.frequency < b.frequency;
        });
    
    for (int i = 0; i < detectedProblemsCount; ++i)
    {
        if (merged[i])
            continue;
        
        FrequencyProblem current = detectedProblemsArray[i];
        
        for (int j = i + 1; j < detectedProblemsCount; ++j)
        {
            if (merged[j])
                continue;
            
            const FrequencyProblem& other = detectedProblemsArray[j];
            
            // Aggressivere Zusammenfassung: innerhalb 1/3 Oktave ODER gleiche Kategorie nahe
            float ratio = current.frequency / other.frequency;
            bool closeFrequency = (ratio > 0.8f && ratio < 1.26f);  // ~1/3 Oktave
            bool sameCategory = (current.category == other.category);
            
            if (closeFrequency || (sameCategory && ratio > 0.7f && ratio < 1.43f))
            {
                // Zusammenfassen - das stärkere Problem behalten
                if (other.deviation > current.deviation)
                {
                    current = other;
                }
                current.confidence = std::max(current.confidence, other.confidence);
                merged[j] = true;
            }
        }
        
        if (consolidatedCount < maxDetectedProblems)
            consolidated[consolidatedCount++] = current;
    }
    
    // Zurückkopieren
    for (int i = 0; i < consolidatedCount; ++i)
        detectedProblemsArray[i] = consolidated[i];
    detectedProblemsCount = consolidatedCount;
}

void SmartAnalyzer::smoothDetections()
{
    // OPTIMIERUNG: Fixed-Size Array Version
    // Detektionen über Zeit glätten für Stabilität
    if (previousProblemsCount == 0)
        return;
    
    for (int i = 0; i < detectedProblemsCount; ++i)
    {
        auto& current = detectedProblemsArray[i];
        
        // Vorheriges passendes Problem finden
        for (int j = 0; j < previousProblemsCount; ++j)
        {
            const auto& prev = previousProblemsArray[j];
            float ratio = current.frequency / prev.frequency;
            if (ratio > 0.9f && ratio < 1.1f && current.category == prev.category)
            {
                // Smoothing anwenden
                current.confidence = settings.detectionSmoothing * prev.confidence +
                                    (1.0f - settings.detectionSmoothing) * current.confidence;
                current.deviation = settings.detectionSmoothing * prev.deviation +
                                   (1.0f - settings.detectionSmoothing) * current.deviation;
                break;
            }
        }
    }
}

float SmartAnalyzer::calculateBandwidth(const std::vector<float>& magnitudes, int peakIndex, float threshold)
{
    if (peakIndex < 1 || peakIndex >= numBins - 1)
        return 100.0f;  // Default
    
    // -3dB Punkte finden
    int lowerBin = peakIndex;
    int upperBin = peakIndex;
    
    while (lowerBin > 0 && magnitudes[static_cast<size_t>(lowerBin)] > threshold)
        --lowerBin;
    
    while (upperBin < numBins - 1 && magnitudes[static_cast<size_t>(upperBin)] > threshold)
        ++upperBin;
    
    float lowerFreq = binToFrequency(lowerBin);
    float upperFreq = binToFrequency(upperBin);
    
    return upperFreq - lowerFreq;
}

float SmartAnalyzer::suggestGainReduction(float deviation, Severity severity)
{
    // Empfohlene Gain-Reduktion basierend auf Abweichung und Schweregrad
    float baseReduction = -deviation * 0.7f;  // 70% der Abweichung
    
    switch (severity)
    {
        case Severity::Low:
            return juce::jlimit(-6.0f, 0.0f, baseReduction * 0.5f);
        case Severity::Medium:
            return juce::jlimit(-9.0f, 0.0f, baseReduction * 0.75f);
        case Severity::High:
            return juce::jlimit(-12.0f, 0.0f, baseReduction);
        default:
            return -3.0f;
    }
}

float SmartAnalyzer::suggestQFactor(float bandwidth, float frequency)
{
    // Q = Frequenz / Bandbreite
    if (bandwidth <= 0.0f)
        return 1.0f;
    
    float q = frequency / bandwidth;
    return juce::jlimit(0.3f, 10.0f, q);
}

SmartAnalyzer::Severity SmartAnalyzer::calculateSeverity(float deviation, float sensitivity)
{
    float adjustedDeviation = deviation * sensitivity;
    
    if (adjustedDeviation > 12.0f)
        return Severity::High;
    else if (adjustedDeviation > 8.0f)
        return Severity::Medium;
    else
        return Severity::Low;
}

void SmartAnalyzer::setSensitivity(float globalSensitivity)
{
    float factor = juce::jlimit(0.1f, 2.0f, globalSensitivity);
    settings.resonanceSensitivity = 0.7f * factor;
    settings.harshnessSensitivity = 0.6f * factor;
    settings.mudSensitivity = 0.5f * factor;
    settings.boxinessSensitivity = 0.5f * factor;
    settings.sibilanceSensitivity = 0.6f * factor;
    settings.rumbleSensitivity = 0.5f * factor;
}

std::vector<SmartAnalyzer::FrequencyProblem> SmartAnalyzer::getProblemsInRange(float minFreq, float maxFreq) const
{
    // OPTIMIERUNG: Verwendet jetzt Fixed-Size Array
    std::vector<FrequencyProblem> result;
    result.reserve(detectedProblemsCount);
    
    for (int i = 0; i < detectedProblemsCount; ++i)
    {
        const auto& problem = detectedProblemsArray[i];
        if (problem.frequency >= minFreq && problem.frequency <= maxFreq)
            result.push_back(problem);
    }
    
    return result;
}

const SmartAnalyzer::FrequencyProblem* SmartAnalyzer::getMostSevereProblem() const
{
    // OPTIMIERUNG: Verwendet jetzt Fixed-Size Array
    if (detectedProblemsCount == 0)
        return nullptr;
    
    return &detectedProblemsArray[0];  // Bereits nach Schweregrad sortiert
}

int SmartAnalyzer::frequencyToBin(float frequency) const
{
    if (sampleRate <= 0.0)
        return 0;
    
    int bin = static_cast<int>(frequency * static_cast<float>(fftSize) / static_cast<float>(sampleRate));
    return juce::jlimit(0, numBins - 1, bin);
}

float SmartAnalyzer::binToFrequency(int bin) const
{
    if (fftSize <= 0)
        return 0.0f;
    
    return static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
}

//==============================================================================
// Erweiterte Analyse-Methoden mit neuen DSP-Modulen
//==============================================================================

void SmartAnalyzer::analyzeWithSpectralFeatures(const std::vector<float>& magnitudes)
{
    // Spektralanalyse durchfÃ¼hren
    spectralAnalysis.prepare(sampleRate, fftSize);
    cachedMetrics = spectralAnalysis.analyze(magnitudes);
    
    // Timbrale Eigenschaften für erweiterte Erkennung nutzen
    // Hohe Helligkeit + niedrige Wärme = potentiell harsch
    if (cachedMetrics.brightness > 0.7f && cachedMetrics.warmth < 0.3f)
    {
        // Verstärke Harshness-Detektion
        for (int i = 0; i < detectedProblemsCount; ++i)
        {
            auto& problem = detectedProblemsArray[i];
            if (problem.category == ProblemCategory::Harshness)
            {
                problem.confidence = std::min(1.0f, problem.confidence * 1.2f);
            }
        }
    }
    
    // Hohe Muddiness = bestätige Mud-Probleme
    if (cachedMetrics.muddiness > 0.6f)
    {
        for (int i = 0; i < detectedProblemsCount; ++i)
        {
            auto& problem = detectedProblemsArray[i];
            if (problem.category == ProblemCategory::Mud)
            {
                problem.confidence = std::min(1.0f, problem.confidence * 1.2f);
            }
        }
    }
    
    // Niedrige Tonalität = wahrscheinlich perkussiv, Resonanzen weniger relevant
    if (cachedMetrics.tonality < 0.3f)
    {
        for (int i = 0; i < detectedProblemsCount; ++i)
        {
            auto& problem = detectedProblemsArray[i];
            if (problem.category == ProblemCategory::Resonance)
            {
                problem.confidence *= 0.8f;
            }
        }
    }
}

void SmartAnalyzer::applyPsychoAcousticWeighting()
{
    if (!usePsychoAcousticWeighting)
        return;
    
    // OPTIMIERUNG: Fixed-Size Array Version
    // Sanfte psychoakustische Anpassung - nicht zu aggressiv filtern
    for (int i = 0; i < detectedProblemsCount; ++i)
    {
        auto& problem = detectedProblemsArray[i];
        // A-Gewichtung für zusätzliche Korrektur (zwischen 0.5 und 1.0)
        float aWeight = psychoModel.getAWeighting(problem.frequency);
        float adjustedWeight = 0.5f + (aWeight * 0.5f);  // Dämpfe den Effekt
        
        // Confidence nur leicht anpassen (mindestens 70% behalten)
        problem.confidence *= std::max(0.7f, adjustedWeight);
    }
    
    // NUR Probleme mit sehr niedriger Confidence entfernen (< 0.1)
    // In-place Filterung für Fixed-Size Array
    int newCount = 0;
    for (int i = 0; i < detectedProblemsCount; ++i)
    {
        if (detectedProblemsArray[i].confidence >= 0.1f)
        {
            if (newCount != i)
                detectedProblemsArray[newCount] = detectedProblemsArray[i];
            newCount++;
        }
    }
    detectedProblemsCount = newCount;
}

void SmartAnalyzer::detectWithInstrumentProfile()
{
    if (currentProfileName == "Default")
        return;  // Kein spezifisches Profil aktiv
    
    const auto& profile = currentProfile;
    
    // OPTIMIERUNG: Fixed-Size Array Version
    // Kritische Frequenzbänder des Profils besonders beachten
    for (int i = 0; i < detectedProblemsCount; ++i)
    {
        auto& problem = detectedProblemsArray[i];
        
        // Mud-Bereich
        if (problem.frequency >= profile.criticalBands.mudLow &&
            problem.frequency <= profile.criticalBands.mudHigh)
        {
            if (problem.category == ProblemCategory::Mud)
            {
                problem.confidence = std::min(1.0f, problem.confidence * 1.3f);
                if (problem.severity == Severity::Low)
                    problem.severity = Severity::Medium;
            }
        }
        
        // Box-Bereich
        if (problem.frequency >= profile.criticalBands.boxLow &&
            problem.frequency <= profile.criticalBands.boxHigh)
        {
            if (problem.category == ProblemCategory::Boxiness)
            {
                problem.confidence = std::min(1.0f, problem.confidence * 1.3f);
                if (problem.severity == Severity::Low)
                    problem.severity = Severity::Medium;
            }
        }
        
        // Harshness-Bereich
        if (problem.frequency >= profile.criticalBands.harshLow &&
            problem.frequency <= profile.criticalBands.harshHigh)
        {
            if (problem.category == ProblemCategory::Harshness)
            {
                problem.confidence = std::min(1.0f, problem.confidence * 1.3f);
                if (problem.severity == Severity::Low)
                    problem.severity = Severity::Medium;
            }
        }
        
        // Sibilance-Bereich
        if (problem.frequency >= profile.criticalBands.sibilanceLow &&
            problem.frequency <= profile.criticalBands.sibilanceHigh)
        {
            if (problem.category == ProblemCategory::Sibilance)
            {
                problem.confidence = std::min(1.0f, problem.confidence * 1.3f);
                if (problem.severity == Severity::Low)
                    problem.severity = Severity::Medium;
            }
        }
    }
    
    // Gegen Zielkurve vergleichen
    auto targetPoints = instrumentProfiles.getTargetCurvePoints(profile);
    
    for (const auto& targetPoint : targetPoints)
    {
        float freq = targetPoint.first;
        float targetDb = targetPoint.second;
        
        // Finde den nächsten Bin
        int bin = frequencyToBin(freq);
        if (bin >= 0 && bin < numBins)
        {
            float currentLevel = averageMagnitude;  // Globaler Durchschnitt als Fallback
            
            // Finde das passende Frequenzband
            for (size_t bandIdx = 0; bandIdx < frequencyBands.size(); ++bandIdx)
            {
                if (freq >= frequencyBands[bandIdx].minFreq && freq < frequencyBands[bandIdx].maxFreq)
                {
                    currentLevel = bandAverages[bandIdx];
                    break;
                }
            }
            
            float deviation = currentLevel - targetDb;
            
            // Wenn wir deutlich über der Zielkurve liegen, ein neues Problem erstellen
            if (deviation > settings.minimumDeviation && std::abs(deviation) > 3.0f)
            {
                // Prüfe ob schon ein Problem in der Nähe existiert
                bool alreadyReported = false;
                for (int i = 0; i < detectedProblemsCount; ++i)
                {
                    if (std::abs(detectedProblemsArray[i].frequency - freq) < freq * 0.1f)
                    {
                        alreadyReported = true;
                        break;
                    }
                }
                
                if (!alreadyReported)
                {
                    FrequencyProblem newProblem;
                    newProblem.frequency = freq;
                    newProblem.magnitude = currentLevel;
                    newProblem.deviation = deviation;
                    newProblem.category = (deviation > 0) ? ProblemCategory::Resonance : ProblemCategory::None;
                    newProblem.severity = (std::abs(deviation) > 6.0f) ? Severity::Medium : Severity::Low;
                    newProblem.confidence = 0.6f;
                    newProblem.suggestedGain = -deviation * 0.7f;
                    newProblem.suggestedQ = 1.0f;
                    
                    if (newProblem.category != ProblemCategory::None)
                        addProblem(newProblem);
                }
            }
        }
    }
}

//==============================================================================
// BOOST-ERKENNUNG: Fehlende Frequenzbereiche erkennen
//==============================================================================

void SmartAnalyzer::detectLackOfAir(const std::vector<float>& magnitudes)
{
    // Fehlende "Luft" = zu wenig Energie über 10 kHz im Vergleich zum Gesamtspektrum
    int startBin = frequencyToBin(10000.0f);
    int endBin = frequencyToBin(18000.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            bandSum += mag;
            ++bandCount;
        }
    }
    
    if (bandCount == 0) return;
    
    float bandAvg = bandSum / static_cast<float>(bandCount);
    
    // Vergleiche mit dem Mitteltonbereich (1-4 kHz) als Referenz
    int refStart = frequencyToBin(1000.0f);
    int refEnd = frequencyToBin(4000.0f);
    refStart = juce::jlimit(0, numBins - 1, refStart);
    refEnd = juce::jlimit(refStart + 1, numBins, refEnd);
    
    float refSum = 0.0f;
    int refCount = 0;
    for (int i = refStart; i < refEnd; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            refSum += mag;
            ++refCount;
        }
    }
    
    float refAvg = refCount > 0 ? refSum / static_cast<float>(refCount) : -60.0f;
    
    // Natürlicher Roll-off der Höhen ist ~6dB/Oktave, also erwarten wir ~12dB weniger bei 12kHz vs 3kHz
    // Wenn deutlich mehr fehlt, haben wir "Lack of Air"
    float expectedDrop = 12.0f;  // Erwarteter natürlicher Drop
    float actualDrop = refAvg - bandAvg;
    float deficit = actualDrop - expectedDrop;
    
    if (deficit > 4.0f)  // Mindestens 4dB mehr als erwartet fehlt
    {
        FrequencyProblem problem;
        problem.frequency = 12000.0f;  // Zentrum des Air-Bereichs
        problem.magnitude = bandAvg;
        problem.deviation = -deficit;  // Negativ = unter Ziel
        problem.category = ProblemCategory::LackOfAir;
        problem.severity = deficit > 8.0f ? Severity::High : (deficit > 6.0f ? Severity::Medium : Severity::Low);
        problem.confidence = juce::jlimit(0.4f, 0.9f, deficit / 12.0f);
        problem.bandwidth = 6000.0f;
        problem.suggestedGain = std::min(6.0f, deficit * 0.5f);  // POSITIV = Boost
        problem.suggestedQ = 0.5f;  // Sehr breit (Shelf-artig)
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectLackOfPresence(const std::vector<float>& magnitudes)
{
    // Fehlende Präsenz = zu wenig Energie im 3-6 kHz Bereich
    int startBin = frequencyToBin(3000.0f);
    int endBin = frequencyToBin(6000.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            bandSum += mag;
            ++bandCount;
        }
    }
    
    if (bandCount == 0) return;
    
    float bandAvg = bandSum / static_cast<float>(bandCount);
    float deficit = averageMagnitude - bandAvg;
    
    // Wenn der Präsenz-Bereich deutlich unter dem Durchschnitt liegt
    if (deficit > 3.0f)
    {
        FrequencyProblem problem;
        problem.frequency = 4000.0f;
        problem.magnitude = bandAvg;
        problem.deviation = -deficit;
        problem.category = ProblemCategory::LackOfPresence;
        problem.severity = deficit > 6.0f ? Severity::High : (deficit > 4.0f ? Severity::Medium : Severity::Low);
        problem.confidence = juce::jlimit(0.4f, 0.85f, deficit / 10.0f);
        problem.bandwidth = 2000.0f;
        problem.suggestedGain = std::min(6.0f, deficit * 0.6f);  // POSITIV = Boost
        problem.suggestedQ = 0.8f;
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectThinSound(const std::vector<float>& magnitudes)
{
    // Dünner Klang = fehlender Body im 80-250 Hz Bereich
    int startBin = frequencyToBin(80.0f);
    int endBin = frequencyToBin(250.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            bandSum += mag;
            ++bandCount;
        }
    }
    
    if (bandCount == 0) return;
    
    float bandAvg = bandSum / static_cast<float>(bandCount);
    
    // Vergleiche mit Mittenbereich (500-2000 Hz)
    int midStart = frequencyToBin(500.0f);
    int midEnd = frequencyToBin(2000.0f);
    midStart = juce::jlimit(0, numBins - 1, midStart);
    midEnd = juce::jlimit(midStart + 1, numBins, midEnd);
    
    float midSum = 0.0f;
    int midCount = 0;
    for (int i = midStart; i < midEnd; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            midSum += mag;
            ++midCount;
        }
    }
    
    float midAvg = midCount > 0 ? midSum / static_cast<float>(midCount) : -60.0f;
    float deficit = midAvg - bandAvg;
    
    // Wenn der Bass/Body-Bereich deutlich unter den Mitten liegt
    if (deficit > 4.0f)
    {
        FrequencyProblem problem;
        problem.frequency = 150.0f;
        problem.magnitude = bandAvg;
        problem.deviation = -deficit;
        problem.category = ProblemCategory::ThinSound;
        problem.severity = deficit > 8.0f ? Severity::High : (deficit > 5.0f ? Severity::Medium : Severity::Low);
        problem.confidence = juce::jlimit(0.4f, 0.85f, deficit / 12.0f);
        problem.bandwidth = 150.0f;
        problem.suggestedGain = std::min(6.0f, deficit * 0.5f);  // POSITIV = Boost
        problem.suggestedQ = 0.7f;
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectLackOfClarity(const std::vector<float>& magnitudes)
{
    // Fehlende Klarheit = zu wenig Energie im 1-3 kHz Bereich
    int startBin = frequencyToBin(1000.0f);
    int endBin = frequencyToBin(3000.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            bandSum += mag;
            ++bandCount;
        }
    }
    
    if (bandCount == 0) return;
    
    float bandAvg = bandSum / static_cast<float>(bandCount);
    float deficit = averageMagnitude - bandAvg;
    
    if (deficit > 3.0f)
    {
        FrequencyProblem problem;
        problem.frequency = 2000.0f;
        problem.magnitude = bandAvg;
        problem.deviation = -deficit;
        problem.category = ProblemCategory::LackOfClarity;
        problem.severity = deficit > 6.0f ? Severity::High : (deficit > 4.0f ? Severity::Medium : Severity::Low);
        problem.confidence = juce::jlimit(0.4f, 0.85f, deficit / 10.0f);
        problem.bandwidth = 1500.0f;
        problem.suggestedGain = std::min(5.0f, deficit * 0.5f);  // POSITIV = Boost
        problem.suggestedQ = 0.8f;
        
        addProblem(problem);
    }
}

void SmartAnalyzer::detectLackOfWarmth(const std::vector<float>& magnitudes)
{
    // Fehlende Wärme = zu wenig Energie im 200-500 Hz Bereich
    int startBin = frequencyToBin(200.0f);
    int endBin = frequencyToBin(500.0f);
    startBin = juce::jlimit(0, numBins - 1, startBin);
    endBin = juce::jlimit(startBin + 1, numBins, endBin);
    
    float bandSum = 0.0f;
    int bandCount = 0;
    
    for (int i = startBin; i < endBin; ++i)
    {
        float mag = magnitudes[static_cast<size_t>(i)];
        if (mag > -120.0f)
        {
            bandSum += mag;
            ++bandCount;
        }
    }
    
    if (bandCount == 0) return;
    
    float bandAvg = bandSum / static_cast<float>(bandCount);
    float deficit = averageMagnitude - bandAvg;
    
    // Aber nur wenn kein Mud-Problem existiert (sonst widersprechen sich die Empfehlungen)
    bool hasMudProblem = false;
    for (int i = 0; i < detectedProblemsCount; ++i)
    {
        if (detectedProblemsArray[i].category == ProblemCategory::Mud ||
            detectedProblemsArray[i].category == ProblemCategory::Boxiness)
        {
            hasMudProblem = true;
            break;
        }
    }
    
    if (!hasMudProblem && deficit > 3.0f)
    {
        FrequencyProblem problem;
        problem.frequency = 300.0f;
        problem.magnitude = bandAvg;
        problem.deviation = -deficit;
        problem.category = ProblemCategory::LackOfWarmth;
        problem.severity = deficit > 6.0f ? Severity::High : (deficit > 4.0f ? Severity::Medium : Severity::Low);
        problem.confidence = juce::jlimit(0.4f, 0.8f, deficit / 10.0f);
        problem.bandwidth = 200.0f;
        problem.suggestedGain = std::min(4.0f, deficit * 0.4f);  // POSITIV = Boost, aber vorsichtig
        problem.suggestedQ = 0.7f;
        
        addProblem(problem);
    }
}
