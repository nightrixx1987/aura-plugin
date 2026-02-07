#include "BandPopup.h"

BandPopup::BandPopup()
{
    // Titel
    titleLabel.setFont(juce::Font(juce::FontOptions(16.0f).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getAccentColor());
    addAndMakeVisible(titleLabel);

    // Type Combo
    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    typeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(typeLabel);

    typeCombo.addItemList(ParameterIDs::getFilterTypeNames(), 1);
    typeCombo.setJustificationType(juce::Justification::centred);
    typeCombo.setTooltip("Filtertyp waehlen\nBell = Glockenfoermig (Standard-EQ)\nLow/High Shelf = Klangregler fuer Tiefen/Hoehen\nLow/High Pass = Frequenzen unterhalb/oberhalb abschneiden\nNotch = Schmaler Kerbfilter\nBandPass = Laesst nur einen Bereich durch\nAllPass = Aendert nur die Phase\nFlat Tilt = Kippt das gesamte Spektrum");
    typeCombo.onChange = [this]() {
        currentFilterType = static_cast<ParameterIDs::FilterType>(typeCombo.getSelectedId() - 1);
        updateSlopeVisibility();
        notifyChange("type", static_cast<float>(typeCombo.getSelectedId() - 1));
    };
    addAndMakeVisible(typeCombo);

    // Channel Combo
    channelLabel.setText("Channel", juce::dontSendNotification);
    channelLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    channelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(channelLabel);

    channelCombo.addItemList(ParameterIDs::getChannelModeNames(), 1);
    channelCombo.setJustificationType(juce::Justification::centred);
    channelCombo.setTooltip("Kanal-Zuordnung\nStereo = Beide Kanaele gleichzeitig bearbeiten\nLeft/Right = Nur linken/rechten Kanal bearbeiten\nMid = Nur Mono-Anteil (Mitte) bearbeiten\nSide = Nur Stereo-Anteil (Seiten) bearbeiten");
    channelCombo.onChange = [this]() {
        notifyChange("channel", static_cast<float>(channelCombo.getSelectedId() - 1));
    };
    addAndMakeVisible(channelCombo);

    // Slope Combo
    slopeLabel.setText("Slope", juce::dontSendNotification);
    slopeLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    slopeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(slopeLabel);

    slopeCombo.addItem("6 dB/Oct", 6);
    slopeCombo.addItem("12 dB/Oct", 12);
    slopeCombo.addItem("18 dB/Oct", 18);
    slopeCombo.addItem("24 dB/Oct", 24);
    slopeCombo.addItem("36 dB/Oct", 36);
    slopeCombo.addItem("48 dB/Oct", 48);
    slopeCombo.addItem("72 dB/Oct", 72);
    slopeCombo.addItem("96 dB/Oct", 96);
    slopeCombo.setJustificationType(juce::Justification::centred);
    slopeCombo.setTooltip("Filtersteilheit (dB/Oktave)\nHoehere Werte = steilerer Filterabfall.\n6-12 dB/Oct = Sanft und musikalisch\n24-48 dB/Oct = Praezise und steil\n72-96 dB/Oct = Sehr steil, fast wie ein Brick-Wall");
    slopeCombo.onChange = [this]() {
        updateSlopeDisplay();
        notifyChange("slope", static_cast<float>(slopeCombo.getSelectedId()));
    };
    addAndMakeVisible(slopeCombo);

    slopeDisplayLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    slopeDisplayLabel.setJustificationType(juce::Justification::centred);
    slopeDisplayLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getAccentColor());
    addAndMakeVisible(slopeDisplayLabel);

    // Bypass Button
    bypassButton.setButtonText("Bypass");
    bypassButton.setTooltip("Band Bypass\nDeaktiviert dieses EQ-Band voruebergehend,\nohne die Einstellungen zu verlieren.");
    bypassButton.onClick = [this]() {
        if (currentBandIndex >= 0)
        {
            for (auto* listener : listeners)
                listener->bandPopupBypassChanged(currentBandIndex, bypassButton.getToggleState());
        }
    };
    addAndMakeVisible(bypassButton);

    // Delete Button
    deleteButton.setButtonText("Delete Band");
    deleteButton.setTooltip("Band loeschen\nEntfernt dieses EQ-Band komplett.\nAlle Einstellungen gehen verloren.");
    deleteButton.onClick = [this]() {
        if (currentBandIndex >= 0)
        {
            for (auto* listener : listeners)
                listener->bandPopupDeleteRequested(currentBandIndex);
        }
    };
    addAndMakeVisible(deleteButton);

    // ===== Dynamic EQ Controls =====
    dynLabel.setText("Dynamic EQ", juce::dontSendNotification);
    dynLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    dynLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dynLabel);

    dynEnabledButton.setButtonText("Enable");
    dynEnabledButton.setTooltip("Dynamic EQ aktivieren\nDas Band reagiert dynamisch auf das Eingangssignal.\nDer Gain wird nur angewendet, wenn der Threshold ueberschritten wird.\nIdeal fuer frequenzabhaengige Kompression.");
    dynEnabledButton.onClick = [this]() { updateDynControlsVisibility(); };
    addAndMakeVisible(dynEnabledButton);

    auto setupDynSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& labelText) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 18);
        addAndMakeVisible(slider);
        label.setText(labelText, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(11.0f)));
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    setupDynSlider(dynThresholdSlider, dynThresholdLabel, "Thresh");
    setupDynSlider(dynRatioSlider, dynRatioLabel, "Ratio");
    setupDynSlider(dynAttackSlider, dynAttackLabel, "Attack");
    setupDynSlider(dynReleaseSlider, dynReleaseLabel, "Release");

    dynThresholdSlider.setTooltip("Threshold (Schwellenwert)\nAb welchem Pegel das dynamische EQ-Band eingreift.\nSignale unterhalb werden nicht bearbeitet.");
    dynRatioSlider.setTooltip("Ratio (Verhaeltnis)\nWie stark der Gain angewendet wird, wenn der Threshold ueberschritten wird.\n1:1 = Kein Effekt, hoehere Werte = staerkerer Eingriff.");
    dynAttackSlider.setTooltip("Attack (Einschwingzeit)\nWie schnell das dynamische Band reagiert.\nKurze Werte = Schnelle Reaktion auf Transienten.\nLange Werte = Sanfteres Einsetzen.");
    dynReleaseSlider.setTooltip("Release (Ausschwingzeit)\nWie schnell das Band nach dem Eingriff wieder loslaesst.\nKurze Werte = Schnelles Loslassen.\nLange Werte = Sanfteres, natuerlicheres Ausklingen.");

    // Auto-Threshold Button
    autoThresholdButton.setButtonText("Auto");
    autoThresholdButton.setTooltip("Auto-Threshold\nSetzt den Threshold automatisch auf den aktuellen Signalpegel.\nSpiele Audio ab und klicke hier, um den optimalen Threshold zu setzen.");
    autoThresholdButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
    autoThresholdButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF00CCFF));
    autoThresholdButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF00CCFF));
    autoThresholdButton.onClick = [this]() {
        if (eqProcessor != nullptr && currentBandIndex >= 0)
        {
            const auto& band = eqProcessor->getBand(currentBandIndex);
            float levelDB = band.getEnvelopeLevelDB();
            // Threshold auf aktuellen Pegel setzen (leicht abgerundet auf 0.5 dB)
            float roundedThreshold = std::round(levelDB * 2.0f) / 2.0f;
            roundedThreshold = juce::jlimit(-60.0f, 0.0f, roundedThreshold);
            dynThresholdSlider.setValue(roundedThreshold, juce::sendNotification);
        }
    };
    addAndMakeVisible(autoThresholdButton);

    updateDynControlsVisibility();

    // NEU: Solo Button
    soloButton.setButtonText("Solo");
    soloButton.setClickingTogglesState(true);
    soloButton.setTooltip("Band Solo\nHoere nur den Frequenzbereich dieses Bandes.\nAlle anderen Baender werden stumm geschaltet.\nPerfekt zum Identifizieren von Problemen in einem bestimmten Bereich.");
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffddaa00));
    addAndMakeVisible(soloButton);

    setSize(200, 460);
    setAlwaysOnTop(true);
    setVisible(false);
}

