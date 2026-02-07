#include "EQProcessor.h"

EQProcessor::EQProcessor()
{
    // Standard-Konfiguration für die Bänder
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        bands[i].setParameters(
            ParameterIDs::DEFAULT_FREQUENCIES[i],
            ParameterIDs::DEFAULT_GAIN,
            ParameterIDs::DEFAULT_Q,
            ParameterIDs::DEFAULT_TYPES[i]
        );
        bands[i].setActive(false);  // Bänder starten inaktiv
    }
}

void EQProcessor::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    
    for (auto& band : bands)
    {
        band.prepare(sampleRate, samplesPerBlock);
    }
}

void EQProcessor::reset()
{
    for (auto& band : bands)
    {
        band.reset();
    }
}

void EQProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    // Input Gain anwenden
    if (std::abs(inputGainLinear - 1.0f) > 0.0001f)
    {
        buffer.applyGain(inputGainLinear);
    }
    
    // Alle aktiven Bänder anwenden
    for (auto& band : bands)
    {
        if (band.isActive() && !band.isBypassed())
        {
            band.processBlock(buffer);
        }
    }

    // Output Gain anwenden
    if (std::abs(outputGainLinear - 1.0f) > 0.0001f)
    {
        buffer.applyGain(outputGainLinear);
    }
}

EQBand& EQProcessor::getBand(int index)
{
    jassert(index >= 0 && index < ParameterIDs::MAX_BANDS);
    return bands[static_cast<size_t>(index)];
}

const EQBand& EQProcessor::getBand(int index) const
{
    jassert(index >= 0 && index < ParameterIDs::MAX_BANDS);
    return bands[static_cast<size_t>(index)];
}

float EQProcessor::getTotalMagnitudeForFrequency(float frequency) const
{
    float totalMagnitude = 0.0f;
    
    for (const auto& band : bands)
    {
        if (band.isActive() && !band.isBypassed())
        {
            totalMagnitude += band.getMagnitudeForFrequency(frequency);
        }
    }
    
    // Output Gain hinzufügen
    totalMagnitude += outputGainDB;
    
    return totalMagnitude;
}

void EQProcessor::getMagnitudeResponse(const float* frequencies, float* magnitudes, int numPoints) const
{
    for (int i = 0; i < numPoints; ++i)
    {
        magnitudes[i] = getTotalMagnitudeForFrequency(frequencies[i]);
    }
}

void EQProcessor::setOutputGain(float gainDB)
{
    outputGainDB = gainDB;
    outputGainLinear = juce::Decibels::decibelsToGain(gainDB);
}

void EQProcessor::setLinearPhaseEnabled(bool enabled)
{
    linearPhaseEnabled = enabled;
    // TODO: Implementiere lineare Phase Filterung (Zero-Latency oder Latenz-Kompensation)
}

void EQProcessor::setInputGain(float gainDB)
{
    inputGainDB = gainDB;
    inputGainLinear = juce::Decibels::decibelsToGain(gainDB);
}

void EQProcessor::copyBandSettings(int sourceBandIndex)
{
    if (sourceBandIndex >= 0 && sourceBandIndex < ParameterIDs::MAX_BANDS)
    {
        const auto& band = bands[sourceBandIndex];
        copiedBandData.frequency = band.getFrequency();
        copiedBandData.gain = band.getGain();
        copiedBandData.q = band.getQ();
        copiedBandData.type = band.getType();
    }
}

void EQProcessor::pasteBandSettings(int targetBandIndex)
{
    if (targetBandIndex >= 0 && targetBandIndex < ParameterIDs::MAX_BANDS)
    {
        auto& band = bands[targetBandIndex];
        band.setParameters(
            copiedBandData.frequency,
            copiedBandData.gain,
            copiedBandData.q,
            copiedBandData.type
        );
        band.setActive(true);
    }
}

