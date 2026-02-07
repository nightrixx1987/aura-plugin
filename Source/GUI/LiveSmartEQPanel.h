#pragma once

#include <JuceHeader.h>
#include "../DSP/LiveSmartEQ.h"
#include "../DSP/SmartAnalyzer.h"
#include "../Parameters/ParameterIDs.h"

/**
 * LiveSmartEQPanel: UI-Komponente für Live SmartEQ Steuerung.
 * 
 * Features:
 * - Ein/Aus Toggle mit visueller Anzeige
 * - Modus-Auswahl (Gentle/Normal/Aggressive/Custom)
 * - Depth-Slider (wie stark eingegriffen wird)
 * - Attack/Release-Regler
 * - Gain-Reduction-Anzeige pro Band
 * - Gesamte Reduktion-Meter
 */
class LiveSmartEQPanel : public juce::Component,
                          public juce::Timer
{
public:
    // Breite im ausgeklappten/eingeklappten Zustand
    static constexpr int expandedWidth = 220;
    static constexpr int collapsedWidth = 24;
    
    LiveSmartEQPanel(juce::AudioProcessorValueTreeState& apvts, LiveSmartEQ& liveEQ)
        : apvtsRef(apvts), liveSmartEQ(liveEQ)
    {
        // Collapse Button (links statt rechts wie bei SmartRecommendationPanel)
        collapseButton.setButtonText("<");
        collapseButton.setTooltip("Panel einklappen/ausklappen");
        collapseButton.onClick = [this]() { toggleCollapsed(); };
        addAndMakeVisible(collapseButton);
        // Enable Button
        enableButton.setButtonText("Live Auto-EQ");
        enableButton.setClickingTogglesState(true);
        enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_ENABLED, enableButton);
        addAndMakeVisible(enableButton);
        
        // Mode ComboBox
        modeCombo.addItemList(ParameterIDs::getLiveSmartEQModeNames(), 1);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_MODE, modeCombo);
        addAndMakeVisible(modeCombo);
        addAndMakeVisible(modeLabel);
        modeLabel.setText("Modus", juce::dontSendNotification);
        modeLabel.setJustificationType(juce::Justification::centred);
        
        // Depth Slider
        setupSlider(depthSlider, depthLabel, "Depth", "%");
        depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_DEPTH, depthSlider);
        depthSlider.setTextValueSuffix("%");
        depthSlider.setNumDecimalPlacesToDisplay(0);
        
        // Attack Slider
        setupSlider(attackSlider, attackLabel, "Attack", "ms");
        attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_ATTACK, attackSlider);
        attackSlider.setTextValueSuffix(" ms");
        attackSlider.setNumDecimalPlacesToDisplay(0);
        
        // Release Slider
        setupSlider(releaseSlider, releaseLabel, "Release", "ms");
        releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_RELEASE, releaseSlider);
        releaseSlider.setTextValueSuffix(" ms");
        releaseSlider.setNumDecimalPlacesToDisplay(0);
        
        // Threshold Slider
        setupSlider(thresholdSlider, thresholdLabel, "Threshold", "dB");
        thresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_THRESHOLD, thresholdSlider);
        thresholdSlider.setTextValueSuffix(" dB");
        thresholdSlider.setNumDecimalPlacesToDisplay(1);
        
        // Max Reduction Slider
        setupSlider(maxReductionSlider, maxReductionLabel, "Max Red.", "dB");
        maxReductionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_MAX_REDUCTION, maxReductionSlider);
        maxReductionSlider.setTextValueSuffix(" dB");
        maxReductionSlider.setNumDecimalPlacesToDisplay(1);
        
        // Transient Protection Toggle
        transientButton.setButtonText("Transient Protect");
        transientButton.setClickingTogglesState(true);
        transientAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_TRANSIENT_PROTECT, transientButton);
        addAndMakeVisible(transientButton);
        
        // Reference-Track als Ziel Button
        useReferenceButton.setButtonText("Use Reference");
        useReferenceButton.setClickingTogglesState(true);
        useReferenceButton.setTooltip("Verwendet den Reference-Track als Ziel-Spektrum");
        useReferenceButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00dddd));
        addAndMakeVisible(useReferenceButton);
        
        // NEU: Mid/Side Mode ComboBox
        msModeCombo.addItemList(ParameterIDs::getLiveSmartEQMSModeNames(), 1);
        msModeCombo.setTooltip("Mid/Side Processing für Live EQ");
        msModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_MS_MODE, msModeCombo);
        addAndMakeVisible(msModeCombo);
        addAndMakeVisible(msModeLabel);
        msModeLabel.setText("M/S Mode", juce::dontSendNotification);
        msModeLabel.setJustificationType(juce::Justification::centredLeft);
        msModeLabel.setFont(juce::Font(juce::FontOptions(9.0f)));
        msModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        
        // NEU: Profile ComboBox
        profileAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvtsRef, ParameterIDs::LIVE_SMART_EQ_PROFILE, profileCombo);
        profileCombo.setTooltip("Instrumenten-/Genre-Profil für Zielkurve");
        addAndMakeVisible(profileCombo);
        addAndMakeVisible(profileLabel);
        profileLabel.setText("Instrument Profile", juce::dontSendNotification);
        profileLabel.setJustificationType(juce::Justification::centredLeft);
        profileLabel.setFont(juce::Font(juce::FontOptions(9.0f)));
        profileLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        
        useReferenceButton.onClick = [this]()
        {
            bool useRef = useReferenceButton.getToggleState();
            
            // DIREKT den Zustand im LiveSmartEQ setzen
            liveSmartEQ.setUseReferenceAsTarget(useRef);
            
            // Auch den externen Callback aufrufen (für Reference-Spektrum laden)
            if (onUseReferenceChanged)
                onUseReferenceChanged(useRef);
        };
        
        // Timer für Meter-Updates starten
        startTimerHz(30);
    }
    
    ~LiveSmartEQPanel() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Hintergrund - passt zum dunklen Theme
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0d0d0d));
        g.fillRoundedRectangle(bounds, 6.0f);
        
        // Rand - Magenta Akzent wenn aktiv
        if (liveSmartEQ.isEnabled())
        {
            g.setColour(juce::Colour(0xffff00ff).withAlpha(0.6f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff2a2a2a));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.0f);
        }
        
        if (collapsed)
        {
            // Vertikaler Text wenn eingeklappt
            g.setColour(juce::Colour(0xffff00ff));
            
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")), "Live EQ", 0, 0);
            
            juce::Path textPath;
            glyphs.createPath(textPath);
            
            auto transform = juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi)
                .translated(14.0f, getHeight() - 30.0f);
            textPath.applyTransform(transform);
            
            g.fillPath(textPath);
        }
        else
        {
            // Titel mit Akzentfarbe
            g.setColour(juce::Colour(0xffff00ff));
            g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
            g.drawText("LIVE SMART EQ", bounds.removeFromTop(22.0f), juce::Justification::centred);
            
            // DEBUG: Status-Anzeige für Reference-Matching
#if JUCE_DEBUG
            {
                bool hasRef = liveSmartEQ.hasReference();
                bool useRef = liveSmartEQ.getUseReferenceAsTarget();
                
                // Non-const Reference um getMatchPoints() aufrufen zu können
                auto& matcher = const_cast<SpectralMatcher&>(liveSmartEQ.getSpectralMatcher());
                int matchCount = static_cast<int>(matcher.getMatchPoints().size());
                size_t refSize = matcher.getReferenceSpectrum().size();
                size_t inSize = matcher.getInputSpectrum().size();
                
                juce::String statusText;
                if (hasRef && useRef)
                {
                    // Zeige detaillierte Info: Ref-Größe, Input-Größe, Match-Punkte
                    statusText = "R:" + juce::String(refSize) + 
                                 " I:" + juce::String(inSize) + 
                                 " M:" + juce::String(matchCount);
                }
                else
                {
                    statusText = "Ref: " + juce::String(hasRef ? "YES" : "NO") + 
                                 " | Use: " + juce::String(useRef ? "YES" : "NO");
                }
                
                g.setColour(hasRef && useRef ? juce::Colours::cyan : juce::Colours::orange);
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.drawText(statusText, bounds.removeFromTop(12), juce::Justification::centred);
            }
#endif
            
            // Gain Reduction Meter
            drawGainReductionMeters(g);
        }
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        
        if (collapsed)
        {
            // Nur Collapse-Button sichtbar
            collapseButton.setBounds(bounds.removeFromTop(22).reduced(1));
            
            // Andere Controls verstecken
            enableButton.setVisible(false);
            modeLabel.setVisible(false);
            modeCombo.setVisible(false);
            depthSlider.setVisible(false);
            depthLabel.setVisible(false);
            attackSlider.setVisible(false);
            attackLabel.setVisible(false);
            releaseSlider.setVisible(false);
            releaseLabel.setVisible(false);
            thresholdSlider.setVisible(false);
            thresholdLabel.setVisible(false);
            maxReductionSlider.setVisible(false);
            maxReductionLabel.setVisible(false);
            transientButton.setVisible(false);
            msModeCombo.setVisible(false);
            msModeLabel.setVisible(false);
            profileCombo.setVisible(false);
            profileLabel.setVisible(false);
        }
        else
        {
            bounds = getLocalBounds().reduced(8);
            
            // Collapse-Button oben links (wie bei SmartRecommendationPanel)
            auto headerRow = bounds.removeFromTop(22);
            collapseButton.setBounds(headerRow.removeFromLeft(22));
            collapseButton.setVisible(true);
            
            // Enable Button
            enableButton.setBounds(bounds.removeFromTop(24).reduced(2, 0));
            
            bounds.removeFromTop(4);
            
            // Modus
            auto modeRow = bounds.removeFromTop(22);
            modeLabel.setBounds(modeRow.removeFromLeft(45));
            modeCombo.setBounds(modeRow.reduced(2, 0));
            
            bounds.removeFromTop(6);
            
            // Kompakteres Layout - 2x3 Grid für Slider
            const int knobSize = 45;
            const int labelHeight = 14;
            const int rowHeight = labelHeight + knobSize + 4;
            const int colWidth = bounds.getWidth() / 2;
            
            // Reihe 1: Depth + Threshold
            auto row1 = bounds.removeFromTop(rowHeight);
            layoutKnobWithLabel(row1.removeFromLeft(colWidth), depthSlider, depthLabel, knobSize, labelHeight);
            layoutKnobWithLabel(row1, thresholdSlider, thresholdLabel, knobSize, labelHeight);
            
            // Reihe 2: Attack + Release
            auto row2 = bounds.removeFromTop(rowHeight);
            layoutKnobWithLabel(row2.removeFromLeft(colWidth), attackSlider, attackLabel, knobSize, labelHeight);
            layoutKnobWithLabel(row2, releaseSlider, releaseLabel, knobSize, labelHeight);
            
            // Reihe 3: Max Reduction + Transient Button
            auto row3 = bounds.removeFromTop(rowHeight);
            layoutKnobWithLabel(row3.removeFromLeft(colWidth), maxReductionSlider, maxReductionLabel, knobSize, labelHeight);
            transientButton.setBounds(row3.reduced(5, (rowHeight - 24) / 2));
            
            // Reihe 4: Use Reference Button
            bounds.removeFromTop(2);
            auto row4 = bounds.removeFromTop(22);
            useReferenceButton.setBounds(row4.reduced(5, 0));
            useReferenceButton.setVisible(true);
            
            // Reihe 5: M/S Mode (Label über ComboBox)
            bounds.removeFromTop(4);
            msModeLabel.setBounds(bounds.removeFromTop(14).reduced(5, 0));
            msModeCombo.setBounds(bounds.removeFromTop(22).reduced(5, 0));
            msModeCombo.setVisible(true);
            msModeLabel.setVisible(true);
            
            // Reihe 6: Profile (Label über ComboBox für volle Breite)
            bounds.removeFromTop(4);
            profileLabel.setBounds(bounds.removeFromTop(14).reduced(5, 0));
            profileCombo.setBounds(bounds.removeFromTop(22).reduced(5, 0));
            profileCombo.setVisible(true);
            profileLabel.setVisible(true);
            
            // Gain Reduction Meters (Rest des Platzes)
            bounds.removeFromTop(5);
            meterBounds = bounds;
            
            // Controls sichtbar machen
            collapseButton.setVisible(true);
            enableButton.setVisible(true);
            modeLabel.setVisible(true);
            modeCombo.setVisible(true);
            depthSlider.setVisible(true);
            depthLabel.setVisible(true);
            attackSlider.setVisible(true);
            attackLabel.setVisible(true);
            releaseSlider.setVisible(true);
            releaseLabel.setVisible(true);
            thresholdSlider.setVisible(true);
            thresholdLabel.setVisible(true);
            maxReductionSlider.setVisible(true);
            maxReductionLabel.setVisible(true);
            transientButton.setVisible(true);
        }
    }
    
    void timerCallback() override
    {
        // WICHTIG: Synchronisiere Button-Zustand mit LiveSmartEQ
        // Falls der Button-Zustand nicht mit dem internen Zustand übereinstimmt, synchronisieren
        bool buttonState = useReferenceButton.getToggleState();
        bool internalState = liveSmartEQ.getUseReferenceAsTarget();
        
        if (buttonState != internalState)
        {
            // Setze den internen Zustand auf den Button-Zustand
            liveSmartEQ.setUseReferenceAsTarget(buttonState);
            
            // Triggere auch den Callback um das Reference-Spektrum zu laden
            if (onUseReferenceChanged)
                onUseReferenceChanged(buttonState);
        }
        
        // Repaint für Meter-Updates und Status-Anzeige
        repaint();
    }
    