void BandPopup::paint(juce::Graphics& g)
{
    // Transparenter Hintergrund für bessere Sichtbarkeit
    g.setColour(CustomLookAndFeel::getBackgroundMid().withAlpha(0.88f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    // Border
    g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.6f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 2.0f);

    // Trennlinie unter Titel
    g.setColour(CustomLookAndFeel::getBackgroundLight());
    g.drawLine(10.0f, 35.0f, static_cast<float>(getWidth() - 10), 35.0f, 1.0f);
}

void BandPopup::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    
    // Titel
    titleLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(15);

    // Type
    typeLabel.setBounds(bounds.removeFromTop(18));
    typeCombo.setBounds(bounds.removeFromTop(30).reduced(2));
    bounds.removeFromTop(8);

    // Channel
    channelLabel.setBounds(bounds.removeFromTop(18));
    channelCombo.setBounds(bounds.removeFromTop(30).reduced(2));
    bounds.removeFromTop(8);

    // Slope (nur bei Cut-Filtern sichtbar)
    slopeLabel.setBounds(bounds.removeFromTop(18));
    auto slopeRow = bounds.removeFromTop(30).reduced(2);
    slopeCombo.setBounds(slopeRow.removeFromLeft(slopeRow.getWidth() - 55));
    slopeRow.removeFromLeft(5);
    slopeDisplayLabel.setBounds(slopeRow);
    bounds.removeFromTop(12);

    // Buttons
    auto buttonRow = bounds.removeFromTop(32);
    bypassButton.setBounds(buttonRow.removeFromLeft(70).reduced(2));
    buttonRow.removeFromLeft(3);
    soloButton.setBounds(buttonRow.removeFromLeft(50).reduced(2));
    buttonRow.removeFromLeft(3);
    deleteButton.setBounds(buttonRow.reduced(2));
    bounds.removeFromTop(8);

    // ===== Dynamic EQ Section =====
    // Trennlinie + Titel
    dynLabel.setBounds(bounds.removeFromTop(18));
    auto dynEnableRow = bounds.removeFromTop(24);
    dynEnabledButton.setBounds(dynEnableRow.reduced(2));
    bounds.removeFromTop(4);

    auto makeDynRow = [&](juce::Label& label, juce::Slider& slider) {
        auto row = bounds.removeFromTop(22);
        label.setBounds(row.removeFromLeft(45));
        slider.setBounds(row.reduced(1));
        bounds.removeFromTop(2);
    };

    // Threshold-Reihe mit Auto-Button
    {
        auto row = bounds.removeFromTop(22);
        dynThresholdLabel.setBounds(row.removeFromLeft(45));
        auto autoArea = row.removeFromRight(32);
        dynThresholdSlider.setBounds(row.reduced(1));
        autoThresholdButton.setBounds(autoArea.reduced(1));
        bounds.removeFromTop(2);
    }
    makeDynRow(dynRatioLabel, dynRatioSlider);
    makeDynRow(dynAttackLabel, dynAttackSlider);
    makeDynRow(dynReleaseLabel, dynReleaseSlider);
}

