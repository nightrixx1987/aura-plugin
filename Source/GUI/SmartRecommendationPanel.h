#pragma once

#include <JuceHeader.h>
#include "../DSP/SmartAnalyzer.h"
#include "../DSP/SmartEQRecommendation.h"
#include "ThemeManager.h"

/**
 * SmartRecommendationPanel: Zeigt EQ-Empfehlungen als interaktive Liste an.
 * 
 * Features:
 * - Liste aller erkannten Probleme mit Empfehlungen
 * - One-Click-Anwendung einzelner Empfehlungen
 * - "Alle anwenden" Button
 * - Konfidenz-Anzeige
 * - Farbcodierung nach Problemkategorie
 * - Horizontal einklappbar
 */
class SmartRecommendationPanel : public juce::Component
{
public:
    // Breite im ausgeklappten/eingeklappten Zustand
    static constexpr int expandedWidth = 200;
    static constexpr int collapsedWidth = 24;
    
    SmartRecommendationPanel()
    {
        // Collapse Button (immer sichtbar)
        addAndMakeVisible(collapseButton);
        collapseButton.setButtonText("<");
        collapseButton.setTooltip("Panel einklappen/ausklappen");
        collapseButton.onClick = [this]() {
            toggleCollapsed();
        };
        
        // Header
        addAndMakeVisible(headerLabel);
        headerLabel.setText("Smart EQ Empfehlungen", juce::dontSendNotification);
        headerLabel.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
        
        // Toggle Button
        addAndMakeVisible(enableButton);
        enableButton.setButtonText("");
        enableButton.setToggleState(true, juce::dontSendNotification);
        enableButton.onClick = [this]() {
            if (onEnableChanged)
                onEnableChanged(enableButton.getToggleState());
        };
        
        // Apply All Button
        addAndMakeVisible(applyAllButton);
        applyAllButton.setButtonText("Alle anwenden");
        applyAllButton.onClick = [this]() {
            if (onApplyAll)
                onApplyAll();
        };
        
        // Undo Last Button
        addAndMakeVisible(undoLastButton);
        undoLastButton.setButtonText("Letzte rueckgaengig");
        undoLastButton.setEnabled(false);
        undoLastButton.onClick = [this]() {
            if (onUndoLastRecommendation)
                onUndoLastRecommendation();
            // Deaktiviere nach einmaligem Undo
            undoLastButton.setEnabled(false);
        };
        
        // Sensitivity Slider
        addAndMakeVisible(sensitivitySlider);
        sensitivitySlider.setRange(0.1, 2.0, 0.1);
        sensitivitySlider.setValue(1.0);
        sensitivitySlider.setTextValueSuffix("x");
        sensitivitySlider.setNumDecimalPlacesToDisplay(1);
        sensitivitySlider.onValueChange = [this]() {
            if (onSensitivityChanged)
                onSensitivityChanged(static_cast<float>(sensitivitySlider.getValue()));
        };
        
        addAndMakeVisible(sensitivityLabel);
        sensitivityLabel.setText("Empfindlichkeit:", juce::dontSendNotification);
        sensitivityLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        
        // Viewport fuer scrollbare Empfehlungsliste
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&listContent, false);
        viewport.setScrollBarsShown(true, false);
        
        updateCollapsedState();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto& theme = ThemeManager::getInstance().getCurrentTheme();
        
        // Hintergrund
        g.setColour(theme.backgroundMid);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        
        // Rahmen
        g.setColour(theme.accentColor.withAlpha(0.3f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
        
        if (collapsed)
        {
            // Vertikaler Text wenn eingeklappt
            g.setColour(theme.textColor);
            g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
            
            // Text vertikal zeichnen
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText(g.getCurrentFont(), "Smart EQ", 0, 0);
            
            juce::Path textPath;
            glyphs.createPath(textPath);
            
            // Rotieren und positionieren
            auto transform = juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi)
                .translated(14.0f, getHeight() - 30.0f);
            textPath.applyTransform(transform);
            
            g.fillPath(textPath);
        }
        else
        {
            // Empfehlungsliste
            drawRecommendations(g);
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
            headerLabel.setVisible(false);
            enableButton.setVisible(false);
            sensitivityLabel.setVisible(false);
            sensitivitySlider.setVisible(false);
            applyAllButton.setVisible(false);
            undoLastButton.setVisible(false);
            viewport.setVisible(false);
        }
        else
        {
            bounds = getLocalBounds().reduced(10);
            
            // Collapse-Button in der Header-Zeile links
            auto headerRow = bounds.removeFromTop(25);
            collapseButton.setBounds(headerRow.removeFromLeft(22));
            headerRow.removeFromLeft(5);
            headerLabel.setBounds(headerRow.removeFromLeft(130));
            enableButton.setBounds(headerRow.removeFromRight(25));
            
            bounds.removeFromTop(5);
            
            // Sensitivity-Zeile
            auto sensitivityRow = bounds.removeFromTop(25);
            sensitivityLabel.setBounds(sensitivityRow.removeFromLeft(100));
            sensitivitySlider.setBounds(sensitivityRow);
            
            bounds.removeFromTop(10);
            
            // Empfehlungsbereich als Viewport (scrollbar)
            auto viewportArea = bounds.removeFromTop(bounds.getHeight() - 60);
            viewport.setBounds(viewportArea);
            recommendationsArea = viewportArea;
            
            // Berechne benoetigte Hoehe fuer alle Empfehlungen
            const int itemHeight = 50;
            const int spacing = 5;
            int totalContentHeight = static_cast<int>(recommendations.size()) * (itemHeight + spacing);
            totalContentHeight = std::max(totalContentHeight, viewportArea.getHeight());
            listContent.setSize(viewportArea.getWidth() - (recommendations.size() > 5 ? 10 : 0), totalContentHeight);
            
            bounds.removeFromTop(5);
            
            // Button-Zeile: Apply All + Undo nebeneinander
            auto buttonRow = bounds.removeFromBottom(25);
            applyAllButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2 - 2));
            buttonRow.removeFromLeft(4);
            undoLastButton.setBounds(buttonRow);
            
