#include "SpectrumGrabTool.h"
#include <cmath>

SpectrumGrabTool::SpectrumGrabTool(EQProcessor& processor)
    : eqProcessor(processor)
{
    setInterceptsMouseClicks(false, false);  // Initially don't intercept
}

void SpectrumGrabTool::paint(juce::Graphics& g)
{
    if (!grabMode)
        return;
    
    // Zeichne Grab-Cursor
    g.setColour(grabCursorColour);
    
    // Fadenkreuz
    float x = mousePosition.x;
    float y = mousePosition.y;
    
    g.drawLine(x - cursorSize, y, x + cursorSize, y, 2.0f);
    g.drawLine(x, y - cursorSize, x, y + cursorSize, 2.0f);
    
    // Kreis um Cursor
    g.drawEllipse(x - cursorSize * 0.5f, y - cursorSize * 0.5f, 
                  cursorSize, cursorSize, 2.0f);
    
    // Info-Text
    if (mousePosition.x > 0 && mousePosition.y > 0)
    {
        float freq = xToFrequency(mousePosition.x);
        juce::String freqText = freq < 1000.0f 
            ? juce::String(freq, 0) + " Hz"
            : juce::String(freq / 1000.0f, 1) + " kHz";
        
        g.setFont(12.0f);
        g.drawText(freqText, 
                   static_cast<int>(x - 30), 
                   static_cast<int>(y + 15), 
                   60, 20, 
                   juce::Justification::centred);
    }
    
    // Hinweis-Text wenn kein Hover
    if (mousePosition.x == 0 && mousePosition.y == 0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(14.0f);
        g.drawText("SPECTRUM GRAB MODE - Klicken Sie auf Peaks im Spektrum",
                   getLocalBounds(), juce::Justification::centred);
    }
}

void SpectrumGrabTool::mouseDown(const juce::MouseEvent& event)
{
    if (!grabMode || spectrumMagnitudes.empty())
        return;
    
    // Analysiere Peak an Mausposition
    PeakInfo peakInfo = analyzePeakAtPosition(event.position);
    
    // Finde freies Band
    int bandIndex = findInactiveBand();
    if (bandIndex == -1)
        return;  // Alle Baender aktiv
    
    // Filtertyp bestimmen
    int filterType = static_cast<int>(ParameterIDs::FilterType::Bell);
    if (autoDetectFilterType)
    {
        if (peakInfo.frequency < 100.0f)
        {
            filterType = peakInfo.isBoost 
                ? static_cast<int>(ParameterIDs::FilterType::LowShelf)
                : static_cast<int>(ParameterIDs::FilterType::HighCut);
        }
        else if (peakInfo.frequency > 10000.0f)
        {
            filterType = peakInfo.isBoost
                ? static_cast<int>(ParameterIDs::FilterType::HighShelf)
                : static_cast<int>(ParameterIDs::FilterType::LowCut);
        }
    }
    
    float gain = peakInfo.isBoost ? defaultBoostGain : defaultCutGain;
    
    // Ueber Callback im Editor APVTS-Parameter setzen (UI-Sync!)
    if (onBandGrabbed)
    {
        onBandGrabbed(bandIndex, peakInfo.frequency, gain, peakInfo.qFactor, filterType);
    }
    
    repaint();
}

void SpectrumGrabTool::mouseMove(const juce::MouseEvent& event)
{
    if (!grabMode)
        return;
    
    mousePosition = event.position;
    repaint();
}

void SpectrumGrabTool::setGrabMode(bool enabled)
{
    grabMode = enabled;
    
    // Only intercept mouse clicks and be visible when grab mode is active
    setInterceptsMouseClicks(enabled, false);
    setVisible(enabled);
    
    if (!enabled)
    {
        mousePosition = juce::Point<float>(0, 0);
    }
    
    setMouseCursor(enabled ? juce::MouseCursor::CrosshairCursor 
                          : juce::MouseCursor::NormalCursor);
    repaint();
}

void SpectrumGrabTool::updateSpectrumData(const std::vector<float>& magnitudes,
                                         float minFreq, float maxFreq)
{
    spectrumMagnitudes = magnitudes;
    spectrumMinFreq = minFreq;
    spectrumMaxFreq = maxFreq;
}

void SpectrumGrabTool::setAutoDetectFilterType(bool shouldAuto)
{
    autoDetectFilterType = shouldAuto;
}

void SpectrumGrabTool::setDefaultBoostGain(float gain)
{
    defaultBoostGain = juce::jlimit(-24.0f, 24.0f, gain);
}

void SpectrumGrabTool::setDefaultCutGain(float gain)
{
    defaultCutGain = juce::jlimit(-24.0f, 24.0f, gain);
}

void SpectrumGrabTool::setIntelligentQMode(bool enabled)
{
    intelligentQMode = enabled;
}