void BandPopup::setBandData(int bandIndex, float frequency, float /*gain*/,
                             ParameterIDs::FilterType type, ParameterIDs::ChannelMode channel,
                             int slope, bool bypassed)
{
    currentBandIndex = bandIndex;
    currentFilterType = type;

    // Titel mit Frequenz
    juce::String freqText = frequency >= 1000.0f 
        ? juce::String(frequency / 1000.0f, 1) + " kHz"
        : juce::String(static_cast<int>(frequency)) + " Hz";
    titleLabel.setText(freqText + " · Band " + juce::String(bandIndex + 1), juce::dontSendNotification);

    // Werte setzen
    typeCombo.setSelectedId(static_cast<int>(type) + 1, juce::dontSendNotification);
    channelCombo.setSelectedId(static_cast<int>(channel) + 1, juce::dontSendNotification);
    slopeCombo.setSelectedId(slope, juce::dontSendNotification);
    bypassButton.setToggleState(bypassed, juce::dontSendNotification);

    updateSlopeVisibility();
    updateSlopeDisplay();
}

void BandPopup::showAtPoint(juce::Point<int> position, juce::Component* parent)
{
    if (parent == nullptr)
        return;

    // Intelligente Positionierung: Links oder rechts vom Point, je nach Position
    auto parentBounds = parent->getLocalBounds();
    int x;
    
    // Wenn Point in rechter Hälfte -> Popup LINKS davon
    // Wenn Point in linker Hälfte -> Popup RECHTS davon
    if (position.x > parentBounds.getWidth() / 2)
    {
        // Point ist rechts -> Popup links vom Point
        x = position.x - getWidth() - 25;
        if (x < 15) // Mindestabstand zum linken Rand
            x = 15;
    }
    else
    {
        // Point ist links -> Popup rechts vom Point
        x = position.x + 25;
        if (x + getWidth() > parentBounds.getWidth() - 15)
            x = parentBounds.getWidth() - getWidth() - 15;
    }
    
    int y = position.y - getHeight() / 2; // Zentriert auf Band-Point Y-Position

    // Bounds-Checking für Y-Achse
    if (y < 70) // Unterhalb des Headers
        y = 70;
    if (y + getHeight() > parentBounds.getHeight() - 155) // Oberhalb der BandControls
        y = parentBounds.getHeight() - getHeight() - 155;

    setBounds(x, y, getWidth(), getHeight());
    setVisible(true);
    toFront(true);
}

