#include "EQBand.h"

EQBand::EQBand()
{
    updateFilters();
}

void EQBand::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    
    for (int i = 0; i < MAX_CASCADE; ++i)
    {
        filtersLeft[i].prepare(sampleRate, samplesPerBlock);
        filtersRight[i].prepare(sampleRate, samplesPerBlock);
    }
    
    // SVF-Filter für Dynamic EQ initialisieren
    svfLeft.prepare(sampleRate, samplesPerBlock);
    svfRight.prepare(sampleRate, samplesPerBlock);
    
    updateFilters();
    updateEnvelopeCoefficients();  // OPTIMIERUNG: Envelope-Koeffizienten initialisieren
}

void EQBand::reset()
{
    for (int i = 0; i < MAX_CASCADE; ++i)
    {
        filtersLeft[i].reset();
        filtersRight[i].reset();
    }
    svfLeft.reset();
    svfRight.reset();
}

void EQBand::setFrequency(float newFrequency)
{
    if (frequency != newFrequency)
    {
        frequency = newFrequency;
        updateFilters();
    }
}

void EQBand::setGain(float gainDB)
{
    if (gain != gainDB)
    {
        gain = gainDB;
        updateFilters();
    }
}

void EQBand::setQ(float newQ)
{
    if (q != newQ)
    {
        q = newQ;
        updateFilters();
    }
}

void EQBand::setType(ParameterIDs::FilterType type)
{
    if (filterType != type)
    {
        filterType = type;
        updateFilters();
    }
}

void EQBand::setChannelMode(ParameterIDs::ChannelMode mode)
{
    channelMode = mode;
}

void EQBand::setBypassed(bool isBypassed)
{
    bypassed = isBypassed;
}

void EQBand::setSlope(int slopeDB)
{
    if (slope != slopeDB)
    {
        slope = slopeDB;
        // Berechne Anzahl der Kaskadenstufen
        numCascadeStages = juce::jlimit(1, MAX_CASCADE, slopeDB / 12);
        updateFilters();
    }
}

// Dynamic EQ Setters (NEW)
void EQBand::setDynamicMode(bool enabled)
{
    dynamicMode = enabled;
    if (enabled)
    {
        envelopeLeft = 0.0f;
        envelopeRight = 0.0f;
        dynamicGainReduction = 0.0f;
    }
}

void EQBand::setThreshold(float thresholdDB)
{
    threshold = juce::jlimit(-60.0f, 0.0f, thresholdDB);
}

void EQBand::setRatio(float newRatio)
{
    ratio = juce::jlimit(1.0f, 10.0f, newRatio);
}

void EQBand::setAttack(float attackMs)
{
    attack = juce::jlimit(0.1f, 500.0f, attackMs);
    updateEnvelopeCoefficients();  // OPTIMIERUNG: Koeffizienten aktualisieren
}

void EQBand::setRelease(float releaseMs)
{
    release = juce::jlimit(10.0f, 2000.0f, releaseMs);
    updateEnvelopeCoefficients();  // OPTIMIERUNG: Koeffizienten aktualisieren
}

void EQBand::setParameters(float newFrequency, float gainDB, float newQ,
                            ParameterIDs::FilterType type,
                            ParameterIDs::ChannelMode newChannelMode,
                            bool isBypassed)
{
    frequency = newFrequency;
    gain = gainDB;
    q = newQ;
    filterType = type;
    channelMode = newChannelMode;
    bypassed = isBypassed;
    updateFilters();
}

void EQBand::updateFilters()
{
    // Gain pro Stufe für Cut-Filter (verteilen über Kaskaden)
    float gainPerStage = gain;
    
    // Für Cut-Filter: Q anpassen für Butterworth-Charakteristik bei Kaskadierung
    bool isCutFilter = (filterType == ParameterIDs::FilterType::LowCut || 
                         filterType == ParameterIDs::FilterType::HighCut);
    
    if (isCutFilter)
    {
        // Bei Cut-Filtern hat Gain keine Bedeutung
        gainPerStage = 0.0f;
    }

    for (int i = 0; i < MAX_CASCADE; ++i)
    {
        if (i < numCascadeStages)
        {
            float stageQ = q;
            
            if (isCutFilter)
            {
                // Exakte Butterworth Q-Werte pro Stufe:
                // Q_k = 1 / (2 * sin(pi * (2k+1) / (2 * 2n)))
                // wobei n = numCascadeStages (Anzahl 2nd-Order Sektionen)
                // k = 0, 1, ..., n-1
                // Die Gesamtordnung ist 2n (jede Sektion ist 2. Ordnung)
                const int totalOrder = numCascadeStages * 2;
                stageQ = 1.0f / (2.0f * std::sin(
                    juce::MathConstants<float>::pi * static_cast<float>(2 * i + 1) 
                    / static_cast<float>(2 * totalOrder)));
            }
            
            filtersLeft[i].updateCoefficients(filterType, frequency, gainPerStage, stageQ, slope);
            filtersRight[i].updateCoefficients(filterType, frequency, gainPerStage, stageQ, slope);
        }
        else
        {
            // Nicht verwendete Stufen auf Unity setzen
            filtersLeft[i].updateCoefficients(ParameterIDs::FilterType::Bell, 1000.0f, 0.0f, 1.0f);
            filtersRight[i].updateCoefficients(ParameterIDs::FilterType::Bell, 1000.0f, 0.0f, 1.0f);
        }
    }
}