float SpectrumGrabTool::frequencyToX(float frequency) const
{
    if (spectrumMinFreq >= spectrumMaxFreq)
        return 0.0f;
    
    float normFreq = std::log(frequency / spectrumMinFreq) / 
                    std::log(spectrumMaxFreq / spectrumMinFreq);
    return normFreq * getWidth();
}

float SpectrumGrabTool::xToFrequency(float x) const
{
    if (getWidth() <= 0)
        return 1000.0f;
    
    float normX = x / getWidth();
    return spectrumMinFreq * std::pow(spectrumMaxFreq / spectrumMinFreq, normX);
}

SpectrumGrabTool::PeakInfo SpectrumGrabTool::analyzePeakAtPosition(const juce::Point<float>& pos)
{
    PeakInfo info;
    info.frequency = xToFrequency(pos.x);
    
    // Finde nächsten Peak im Spektrum
    int peakIndex = findNearestPeakIndex(info.frequency);
    
    if (peakIndex >= 0 && peakIndex < static_cast<int>(spectrumMagnitudes.size()))
    {
        info.magnitude = spectrumMagnitudes[peakIndex];
        
        // Bestimme ob Boost oder Cut basierend auf Magnitude
        // Höhere Magnitude = wahrscheinlich zu laut = Cut
        info.isBoost = info.magnitude < 0.5f;  // Threshold kann angepasst werden
        
        // Q-Faktor aus Peak-Breite berechnen
        if (intelligentQMode)
        {
            info.qFactor = calculateQFromPeakWidth(peakIndex);
        }
        else
        {
            info.qFactor = 0.71f;  // Standard Q
        }
    }
    else
    {
        // Fallback
        info.magnitude = 0.5f;
        info.qFactor = 0.71f;
        info.isBoost = true;
    }
    
    return info;
}

int SpectrumGrabTool::findNearestPeakIndex(float targetFreq)
{
    if (spectrumMagnitudes.empty())
        return -1;
    
    // Konvertiere Zielfrequenz zu Index
    float normFreq = std::log(targetFreq / spectrumMinFreq) / 
                    std::log(spectrumMaxFreq / spectrumMinFreq);
    int targetIndex = static_cast<int>(normFreq * spectrumMagnitudes.size());
    targetIndex = juce::jlimit(0, static_cast<int>(spectrumMagnitudes.size()) - 1, targetIndex);
    
    // Suche lokales Maximum in der Nähe
    int searchRadius = 10;
    int bestIndex = targetIndex;
    float maxMag = spectrumMagnitudes[targetIndex];
    
    for (int i = -searchRadius; i <= searchRadius; ++i)
    {
        int idx = targetIndex + i;
        if (idx >= 0 && idx < static_cast<int>(spectrumMagnitudes.size()))
        {
            if (spectrumMagnitudes[idx] > maxMag)
            {
                maxMag = spectrumMagnitudes[idx];
                bestIndex = idx;
            }
        }
    }
    
    return bestIndex;
}

float SpectrumGrabTool::calculateQFromPeakWidth(int peakIndex)
{
    if (peakIndex < 2 || peakIndex >= static_cast<int>(spectrumMagnitudes.size()) - 2)
        return 0.71f;
    
    float peakMag = spectrumMagnitudes[peakIndex];
    float threshold = peakMag * 0.707f;  // -3dB Punkt
    
    // Finde Breite bei -3dB
    int leftIndex = peakIndex;
    while (leftIndex > 0 && spectrumMagnitudes[leftIndex] > threshold)
        --leftIndex;
    
    int rightIndex = peakIndex;
    while (rightIndex < static_cast<int>(spectrumMagnitudes.size()) - 1 && 
           spectrumMagnitudes[rightIndex] > threshold)
        ++rightIndex;
    
    // Berechne Frequenz-Verhältnis
    float leftFreq = spectrumMinFreq * std::pow(spectrumMaxFreq / spectrumMinFreq,
        static_cast<float>(leftIndex) / spectrumMagnitudes.size());
    float rightFreq = spectrumMinFreq * std::pow(spectrumMaxFreq / spectrumMinFreq,
        static_cast<float>(rightIndex) / spectrumMagnitudes.size());
    float centerFreq = spectrumMinFreq * std::pow(spectrumMaxFreq / spectrumMinFreq,
        static_cast<float>(peakIndex) / spectrumMagnitudes.size());
    
    // Q = Centerfreq / Bandwidth
    float bandwidth = rightFreq - leftFreq;
    float q = bandwidth > 0.0f ? centerFreq / bandwidth : 0.71f;
    
    // Begrenze Q auf sinnvolle Werte
    return juce::jlimit(0.1f, 10.0f, q);
}

int SpectrumGrabTool::findInactiveBand()
{
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        if (!eqProcessor.getBand(i).isActive())
            return i;
    }
    return -1;  // Alle Bänder aktiv
}
