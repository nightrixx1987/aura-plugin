#pragma once

#include <JuceHeader.h>
#include "../DSP/ReferenceAudioPlayer.h"
#include "../DSP/SpectralMatcher.h"
#include "CustomLookAndFeel.h"

/**
 * ReferenceTrackPanel: UI-Komponente für Reference-Track-Funktionalität.
 * 
 * Features:
 * - Drag & Drop von Audio-Dateien
 * - Waveform-Anzeige mit Playback-Position
 * - Play/Stop/Loop-Controls
 * - Volume-Regler für Reference-Track
 * - A/B-Button für schnellen Vergleich
 * - Spectrum-Overlay-Toggle
 * - NEU: EQ Match Button (wie FabFilter Pro-Q4)
 */
class ReferenceTrackPanel : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            public juce::Timer
{
public:
    // Minimale/Maximale Höhe
    static constexpr int minHeight = 80;
    static constexpr int maxHeight = 300;
    static constexpr int defaultHeight = 130;
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    explicit ReferenceTrackPanel(ReferenceAudioPlayer& player)
        : audioPlayer(player)
    {
        // Titel
        titleLabel.setText("Reference Track", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColor());
        addAndMakeVisible(titleLabel);
        
        // Load-Button
        loadButton.setButtonText("Load");
        loadButton.setTooltip("Lade einen Reference-Track (WAV, AIFF, FLAC, MP3)");
        loadButton.onClick = [this]() { loadReferenceFile(); };
        addAndMakeVisible(loadButton);
        
        // Clear-Button
        clearButton.setButtonText("X");
        clearButton.setTooltip("Reference-Track entfernen");
        clearButton.onClick = [this]() 
        { 
            audioPlayer.unloadFile();
            fileNameLabel.setText("Drag & Drop oder Load klicken", juce::dontSendNotification);
            repaint();
        };
        addAndMakeVisible(clearButton);
        
        // Play/Stop-Button
        playButton.setButtonText("Play");
        playButton.setClickingTogglesState(true);
        playButton.setTooltip("Reference-Track abspielen");
        playButton.onClick = [this]()
        {
            audioPlayer.setPlaying(playButton.getToggleState());
            playButton.setButtonText(playButton.getToggleState() ? "Stop" : "Play");
        };
        addAndMakeVisible(playButton);
        
        // Loop-Button
        loopButton.setButtonText("Loop");
        loopButton.setClickingTogglesState(true);
        loopButton.setToggleState(true, juce::dontSendNotification);
        loopButton.setTooltip("Loop ein/aus");
        loopButton.onClick = [this]()
        {
            audioPlayer.setLooping(loopButton.getToggleState());
        };
        addAndMakeVisible(loopButton);
        
        // Volume-Slider
        volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        volumeSlider.setRange(0.0, 2.0, 0.01);
        volumeSlider.setValue(1.0);
        volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
        volumeSlider.setTooltip("Reference-Track Lautstärke");
        volumeSlider.onValueChange = [this]()
        {
            audioPlayer.setPlaybackGain(static_cast<float>(volumeSlider.getValue()));
        };
        addAndMakeVisible(volumeSlider);
        
        volumeLabel.setText("Vol:", juce::dontSendNotification);
        volumeLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(volumeLabel);
        
        // Spectrum Overlay Toggle
        spectrumOverlayButton.setButtonText("Spektrum anzeigen");
        spectrumOverlayButton.setClickingTogglesState(true);
        spectrumOverlayButton.setTooltip("Zeige Reference-Spektrum im Analyzer");
        spectrumOverlayButton.onClick = [this]()
        {
            if (onSpectrumOverlayChanged)
                onSpectrumOverlayChanged(spectrumOverlayButton.getToggleState());
        };
        addAndMakeVisible(spectrumOverlayButton);
        
        // A/B-Button
        abButton.setButtonText("A/B Vergleich");
        abButton.setClickingTogglesState(true);
        abButton.setTooltip("A/B-Vergleich: Schaltet zwischen Input und Reference um");
        abButton.onClick = [this]()
        {
            if (onABCompareChanged)
                onABCompareChanged(abButton.getToggleState());
        };
        addAndMakeVisible(abButton);
        
        // NEU: EQ Match Button (wie FabFilter Pro-Q4)
        matchEQButton.setButtonText("Match EQ");
        matchEQButton.setTooltip("Generiert automatisch EQ-Kurve basierend auf Reference-Track");
        matchEQButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00aa88));
        matchEQButton.onClick = [this]()
        {
            if (onMatchEQClicked)
                onMatchEQClicked();
        };
        addAndMakeVisible(matchEQButton);
        
        // NEU: Match Strength Slider
        matchStrengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        matchStrengthSlider.setRange(0.0, 1.0, 0.01);
        matchStrengthSlider.setValue(0.7);
        matchStrengthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        matchStrengthSlider.setTooltip("Match-Stärke: Wie stark soll das EQ-Matching sein?");
        matchStrengthSlider.onValueChange = [this]()
        {
            if (onMatchStrengthChanged)
                onMatchStrengthChanged(static_cast<float>(matchStrengthSlider.getValue()));
        };
        addAndMakeVisible(matchStrengthSlider);
        
        matchStrengthLabel.setText("Strength:", juce::dontSendNotification);
        matchStrengthLabel.setJustificationType(juce::Justification::centredRight);
        matchStrengthLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        addAndMakeVisible(matchStrengthLabel);
        
        // Dateiname-Label
        fileNameLabel.setText("Drag & Drop oder Load klicken", juce::dontSendNotification);
        fileNameLabel.setJustificationType(juce::Justification::centredLeft);
        fileNameLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColor().withAlpha(0.6f));
        addAndMakeVisible(fileNameLabel);
        
        // Callbacks vom AudioPlayer
        audioPlayer.onFileLoaded = [this](const juce::File& file)
        {
            fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
            fileNameLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColor());
            repaint();
        };
        
        audioPlayer.onFileUnloaded = [this]()
        {
            fileNameLabel.setText("Drag & Drop oder Load klicken", juce::dontSendNotification);
            fileNameLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColor().withAlpha(0.6f));
            playButton.setToggleState(false, juce::dontSendNotification);
            playButton.setButtonText("Play");
            repaint();
        };
        
        audioPlayer.onThumbnailChanged = [this]()
        {
            repaint();
        };
        
        // Timer für Playback-Position
        startTimerHz(30);
    }
    
    ~ReferenceTrackPanel() override
    {
        stopTimer();
    }
    
    //==========================================================================
    // Component
    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        // Resize-Handle oben (visueller Hinweis - dicker und besser sichtbar)
        auto handleArea = getLocalBounds().removeFromTop(resizeHandleHeight);
        
        // Hintergrund für Handle-Bereich
        g.setColour(CustomLookAndFeel::getBackgroundDark());
        g.fillRect(handleArea);
        
        // Griff-Linie in der Mitte (breit und gut sichtbar)
        auto gripArea = handleArea.reduced(getWidth() / 4, 3);
        g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.6f));
        g.fillRoundedRectangle(gripArea.toFloat(), 2.0f);
        
        // Zusätzliche Griff-Linien für bessere Visualisierung
        g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.3f));
        g.drawHorizontalLine(handleArea.getCentreY() - 2, gripArea.getX() + 20.0f, gripArea.getRight() - 20.0f);
        g.drawHorizontalLine(handleArea.getCentreY() + 2, gripArea.getX() + 20.0f, gripArea.getRight() - 20.0f);
        
        // Hintergrund
        g.setColour(CustomLookAndFeel::getBackgroundMid());
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
        
        // Rahmen
        g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.3f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);
        
        // Waveform-Bereich
        auto waveformBounds = getWaveformBounds();
        
        // Waveform-Hintergrund
        g.setColour(CustomLookAndFeel::getBackgroundDark());
        g.fillRect(waveformBounds);
        
        if (audioPlayer.isLoaded())
        {
            // Waveform zeichnen - Cyan/Türkis wie das Spektrum
            auto& thumbnail = audioPlayer.getThumbnail();
            
            // Hintergrund-Gradient für schönere Darstellung
            juce::ColourGradient waveformGradient(
                juce::Colour(0xff00dddd).withAlpha(0.8f),  // Türkis oben
                static_cast<float>(waveformBounds.getCentreX()), static_cast<float>(waveformBounds.getY()),
                juce::Colour(0xff0088aa).withAlpha(0.5f),  // Dunkleres Türkis unten
                static_cast<float>(waveformBounds.getCentreX()), static_cast<float>(waveformBounds.getBottom()),
                false
            );
            g.setGradientFill(waveformGradient);
            thumbnail.drawChannels(g, waveformBounds, 0.0, thumbnail.getTotalLength(), 1.0f);
            
            // Playback-Position
            if (audioPlayer.isPlaying() || audioPlayer.getPlaybackPosition() > 0.0f)
            {
                float posX = waveformBounds.getX() + 
                             audioPlayer.getPlaybackPosition() * waveformBounds.getWidth();
                g.setColour(juce::Colours::white);
                g.drawVerticalLine(static_cast<int>(posX), 
                                   static_cast<float>(waveformBounds.getY()), 
                                   static_cast<float>(waveformBounds.getBottom()));
            }
            
            // Dauer anzeigen
            g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.6f));
            g.setFont(10.0f);
            
            int totalSecs = static_cast<int>(audioPlayer.getDurationSeconds());
            int mins = totalSecs / 60;
            int secs = totalSecs % 60;
            
            juce::String durationText = juce::String::formatted("%d:%02d", mins, secs);
            g.drawText(durationText, waveformBounds.removeFromRight(40).reduced(2), 
                       juce::Justification::centredRight);
        }
        else
        {
            // Placeholder wenn keine Datei geladen
            g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.3f));
            g.setFont(12.0f);
            g.drawText("Keine Reference geladen", waveformBounds, juce::Justification::centred);
        }
        
        // Drag & Drop Highlight
        if (isDragOver)
        {
            g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.3f));
            g.fillRect(waveformBounds);
            g.setColour(CustomLookAndFeel::getAccentColor());
            g.drawRect(waveformBounds, 2);
            
            g.setColour(juce::Colours::white);
            g.setFont(14.0f);
            g.drawText("Drop Audio File Here", waveformBounds, juce::Justification::centred);
        }
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        
        // Obere Zeile: Titel, Load, Clear
        auto topRow = bounds.removeFromTop(24);
        titleLabel.setBounds(topRow.removeFromLeft(100));
        clearButton.setBounds(topRow.removeFromRight(24));
        topRow.removeFromRight(5);
        loadButton.setBounds(topRow.removeFromRight(50));
        fileNameLabel.setBounds(topRow.reduced(5, 0));
        
        bounds.removeFromTop(5);
        
        // Waveform-Bereich (Mitte)
        waveformArea = bounds.removeFromTop(60);
        
        bounds.removeFromTop(5);
        
        // Untere Zeile: Controls
        auto bottomRow = bounds.removeFromTop(26);
        
        playButton.setBounds(bottomRow.removeFromLeft(50));
        bottomRow.removeFromLeft(5);
        loopButton.setBounds(bottomRow.removeFromLeft(45));
        bottomRow.removeFromLeft(10);
        
        volumeLabel.setBounds(bottomRow.removeFromLeft(30));
        volumeSlider.setBounds(bottomRow.removeFromLeft(80));
        bottomRow.removeFromLeft(10);
        
        spectrumOverlayButton.setBounds(bottomRow.removeFromLeft(100));
        bottomRow.removeFromLeft(5);
        abButton.setBounds(bottomRow.removeFromLeft(75));
        bottomRow.removeFromLeft(10);
        
        // NEU: Match EQ Controls
        matchEQButton.setBounds(bottomRow.removeFromLeft(70));
        bottomRow.removeFromLeft(8);
        matchStrengthLabel.setBounds(bottomRow.removeFromLeft(55));
        matchStrengthSlider.setBounds(bottomRow.removeFromLeft(90));
    }
    
    //==========================================================================
    // FileDragAndDropTarget
    //==========================================================================
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (const auto& file : files)
        {
            juce::File f(file);
            juce::String ext = f.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || 
                ext == ".flac" || ext == ".mp3" || ext == ".ogg")
            {
                return true;
            }
        }
        return false;
    }
    
    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        isDragOver = true;
        repaint();
    }
    
    void fileDragExit(const juce::StringArray&) override
    {
        isDragOver = false;
        repaint();
    }
    
    void filesDropped(const juce::StringArray& files, int, int) override
    {
        isDragOver = false;
        
        for (const auto& filePath : files)
        {
            juce::File file(filePath);
            if (audioPlayer.loadFile(file))
            {
                break;  // Nur erste gültige Datei laden
            }
        }
        
        repaint();
    }
    
    //==========================================================================
    // Timer (für Playback-Position Update)
    //==========================================================================
    void timerCallback() override
    {
        if (audioPlayer.isPlaying())
        {
            repaint();
        }
        
        // Update Play-Button wenn Playback endet
        if (!audioPlayer.isPlaying() && playButton.getToggleState())
        {
            playButton.setToggleState(false, juce::dontSendNotification);
            playButton.setButtonText("Play");
        }
    }
    
    //==========================================================================
    // Callbacks
    //==========================================================================
    std::function<void(bool)> onSpectrumOverlayChanged;
    std::function<void(bool)> onABCompareChanged;
    
    //==========================================================================
    // Getters
    //==========================================================================
    bool isSpectrumOverlayEnabled() const { return spectrumOverlayButton.getToggleState(); }
    bool isABCompareEnabled() const { return abButton.getToggleState(); }
    
