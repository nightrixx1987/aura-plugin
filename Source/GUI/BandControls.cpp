#include "BandControls.h"

BandControls::BandControls()
{
    // Frequenz-Slider
    setupSlider(frequencySlider, frequencyLabel, "Frequency");
    frequencySlider.setRange(20.0, 20000.0);
    frequencySlider.setSkewFactorFromMidPoint(1000.0);
    // Kein Suffix - APVTS Parameter hat bereits stringFromValueFunction mit "Hz"/"kHz"
    frequencySlider.setNumDecimalPlacesToDisplay(0);
    frequencySlider.onValueChange = [this]() {
        notifyChange("frequency", static_cast<float>(frequencySlider.getValue()));
    };

    // Gain-Slider
    setupSlider(gainSlider, gainLabel, "Gain");
    gainSlider.setRange(-30.0, 30.0);
    // Kein Suffix - APVTS Parameter hat bereits stringFromValueFunction mit "dB"
    gainSlider.setNumDecimalPlacesToDisplay(1);
    gainSlider.onValueChange = [this]() {
        notifyChange("gain", static_cast<float>(gainSlider.getValue()));
    };

    // Q-Slider
    setupSlider(qSlider, qLabel, "Q");
    qSlider.setRange(0.1, 18.0);
    qSlider.setSkewFactorFromMidPoint(1.0);
    qSlider.setNumDecimalPlacesToDisplay(2);
    qSlider.onValueChange = [this]() {
        notifyChange("q", static_cast<float>(qSlider.getValue()));
    };

    // Bypass-Button
    bypassButton.setButtonText("Bypass");
    bypassButton.onClick = [this]() {
        notifyChange("bypass", bypassButton.getToggleState() ? 1.0f : 0.0f);
    };
    addAndMakeVisible(bypassButton);

    // Band-Label
    bandLabel.setFont(juce::Font(juce::FontOptions(17.0f).withStyle("Bold")));
    bandLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bandLabel);

    // Initial keine Auswahl
    clearSelection();
}

void BandControls::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void BandControls::paint(juce::Graphics& g)
{
    g.fillAll(CustomLookAndFeel::getBackgroundMid());
    
    // Rahmen
    g.setColour(CustomLookAndFeel::getBackgroundLight());
    g.drawRect(getLocalBounds(), 1);

    if (currentBandIndex < 0)
    {
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));
        g.setFont(14.0f);
        g.drawText("Select a band or double-click to create one",
                   getLocalBounds(), juce::Justification::centred);
    }
}

void BandControls::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    
    // Band-Label oben
    bandLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(4);

    // Slider in einer Reihe mit Bypass rechts
    auto sliderArea = bounds.removeFromTop(105);
    const int bypassWidth = 90;
    
    // Bypass-Button rechts
    auto bypassArea = sliderArea.removeFromRight(bypassWidth).reduced(5);
    bypassArea.removeFromTop(25); // Align mit Slider-Mitte
    bypassButton.setBounds(bypassArea.removeFromTop(35));
    
    sliderArea.removeFromRight(8); // Abstand
    
    // Drei Slider links
    const int sliderWidth = sliderArea.getWidth() / 3;

    auto freqArea = sliderArea.removeFromLeft(sliderWidth);
    frequencyLabel.setBounds(freqArea.removeFromTop(20));
    frequencySlider.setBounds(freqArea);

    auto gainArea = sliderArea.removeFromLeft(sliderWidth);
    gainLabel.setBounds(gainArea.removeFromTop(20));
    gainSlider.setBounds(gainArea);

    auto qArea = sliderArea;
    qLabel.setBounds(qArea.removeFromTop(20));
    qSlider.setBounds(qArea);
}

void BandControls::setBandData(int bandIndex, float frequency, float gain, float q,
                                ParameterIDs::FilterType /*type*/, ParameterIDs::ChannelMode /*channel*/,
                                bool bypassed)
{
    currentBandIndex = bandIndex;

    // Controls aktivieren
    frequencySlider.setEnabled(true);
    gainSlider.setEnabled(true);
    qSlider.setEnabled(true);
    bypassButton.setEnabled(true);

    // Werte setzen (ohne Notifications zu triggern)
    frequencySlider.setValue(frequency, juce::dontSendNotification);
    gainSlider.setValue(gain, juce::dontSendNotification);
    qSlider.setValue(q, juce::dontSendNotification);
    bypassButton.setToggleState(bypassed, juce::dontSendNotification);

    // Band-Label mit Farbe
    juce::Colour bandColour = CustomLookAndFeel::getBandColor(bandIndex);
    bandLabel.setText("Band " + juce::String(bandIndex + 1), juce::dontSendNotification);
    bandLabel.setColour(juce::Label::textColourId, bandColour);

    repaint();
}

void BandControls::clearSelection()
{
    currentBandIndex = -1;

    frequencySlider.setEnabled(false);
    gainSlider.setEnabled(false);
    qSlider.setEnabled(false);
    bypassButton.setEnabled(false);

    bandLabel.setText("", juce::dontSendNotification);

    repaint();
}

void BandControls::setAttachments(juce::AudioProcessorValueTreeState& apvts, int bandIndex)
{
    clearAttachments();

    freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::getBandFreqID(bandIndex), frequencySlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::getBandGainID(bandIndex), gainSlider);
    qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::getBandQID(bandIndex), qSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ParameterIDs::getBandBypassID(bandIndex), bypassButton);
}

void BandControls::clearAttachments()
{
    freqAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    bypassAttachment.reset();
}

void BandControls::addListener(Listener* listener)
{
    listeners.push_back(listener);
}

void BandControls::removeListener(Listener* listener)
{
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

void BandControls::notifyChange(const juce::String& paramName, float value)
{
    if (currentBandIndex >= 0)
    {
        for (auto* listener : listeners)
        {
            listener->bandControlChanged(currentBandIndex, paramName, value);
        }
    }
}