            // Controls sichtbar machen
            collapseButton.setVisible(true);
            headerLabel.setVisible(true);
            enableButton.setVisible(true);
            sensitivityLabel.setVisible(true);
            sensitivitySlider.setVisible(true);
            applyAllButton.setVisible(true);
            undoLastButton.setVisible(true);
            viewport.setVisible(true);
        }
    }
    
    //==========================================================================
    // Collapse-Funktionalität
    //==========================================================================
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
    
    void toggleCollapsed()
    {
        setCollapsed(!collapsed);
    }
    
    int getPreferredWidth() const
    {
        return collapsed ? collapsedWidth : expandedWidth;
    }
    
    // Callback wenn sich Collapsed-Status ändert
    std::function<void(bool)> onCollapsedChanged;
    
    //==========================================================================
    // Empfehlungen aktualisieren
    //==========================================================================
    void updateRecommendations(const std::vector<SmartEQRecommendation::Recommendation>& newRecs)
    {
        recommendations = newRecs;
        applyAllButton.setEnabled(!recommendations.empty());
        
        // Content-Hoehe fuer Viewport aktualisieren
        const int itemHeight = 50;
        const int spacing = 5;
        int totalHeight = static_cast<int>(recommendations.size()) * (itemHeight + spacing);
        totalHeight = std::max(totalHeight, viewport.getHeight());
        listContent.setSize(viewport.getWidth() - (recommendations.size() > 5 ? 10 : 0), totalHeight);
        
        repaint();
    }
    
    void clearRecommendations()
    {
        recommendations.clear();
        applyAllButton.setEnabled(false);
        repaint();
    }
    
    //==========================================================================
    // Callbacks
    //==========================================================================
    std::function<void(bool)> onEnableChanged;
    std::function<void(int)> onApplyRecommendation;  // Index der Empfehlung
    std::function<void()> onApplyAll;
    std::function<void(float)> onSensitivityChanged;
    std::function<void()> onUndoLastRecommendation;  // Letzte Empfehlung rueckgaengig
    
    //==========================================================================
    // Status
    //==========================================================================
    void setAnalysisEnabled(bool enabled)
    {
        enableButton.setToggleState(enabled, juce::dontSendNotification);
    }
    
    bool isAnalysisEnabled() const
    {
        return enableButton.getToggleState();
    }
    