private:
    void layoutKnobWithLabel(juce::Rectangle<int> area, juce::Slider& slider, 
                              juce::Label& label, int knobSize, int labelHeight)
    {
        // Label oben
        auto labelArea = area.removeFromTop(labelHeight);
        label.setBounds(labelArea);
        
        // Knob zentriert darunter
        auto knobArea = area.withSizeKeepingCentre(knobSize, knobSize);
        slider.setBounds(knobArea);
    }
    
    void setupSlider(juce::Slider& slider, juce::Label& label, 
                     const juce::String& name, const juce::String& /*suffix*/)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 11);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff00ff));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a2a2a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffcccccc));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1a1a1a));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(slider);
        
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(9.0f)));
        label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        addAndMakeVisible(label);
    }
    
    void drawGainReductionMeters(juce::Graphics& g)
    {
        if (meterBounds.isEmpty()) return;
        
        // Hintergrund für Meter-Bereich
        g.setColour(juce::Colour(0xff151515));
        g.fillRoundedRectangle(meterBounds.toFloat(), 4.0f);
        
        const int numBands = liveSmartEQ.getMaxBands();
        const float meterWidth = static_cast<float>(meterBounds.getWidth()) / static_cast<float>(numBands);
        const float meterHeight = static_cast<float>(meterBounds.getHeight()) - 4.0f;
        
        // Gesamt-Reduktion anzeigen
        float totalReduction = liveSmartEQ.getTotalGainReduction();
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText(juce::String(totalReduction, 1) + " dB", 
                   meterBounds.removeFromTop(12), juce::Justification::centred);
        
        for (int i = 0; i < numBands; ++i)
        {
            const auto& state = liveSmartEQ.getBandState(i);
            
            auto meterRect = juce::Rectangle<float>(
                static_cast<float>(meterBounds.getX()) + static_cast<float>(i) * meterWidth + 1.0f,
                static_cast<float>(meterBounds.getY()) + 2.0f,
                meterWidth - 2.0f,
                meterHeight - 2.0f
            );
            
            // Hintergrund
            g.setColour(juce::Colour(0xff0a0a1a));
            g.fillRoundedRectangle(meterRect, 3.0f);
            
            if (state.active && state.gainReduction < -0.1f)
            {
                // Gain Reduction als Balken (-24dB = volle Höhe)
                float reductionNorm = std::abs(state.gainReduction) / 24.0f;
                reductionNorm = juce::jlimit(0.0f, 1.0f, reductionNorm);
                
                float barHeight = meterHeight * reductionNorm;
                
                auto barRect = meterRect.withHeight(barHeight).withBottomY(meterRect.getBottom());
                
                // Farbe basierend auf Problem-Kategorie
                juce::Colour barColour = SmartAnalyzer::getColourForCategory(state.category);
                
                g.setColour(barColour.withAlpha(0.8f));
                g.fillRoundedRectangle(barRect, 3.0f);
                
                // Frequenz-Label
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                
                juce::String freqText;
                if (state.frequency >= 1000.0f)
                    freqText = juce::String(state.frequency / 1000.0f, 1) + "k";
                else
                    freqText = juce::String(static_cast<int>(state.frequency));
                
                g.drawText(freqText, meterRect.toNearestInt(), juce::Justification::centredTop);
                
                // Reduktion-Wert
                g.drawText(juce::String(state.gainReduction, 1) + "dB", 
                          meterRect.toNearestInt(), juce::Justification::centredBottom);
            }
            
            // Rand
            g.setColour(juce::Colour(0xff3a3a5e));
            g.drawRoundedRectangle(meterRect, 3.0f, 1.0f);
        }
    }
    
    // References
    juce::AudioProcessorValueTreeState& apvtsRef;
    LiveSmartEQ& liveSmartEQ;
    
    // UI Components
    juce::ToggleButton enableButton;
    juce::ComboBox modeCombo;
    juce::Label modeLabel;
    
    juce::Slider depthSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider thresholdSlider;
    juce::Slider maxReductionSlider;
    
    juce::Label depthLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label thresholdLabel;
    juce::Label maxReductionLabel;
    
    juce::ToggleButton transientButton;
    juce::ToggleButton useReferenceButton;  // Reference-Track als Ziel verwenden
    
    juce::ComboBox msModeCombo;             // Mid/Side Mode
    juce::Label msModeLabel;
    juce::ComboBox profileCombo;            // Instrumenten-Profil
    juce::Label profileLabel;
    
    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxReductionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> transientAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> msModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAttachment;
    
    // Meter bounds
    juce::Rectangle<int> meterBounds;
    
    // Collapse-Funktionalität
    juce::TextButton collapseButton;
    bool collapsed = false;
    
