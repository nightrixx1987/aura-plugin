#pragma once

#include <JuceHeader.h>
#include "CustomLookAndFeel.h"
#include "../Parameters/ParameterIDs.h"

/**
 * BandControls: Detaillierte Steuerung für das ausgewählte EQ-Band.
 */
class BandControls : public juce::Component
{
public:
    // Callback für Änderungen
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void bandControlChanged(int bandIndex, const juce::String& parameterName, float value) = 0;
    };

    BandControls();
    ~BandControls() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Band-Daten setzen
    void setBandData(int bandIndex, float frequency, float gain, float q,
                     ParameterIDs::FilterType type, ParameterIDs::ChannelMode channel,
                     bool bypassed);
    
    // Kein Band ausgewählt
    void clearSelection();

    // Getter für aktuelles Band
    int getCurrentBandIndex() const { return currentBandIndex; }

    // Listener
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // APVTS-Attachments setzen
    void setAttachments(juce::AudioProcessorValueTreeState& apvts, int bandIndex);
    void clearAttachments();

private:
    int currentBandIndex = -1;
    std::vector<Listener*> listeners;

    // Controls
    juce::Slider frequencySlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::ToggleButton bypassButton;

    // Labels
    juce::Label frequencyLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Label bandLabel;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText);
    void notifyChange(const juce::String& paramName, float value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControls)
};