void EQBand::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (bypassed || !active)
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels < 2)
    {
        // Mono: nur linken Kanal verarbeiten
        auto* data = buffer.getWritePointer(0);
        
        // Dynamic EQ für Mono — SVF-basiert (modulationsstabil)
        if (dynamicMode)
        {
            // Einmalig Basis-Parameter setzen (inkl. teurem tan())
            svfLeft.setParameters(filterType, frequency, gain, q);
            
            for (int i = 0; i < numSamples; ++i)
            {
                float env = processEnvelope(std::abs(data[i]), envelopeLeft);
                float dynamicGain = calculateDynamicGain(env);
                float effectiveGain = gain * dynamicGain;
                
                // SVF: nur Gain-Update (kein tan() — ~10x schneller)
                svfLeft.updateGainOnly(effectiveGain);
                data[i] = svfLeft.processSample(data[i]);
            }
        }
        else
        {
            // Standard Processing
            for (int stage = 0; stage < numCascadeStages; ++stage)
            {
                filtersLeft[stage].processBlock(data, numSamples);
            }
        }
        return;
    }

    auto* leftData = buffer.getWritePointer(0);
    auto* rightData = buffer.getWritePointer(1);

    // Dynamic EQ Processing — SVF-basiert (modulationsstabil)
    if (dynamicMode)
    {
        // Nur bei Parameteränderung (Frequenz/Q/Typ) teure tan()-Berechnung
        if (svfLeft.needsFullUpdate(filterType, frequency, q))
        {
            svfLeft.setParameters(filterType, frequency, gain, q);
            svfRight.setParameters(filterType, frequency, gain, q);
        }
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Envelope Followers für beide Kanäle
            float envL = processEnvelope(std::abs(leftData[i]), envelopeLeft);
            float envR = processEnvelope(std::abs(rightData[i]), envelopeRight);
            
            // Nutze Maximum für linked Processing
            float maxEnv = juce::jmax(envL, envR);
            float dynamicGain = calculateDynamicGain(maxEnv);
            float effectiveGain = gain * dynamicGain;
            
            // SVF: nur Gain-Update pro Sample (kein tan() — ~10x schneller)
            svfLeft.updateGainOnly(effectiveGain);
            svfRight.updateGainOnly(effectiveGain);
            
            leftData[i] = svfLeft.processSample(leftData[i]);
            rightData[i] = svfRight.processSample(rightData[i]);
        }
        return;
    }

    switch (channelMode)
    {
        case ParameterIDs::ChannelMode::Stereo:
            // Beide Kanäle gleich verarbeiten
            for (int stage = 0; stage < numCascadeStages; ++stage)
            {
                filtersLeft[stage].processBlock(leftData, numSamples);
                filtersRight[stage].processBlock(rightData, numSamples);
            }
            break;

        case ParameterIDs::ChannelMode::Left:
            // Nur linken Kanal verarbeiten
            for (int stage = 0; stage < numCascadeStages; ++stage)
            {
                filtersLeft[stage].processBlock(leftData, numSamples);
            }
            break;

        case ParameterIDs::ChannelMode::Right:
            // Nur rechten Kanal verarbeiten
            for (int stage = 0; stage < numCascadeStages; ++stage)
            {
                filtersRight[stage].processBlock(rightData, numSamples);
            }
            break;

        case ParameterIDs::ChannelMode::Mid:
        case ParameterIDs::ChannelMode::Side:
            // OPTIMIERT: Blockweise M/S-Verarbeitung statt Sample-für-Sample
            // Schritt 1: Encode gesamten Block zu M/S (Mid in leftData, Side in rightData)
            for (int i = 0; i < numSamples; ++i)
            {
                const float mid = (leftData[i] + rightData[i]) * 0.5f;
                const float side = (leftData[i] - rightData[i]) * 0.5f;
                leftData[i] = mid;
                rightData[i] = side;
            }
            
            // Schritt 2: Blockweise Verarbeitung (nur Mid oder Side)
            if (channelMode == ParameterIDs::ChannelMode::Mid)
            {
                for (int stage = 0; stage < numCascadeStages; ++stage)
                {
                    filtersLeft[stage].processBlock(leftData, numSamples);
                }
            }
            else // Side
            {
                for (int stage = 0; stage < numCascadeStages; ++stage)
                {
                    filtersRight[stage].processBlock(rightData, numSamples);
                }
            }
            
            // Schritt 3: Decode gesamten Block zurück zu L/R
            for (int i = 0; i < numSamples; ++i)
            {
                const float left = leftData[i] + rightData[i];
                const float right = leftData[i] - rightData[i];
                leftData[i] = left;
                rightData[i] = right;
            }
            break;

        default:
            break;
    }
}