private:
    //==========================================================================
    // Hilfsfunktionen
    //==========================================================================
    void loadReferenceFile()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Wähle Reference-Track",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg"
        );
        
        auto chooserFlags = juce::FileBrowserComponent::openMode | 
                     juce::FileBrowserComponent::canSelectFiles;
        
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioPlayer.loadFile(file);
            }
        });
    }
    
    juce::Rectangle<int> getWaveformBounds() const
    {
        return waveformArea;
    }
    
    //==========================================================================
    // Member
    //==========================================================================
    ReferenceAudioPlayer& audioPlayer;
    
    juce::Label titleLabel;
    juce::Label fileNameLabel;
    juce::TextButton loadButton;
    juce::TextButton clearButton;
    juce::TextButton playButton;
    juce::ToggleButton loopButton;
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    juce::ToggleButton spectrumOverlayButton;
    juce::ToggleButton abButton;
    
    // NEU: Match EQ Controls
    juce::TextButton matchEQButton;
    juce::Slider matchStrengthSlider;
    juce::Label matchStrengthLabel;
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    juce::Rectangle<int> waveformArea;
    bool isDragOver = false;
    
    // Resize-Funktionalität
    bool isResizing = false;
    int resizeStartY = 0;
    int resizeStartHeight = 0;
    static constexpr int resizeHandleHeight = 12;
    
public:
    // Callback wenn Höhe geändert wird
    std::function<void(int)> onHeightChanged;
    
    // NEU: Callbacks für Match EQ
    std::function<void()> onMatchEQClicked;
    std::function<void(float)> onMatchStrengthChanged;
    
    // Resize Mouse-Handling
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (isInResizeArea(e.position))
        {
            isResizing = true;
            resizeStartY = e.getScreenY();
            resizeStartHeight = getHeight();
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isResizing)
        {
            int deltaY = resizeStartY - e.getScreenY();
            int newHeight = juce::jlimit(minHeight, maxHeight, resizeStartHeight + deltaY);
            
            if (onHeightChanged)
                onHeightChanged(newHeight);
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isResizing = false;
    }
    
    void mouseMove(const juce::MouseEvent& e) override
    {
        if (isInResizeArea(e.position))
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    
    void mouseExit(const juce::MouseEvent&) override
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    
private:
    bool isInResizeArea(juce::Point<float> pos) const
    {
        return pos.y < resizeHandleHeight;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferenceTrackPanel)
};
