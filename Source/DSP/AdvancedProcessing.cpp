#include "AdvancedProcessing.h"

//==============================================================================
// MidSideProcessor
//==============================================================================

MidSideProcessor::MidSideProcessor()
{
}

void MidSideProcessor::setMode(ProcessingMode newMode)
{
    mode = newMode;
}

void MidSideProcessor::encodeToMidSide(float& left, float& right) const
{
    float mid = (left + right) * 0.5f;
    float side = (left - right) * 0.5f;
    left = mid;
    right = side;
}

void MidSideProcessor::decodeFromMidSide(float& mid, float& side) const
{
    float left = mid + side;
    float right = mid - side;
    mid = left;
    side = right;
}

void MidSideProcessor::processMidSide(juce::AudioBuffer<float>& buffer)
{
    if (mode == ProcessingMode::Stereo)
        return;  // Keine Verarbeitung im Stereo-Modus

    auto numSamples = buffer.getNumSamples();
    auto* leftData = buffer.getWritePointer(0);
    auto* rightData = buffer.getWritePointer(1, 0);

    if (rightData == nullptr)
        return;  // Mono, keine Mid/Side nötig

    for (int i = 0; i < numSamples; ++i)
    {
        float mid = (leftData[i] + rightData[i]) * 0.5f;
        float side = (leftData[i] - rightData[i]) * 0.5f;

        if (mode == ProcessingMode::MidOnly)
        {
            side = 0.0f;
        }
        else if (mode == ProcessingMode::SideOnly)
        {
            mid = 0.0f;
        }

        leftData[i] = mid + side;
        rightData[i] = mid - side;
    }
}

void MidSideProcessor::prepare(double newSampleRate, int /*blockSize*/)
{
    this->sampleRate = newSampleRate;
}

void MidSideProcessor::reset()
{
}

//==============================================================================
// SpectralShaper
//==============================================================================

SpectralShaper::SpectralShaper()
{
    for (auto& band : bands)
    {
        band.active = false;
    }
}

void SpectralShaper::setBand(int bandIndex, const SpectralBand& band)
{
    if (bandIndex >= 0 && bandIndex < NUM_SPECTRAL_BANDS)
    {
        bands[bandIndex] = band;
    }
}

const SpectralShaper::SpectralBand& SpectralShaper::getBand(int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < NUM_SPECTRAL_BANDS)
        return bands[bandIndex];
    return bands[0];
}

void SpectralShaper::processBock(juce::AudioBuffer<float>& /*buffer*/)
{
    // Simplified spektral processing
    // Nur aktive Bänder verarbeiten
    for (const auto& band : bands)
    {
        if (band.active && std::abs(band.gain) > 0.01f)
        {
            // TODO: Implementiere spektrale Filter
        }
    }
}

void SpectralShaper::prepare(double newSampleRate, int /*blockSize*/)
{
    this->sampleRate = newSampleRate;
}

//==============================================================================
// TransientPreserver
//==============================================================================

TransientPreserver::TransientPreserver()
{
}

void TransientPreserver::setAmount(float amount)
{
    preserveAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void TransientPreserver::process(juce::AudioBuffer<float>& buffer)
{
    if (preserveAmount < 0.01f)
        return;  // Aus

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float transientStrength = detectTransient(data[i]);
            if (transientStrength > transientThreshold && preserveAmount > 0.5f)
            {
                // Verstärke Transienten leicht
                data[i] *= (1.0f + preserveAmount * 0.1f);
            }
            lastSample = data[i];
        }
    }
}

void TransientPreserver::prepare(double newSampleRate, int /*blockSize*/)
{
    this->sampleRate = newSampleRate;
}

void TransientPreserver::reset()
{
    lastSample = 0.0f;
}

float TransientPreserver::detectTransient(float sample)
{
    return std::abs(sample - lastSample);
}

//==============================================================================
// ParallelProcessor
//==============================================================================

ParallelProcessor::ParallelProcessor()
{
}

void ParallelProcessor::setWetDryMix(float amount)
{
    wetAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void ParallelProcessor::processParallel(juce::AudioBuffer<float>& dryBuffer,
                                        const juce::AudioBuffer<float>& wetBuffer)
{
    auto numSamples = dryBuffer.getNumSamples();

    for (int ch = 0; ch < dryBuffer.getNumChannels(); ++ch)
    {
        auto* dryData = dryBuffer.getWritePointer(ch);
        const auto* wetData = wetBuffer.getReadPointer(ch);

        if (wetData != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                dryData[i] = (dryData[i] * (1.0f - wetAmount)) + (wetData[i] * wetAmount);
            }
        }
    }
}

void ParallelProcessor::prepare(double newSampleRate, int /*blockSize*/)
{
    this->sampleRate = newSampleRate;
}

//==============================================================================
// StereoLinker
//==============================================================================

void StereoLinker::setLinkMode(LinkMode mode)
{
    linkMode = mode;
}

void StereoLinker::synchronizeParameters(float& leftFreq, float& rightFreq,
                                        float& leftGain, float& rightGain)
{
    if (linkMode == LinkMode::Unlinked)
        return;  // Keine Synchronisation

    if (linkMode == LinkMode::Linked)
    {
        // L und R werden gleich gesetzt (zu L)
        rightFreq = leftFreq;
        rightGain = leftGain;
    }
    else if (linkMode == LinkMode::Symmetrical)
    {
        // Mittelwert berechnen
        float avgFreq = (leftFreq + rightFreq) * 0.5f;
        float avgGain = (leftGain + rightGain) * 0.5f;
        leftFreq = avgFreq;
        rightFreq = avgFreq;
        leftGain = avgGain;
        rightGain = avgGain;
    }
}