float EQBand::getMagnitudeForFrequency(float freq) const
{
    if (bypassed || !active)
        return 0.0f;

    // Magnitude aller Kaskadenstufen addieren (in dB)
    float totalMagnitude = 0.0f;
    for (int stage = 0; stage < numCascadeStages; ++stage)
    {
        totalMagnitude += filtersLeft[stage].getMagnitudeForFrequency(freq);
    }
    return totalMagnitude;
}

void EQBand::encodeToMidSide(float& left, float& right)
{
    const float mid = (left + right) * 0.5f;
    const float side = (left - right) * 0.5f;
    left = mid;
    right = side;
}

void EQBand::decodeFromMidSide(float& mid, float& side)
{
    const float left = mid + side;
    const float right = mid - side;
    mid = left;
    side = right;
}

// OPTIMIERUNG: Envelope-Koeffizienten einmalig berechnen (nicht pro Sample!)
void EQBand::updateEnvelopeCoefficients()
{
    if (currentSampleRate <= 0.0) return;
    
    float attackTimeSamples = attack * 0.001f * static_cast<float>(currentSampleRate);
    float releaseTimeSamples = release * 0.001f * static_cast<float>(currentSampleRate);
    
    cachedAttackCoeff = std::exp(-1.0f / std::max(1.0f, attackTimeSamples));
    cachedReleaseCoeff = std::exp(-1.0f / std::max(1.0f, releaseTimeSamples));
}

// Dynamic EQ Processing Functions (OPTIMIZED)
float EQBand::processEnvelope(float input, float& envelope)
{
    // RMS-ähnliche Envelope Follower
    float inputSquared = input * input;
    
    // OPTIMIERUNG: Verwende gecachte Koeffizienten statt exp() pro Sample
    // Envelope Update
    if (inputSquared > envelope)
    {
        // Attack
        envelope = cachedAttackCoeff * envelope + (1.0f - cachedAttackCoeff) * inputSquared;
    }
    else
    {
        // Release
        envelope = cachedReleaseCoeff * envelope + (1.0f - cachedReleaseCoeff) * inputSquared;
    }
    
    // OPTIMIERUNG: Schnellere dB-Approximation für Envelope
    // log10(x) ≈ (x-1)/(x+1) * 2/ln(10) für x nahe 1, aber wir brauchen genaueren Wert
    float envelopeDB = 10.0f * std::log10(envelope + 1e-10f);
    return envelopeDB;
}

float EQBand::calculateDynamicGain(float envelopeDB)
{
    // Berechne Gain Reduction basierend auf Threshold und Ratio
    float overThreshold = envelopeDB - threshold;
    
    if (overThreshold > 0.0f)
    {
        // Über Threshold: Gain Reduction anwenden
        float gainReductionDB = overThreshold * (1.0f - 1.0f / ratio);
        dynamicGainReduction = gainReductionDB;
        
        // Konvertiere zu linearem Skalierungsfaktor (0.0 - 1.0)
        // FIX: Clamp auf [0, 1] — ohne Clamp konnte der Wert negativ werden
        // und die EQ-Band-Polarität umkehren wenn gain klein war
        float absGain = std::abs(gain) + 1e-6f;
        float dynamicScale = 1.0f - (gainReductionDB / absGain);
        return juce::jlimit(0.0f, 1.0f, dynamicScale);
    }
    
    dynamicGainReduction = 0.0f;
    return 1.0f;  // Kein Effekt unter Threshold
}
