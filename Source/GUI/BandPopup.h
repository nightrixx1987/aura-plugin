#pragma once

#include <JuceHeader.h>
#include "CustomLookAndFeel.h"
#include "../Parameters/ParameterIDs.h"
#include "../DSP/EQProcessor.h"

/**
 * BandPopup: Schwebendes Einstellfenster für EQ-Band (FabFilter-Style)
 * Erscheint beim Klick auf einen Band-Point
 */
class BandPopup : public juce::Component
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void bandPopupValueChanged(int bandIndex, const juce::String& parameterName, float value) = 0;
        virtual void bandPopupDeleteRequested(int bandIndex) = 0;
        virtual void bandPopupBypassChanged(int bandIndex, bool bypassed) = 0;
    };

    BandPopup();
    ~BandPopup() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Band-Daten setzen
    void setBandData(int bandIndex, float frequency, float gain, 
                     ParameterIDs::FilterType type, ParameterIDs::ChannelMode channel,
                     int slope, bool bypassed);
    
    // Position am Point setzen
    void showAtPoint(juce::Point<int> position, juce::Component* parent);
    
    // Listener
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // APVTS-Attachments
    void setAttachments(juce::AudioProcessorValueTreeState& apvts, int bandIndex);
    void clearAttachments();
    
    // EQ-Processor für Auto-Threshold
    void setEQProcessor(EQProcessor* processor) { eqProcessor = processor; }

private:
    int currentBandIndex = -1;
    ParameterIDs::FilterType currentFilterType = ParameterIDs::FilterType::Bell;
    std::vector<Listener*> listeners;
    EQProcessor* eqProcessor = nullptr;

    // Controls
    juce::Label titleLabel;
    juce::ComboBox typeCombo;
    juce::ComboBox channelCombo;
    juce::ComboBox slopeCombo;
    juce::ToggleButton bypassButton;
    juce::TextButton deleteButton;

    juce::Label typeLabel;
    juce::Label channelLabel;
    juce::Label slopeLabel;
    juce::Label slopeDisplayLabel;

    // Dynamic EQ Controls
    juce::ToggleButton dynEnabledButton;
    juce::Slider dynThresholdSlider;
    juce::Slider dynRatioSlider;
    juce::Slider dynAttackSlider;
    juce::Slider dynReleaseSlider;
    juce::Label dynLabel;
    juce::Label dynThresholdLabel;
    juce::Label dynRatioLabel;
    juce::Label dynAttackLabel;
    juce::Label dynReleaseLabel;
    juce::TextButton autoThresholdButton;  // Auto-Threshold Button
    
    // NEU: Solo Button
    juce::ToggleButton soloButton;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Dynamic EQ Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynEnabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynThresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynReleaseAttachment;
    
    // NEU: Solo Attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;

    void notifyChange(const juce::String& paramName, float value);
    void updateSlopeVisibility();
    void updateSlopeDisplay();
    void updateDynControlsVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPopup)
};