public:
    // Callback wenn Reference-Modus sich ändert
    std::function<void(bool)> onUseReferenceChanged;
    
    // Reference-Status aktualisieren (ob verfügbar)
    void setReferenceAvailable(bool available)
    {
        useReferenceButton.setEnabled(available);
        if (!available)
        {
            useReferenceButton.setToggleState(false, juce::dontSendNotification);
        }
    }
    
    bool isUsingReference() const { return useReferenceButton.getToggleState(); }
    
private:
    void toggleCollapsed()
    {
        setCollapsed(!collapsed);
    }
    
    void updateCollapsedState()
    {
        collapseButton.setButtonText(collapsed ? ">" : "<");
        collapseButton.setVisible(true);
        resized();
        repaint();
    }
    
public:
    // Collapse-API
    bool isCollapsed() const { return collapsed; }
    
    void setCollapsed(bool shouldBeCollapsed)
    {
        if (collapsed != shouldBeCollapsed)
        {
            collapsed = shouldBeCollapsed;
            updateCollapsedState();
            if (onCollapsedChanged)
                onCollapsedChanged(collapsed);
        }
    }
    
    int getPreferredWidth() const
    {
        return collapsed ? collapsedWidth : expandedWidth;
    }
    
    // Callback wenn sich der Collapse-Zustand ändert
    std::function<void(bool)> onCollapsedChanged;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveSmartEQPanel)
};