void BandPopup::setAttachments(juce::AudioProcessorValueTreeState& apvts, int bandIndex)
{
    clearAttachments();

    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParameterIDs::getBandTypeID(bandIndex), typeCombo);
    channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParameterIDs::getBandChannelID(bandIndex), channelCombo);
    slopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParameterIDs::getBandSlopeID(bandIndex), slopeCombo);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ParameterIDs::getBandBypassID(bandIndex), bypassButton);

    // Dynamic EQ Attachments
    dynEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ParameterIDs::getBandDynEnabledID(bandIndex), dynEnabledButton);
    dynThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::getBandDynThresholdID(bandIndex), dynThresholdSlider);
    dynRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::getBandDynRatioID(bandIndex), dynRatioSlider);
    dynAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::getBandDynAttackID(bandIndex), dynAttackSlider);
    dynReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::getBandDynReleaseID(bandIndex), dynReleaseSlider);

    // NEU: Solo Attachment
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ParameterIDs::getBandSoloID(bandIndex), soloButton);

    updateDynControlsVisibility();
}

void BandPopup::clearAttachments()
{
    typeAttachment.reset();
    channelAttachment.reset();
    slopeAttachment.reset();
    bypassAttachment.reset();
    dynEnabledAttachment.reset();
    dynThresholdAttachment.reset();
    dynRatioAttachment.reset();
    dynAttackAttachment.reset();
    dynReleaseAttachment.reset();
    soloAttachment.reset();
}

void BandPopup::addListener(Listener* listener)
{
    listeners.push_back(listener);
}

void BandPopup::removeListener(Listener* listener)
{
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

void BandPopup::notifyChange(const juce::String& paramName, float value)
{
    if (currentBandIndex >= 0)
    {
        for (auto* listener : listeners)
            listener->bandPopupValueChanged(currentBandIndex, paramName, value);
    }
}

void BandPopup::updateSlopeVisibility()
{
    bool isCutFilter = (currentFilterType == ParameterIDs::FilterType::LowCut || 
                       currentFilterType == ParameterIDs::FilterType::HighCut);
    
    slopeCombo.setEnabled(isCutFilter);
    slopeLabel.setEnabled(isCutFilter);
    slopeDisplayLabel.setVisible(isCutFilter);
    
    float alpha = isCutFilter ? 1.0f : 0.4f;
    slopeCombo.setAlpha(alpha);
    slopeLabel.setColour(juce::Label::textColourId, 
                         CustomLookAndFeel::getTextColor().withAlpha(alpha));
    
    if (isCutFilter)
        updateSlopeDisplay();
}

void BandPopup::updateSlopeDisplay()
{
    int slopeValue = slopeCombo.getSelectedId();
    slopeDisplayLabel.setText(juce::String(slopeValue) + " dB/oct", juce::dontSendNotification);
}

void BandPopup::updateDynControlsVisibility()
{
    bool dynEnabled = dynEnabledButton.getToggleState();
    float alpha = dynEnabled ? 1.0f : 0.4f;
    
    dynThresholdSlider.setEnabled(dynEnabled);
    dynRatioSlider.setEnabled(dynEnabled);
    dynAttackSlider.setEnabled(dynEnabled);
    dynReleaseSlider.setEnabled(dynEnabled);
    
    dynThresholdSlider.setAlpha(alpha);
    dynRatioSlider.setAlpha(alpha);
    dynAttackSlider.setAlpha(alpha);
    dynReleaseSlider.setAlpha(alpha);
    autoThresholdButton.setEnabled(dynEnabled);
    autoThresholdButton.setAlpha(alpha);
    dynThresholdLabel.setAlpha(alpha);
    dynRatioLabel.setAlpha(alpha);
    dynAttackLabel.setAlpha(alpha);
    dynReleaseLabel.setAlpha(alpha);
}