private:
    bool collapsed = false;
    
    void updateCollapsedState()
    {
        collapseButton.setButtonText(collapsed ? ">" : "<");
        collapseButton.setTooltip(collapsed ? "Panel ausklappen" : "Panel einklappen");
        resized();
        repaint();
    }
    
    juce::TextButton collapseButton;
    juce::Label headerLabel;
    juce::ToggleButton enableButton;
    juce::TextButton applyAllButton;
    juce::TextButton undoLastButton;
    juce::Slider sensitivitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label sensitivityLabel;
    juce::Viewport viewport;
    juce::Component listContent;  // Virtueller Content fuer Viewport
    
    std::vector<SmartEQRecommendation::Recommendation> recommendations;
    juce::Rectangle<int> recommendationsArea;
    
    void drawRecommendations(juce::Graphics& g)
    {
        auto& theme = ThemeManager::getInstance().getCurrentTheme();
        
        if (recommendations.empty())
        {
            g.setColour(theme.textColor.withAlpha(0.5f));
            g.setFont(12.0f);
            g.drawText("Keine Probleme erkannt", recommendationsArea,
                      juce::Justification::centred);
            return;
        }
        
        const int itemHeight = 50;
        const int spacing = 5;
        
        // Viewport-Offset beruecksichtigen
        int scrollOffsetY = viewport.getViewPositionY();
        int y = recommendationsArea.getY() - scrollOffsetY;
        
        // Zeichne ALLE Empfehlungen (Viewport kuerzt sichtbaren Bereich)
        for (size_t i = 0; i < recommendations.size(); ++i)
        {
            const auto& rec = recommendations[i];
            juce::Rectangle<int> itemBounds(recommendationsArea.getX(), y,
                                           recommendationsArea.getWidth(), itemHeight);
            
            // Nur zeichnen wenn sichtbar (Performance-Optimierung)
            if (y + itemHeight >= recommendationsArea.getY() - scrollOffsetY && 
                y <= recommendationsArea.getBottom() - scrollOffsetY + viewport.getHeight())
            {
                drawRecommendationItem(g, rec, itemBounds, static_cast<int>(i));
            }
            
            y += itemHeight + spacing;
        }
    }
    
    void drawRecommendationItem(juce::Graphics& g, const SmartEQRecommendation::Recommendation& rec,
                                juce::Rectangle<int> bounds, int /*index*/)
    {
        auto& theme = ThemeManager::getInstance().getCurrentTheme();
        juce::Colour categoryColour = SmartAnalyzer::getColourForCategory(rec.sourceCategory);
        
        // Hintergrund
        g.setColour(theme.backgroundLight.withAlpha(0.5f));
        g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
        
        // Linker Farbstreifen
        g.setColour(categoryColour);
        g.fillRoundedRectangle(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()), 4.0f, static_cast<float>(bounds.getHeight()), 2.0f);
        
        // Inhalt
        auto contentBounds = bounds.reduced(10, 5);
        contentBounds.removeFromLeft(5);  // Nach Farbstreifen
        
        // Kategorie und Frequenz
        g.setColour(theme.textColor);
        g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        
        juce::String freqStr;
        if (rec.frequency >= 1000.0f)
            freqStr = juce::String(rec.frequency / 1000.0f, 1) + " kHz";
        else
            freqStr = juce::String(static_cast<int>(rec.frequency)) + " Hz";
        
        g.drawText(SmartAnalyzer::getCategoryName(rec.sourceCategory) + " @ " + freqStr,
                  contentBounds.removeFromTop(18), juce::Justification::left);
        
        // Empfohlene Einstellungen
        g.setFont(11.0f);
        g.setColour(theme.textColor.withAlpha(0.7f));
        
        juce::String settingsStr;
        settingsStr << "Gain: " << juce::String(rec.gain, 1) << " dB, Q: " << juce::String(rec.q, 1);
        g.drawText(settingsStr, contentBounds.removeFromTop(14), juce::Justification::left);
        
        // Konfidenz-Balken
        auto confidenceBar = contentBounds.removeFromTop(8);
        confidenceBar.setWidth(100);
        
        g.setColour(theme.backgroundDark);
        g.fillRoundedRectangle(confidenceBar.toFloat(), 2.0f);
        
        g.setColour(categoryColour.withAlpha(0.8f));
        g.fillRoundedRectangle(static_cast<float>(confidenceBar.getX()), static_cast<float>(confidenceBar.getY()),
                              confidenceBar.getWidth() * rec.confidence,
                              static_cast<float>(confidenceBar.getHeight()), 2.0f);
        
        // Schweregrad-Badge
        juce::String severityStr = SmartAnalyzer::getSeverityName(rec.severity);
        juce::Colour severityColour = rec.severity == SmartAnalyzer::Severity::High ? juce::Colours::red :
                                      rec.severity == SmartAnalyzer::Severity::Medium ? juce::Colours::orange :
                                      juce::Colours::green;
        
        auto badgeBounds = juce::Rectangle<int>(bounds.getRight() - 55, bounds.getY() + 5, 50, 18);
        g.setColour(severityColour.withAlpha(0.2f));
        g.fillRoundedRectangle(badgeBounds.toFloat(), 3.0f);
        g.setColour(severityColour);
        g.drawRoundedRectangle(badgeBounds.toFloat(), 3.0f, 1.0f);
        g.setFont(10.0f);
        g.drawText(severityStr, badgeBounds, juce::Justification::centred);
        
        // Applied-Status
        if (rec.applied)
        {
            g.setColour(juce::Colours::green.withAlpha(0.3f));
            g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
            
            g.setColour(juce::Colours::green);
            g.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
            g.drawText("✓", bounds.removeFromRight(25), juce::Justification::centred);
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        // Klick auf Empfehlung erkennen
        if (!recommendationsArea.contains(e.getPosition()))
            return;
        
        const int itemHeight = 50;
        const int spacing = 5;
        // Scroll-Offset beruecksichtigen
        int relativeY = e.y - recommendationsArea.getY() + viewport.getViewPositionY();
        int clickedIndex = relativeY / (itemHeight + spacing);
        
        if (clickedIndex >= 0 && clickedIndex < static_cast<int>(recommendations.size()))
        {
            if (onApplyRecommendation)
            {
                onApplyRecommendation(clickedIndex);
                undoLastButton.setEnabled(true);  // Undo aktivieren nach Anwendung
            }
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmartRecommendationPanel)
};
