#pragma once

#include <JuceHeader.h>
#include "../DSP/SmartAnalyzer.h"
#include "ThemeManager.h"

/**
 * SmartHighlightOverlay: Visualisiert erkannte Frequenzprobleme als farbige Overlays.
 * 
 * Features:
 * - Semitransparente Bereiche für Problemfrequenzen
 * - Farbcodierung nach Problemkategorie
 * - Pulsierender Effekt für hohe Schweregrade
 * - Hover-Informationen
 * - Integration mit ThemeManager
 */
class SmartHighlightOverlay : public juce::Component,
                               public juce::Timer
{
public:
    //==========================================================================
    // Highlight-Darstellungsmodus
    //==========================================================================
    enum class DisplayMode
    {
        Regions,        // Farbige Regionen
        Bars,           // Vertikale Balken
        Gradient,       // Gradient-Overlay
        Subtle          // Dezente Markierungen nur am Rand
    };
    
    SmartHighlightOverlay()
    {
        setInterceptsMouseClicks(false, false);  // Grundsaetzlich transparent
        startTimerHz(30);
    }
    
    // hitTest: Nur true wenn ein Problem an dieser Position vorliegt
    // Dadurch gehen Klicks auf leere Bereiche zum Spektrum-Display durch (Band-Erstellung!)
    bool hitTest(int x, int /*y*/) override
    {
        if (!enabled || problems.empty())
            return false;
        
        float freq = xToFrequency(static_cast<float>(x));
        
        for (const auto& problem : problems)
        {
            float halfBandwidth = problem.bandwidth * 0.5f;
            if (freq >= problem.frequency - halfBandwidth &&
                freq <= problem.frequency + halfBandwidth)
            {
                return true;  // Problem gefunden -> Klick abfangen
            }
        }
        
        return false;  // Kein Problem -> Klick durchlassen
    }
    
    ~SmartHighlightOverlay() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        if (!enabled || problems.empty() || !showLabels)
            return;
        
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        
        for (const auto& problem : problems)
        {
            drawProblemHighlight(g, problem, width, height);
        }
        
        // Hover-Info zeichnen
        if (hoveredProblemIndex >= 0 && hoveredProblemIndex < static_cast<int>(problems.size()))
        {
            drawHoverInfo(g, problems[static_cast<size_t>(hoveredProblemIndex)]);
        }
    }
    
    void timerCallback() override
    {
        // Animation für pulsierende Highlights
        pulsePhase += 0.1f;
        if (pulsePhase > juce::MathConstants<float>::twoPi)
            pulsePhase -= juce::MathConstants<float>::twoPi;
        
        if (enabled && !problems.empty())
            repaint();
    }
    
    //==========================================================================
    // Probleme aktualisieren
    //==========================================================================
    void updateProblems(const std::vector<SmartAnalyzer::FrequencyProblem>& newProblems)
    {
        problems = newProblems;
        repaint();
    }
    
    void clearProblems()
    {
        problems.clear();
        hoveredProblemIndex = -1;
        repaint();
    }
    
    //==========================================================================
    // Frequenz-zu-X-Konvertierung (muss von Parent gesetzt werden)
    //==========================================================================
    void setFrequencyRange(float minFreq, float maxFreq)
    {
        minFrequency = minFreq;
        maxFrequency = maxFreq;
    }
    
    //==========================================================================
    // Einstellungen
    //==========================================================================
    void setEnabled(bool shouldBeEnabled)
    {
        enabled = shouldBeEnabled;
        repaint();
    }
    
    bool isOverlayEnabled() const { return enabled; }
    
    void setDisplayMode(DisplayMode mode)
    {
        displayMode = mode;
        repaint();
    }
    
    DisplayMode getDisplayMode() const { return displayMode; }
    
    void setOpacity(float newOpacity)
    {
        opacity = juce::jlimit(0.0f, 1.0f, newOpacity);
        repaint();
    }
    
    float getOpacity() const { return opacity; }
    
    void setShowLabels(bool show)
    {
        showLabels = show;
        repaint();
    }
    
    bool getShowLabels() const { return showLabels; }
    
    void setPulseEnabled(bool shouldPulse)
    {
        pulseEnabled = shouldPulse;
    }
    
    //==========================================================================
    // Mouse-Interaktion für Hover
    //==========================================================================
    void mouseMove(const juce::MouseEvent& e) override
    {
        updateHoveredProblem(e.position);
    }
    
    void mouseExit(const juce::MouseEvent&) override
    {
        hoveredProblemIndex = -1;
        repaint();
    }
    
    //==========================================================================
    // Problem bei Position finden
    //==========================================================================
    const SmartAnalyzer::FrequencyProblem* getProblemAtPosition(juce::Point<float> pos) const
    {
        float freq = xToFrequency(pos.x);
        
        for (const auto& problem : problems)
        {
            float halfBandwidth = problem.bandwidth * 0.5f;
            if (freq >= problem.frequency - halfBandwidth &&
                freq <= problem.frequency + halfBandwidth)
            {
                return &problem;
            }
        }
        
        return nullptr;
    }
    
    //==========================================================================
    // Callback für Klick auf Problem
    //==========================================================================
    std::function<void(const SmartAnalyzer::FrequencyProblem&)> onProblemClicked;
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (auto* problem = getProblemAtPosition(e.position.toFloat()))
        {
            if (onProblemClicked)
                onProblemClicked(*problem);
        }
    }
    
private:
    std::vector<SmartAnalyzer::FrequencyProblem> problems;
    int hoveredProblemIndex = -1;  // FIX: Index statt Pointer (Dangling Pointer Prevention)
    
    bool enabled = true;
    DisplayMode displayMode = DisplayMode::Regions;
    float opacity = 0.25f;  // Dezenter Standard (war 0.3f)
    bool showLabels = true;
    bool pulseEnabled = true;
    float pulsePhase = 0.0f;
    
    float minFrequency = 20.0f;
    float maxFrequency = 20000.0f;
    
    //==========================================================================
    // Koordinaten-Konvertierung
    //==========================================================================
    float frequencyToX(float frequency) const
    {
        if (frequency <= 0.0f || minFrequency <= 0.0f)
            return 0.0f;
        
        float logMin = std::log10(minFrequency);
        float logMax = std::log10(maxFrequency);
        float logFreq = std::log10(frequency);
        
        return ((logFreq - logMin) / (logMax - logMin)) * static_cast<float>(getWidth());
    }
    
    float xToFrequency(float x) const
    {
        float logMin = std::log10(minFrequency);
        float logMax = std::log10(maxFrequency);
        float normalizedX = x / static_cast<float>(getWidth());
        
        return std::pow(10.0f, logMin + normalizedX * (logMax - logMin));
    }
    
    //==========================================================================
    // Highlight zeichnen
    //==========================================================================
    void drawProblemHighlight(juce::Graphics& g, const SmartAnalyzer::FrequencyProblem& problem,
                              float /*width*/, float height)
    {
        // Position berechnen
        float centerX = frequencyToX(problem.frequency);
        
        // Visuelle Bandbreite berechnen - SCHMALER als die echte Bandbreite
        // Für Resonanzen: echte Bandbreite, für breitbandige Probleme: schmaler darstellen
        float visualBandwidth = problem.bandwidth;
        
        // Breitbandige Kategorien visuell schmaler darstellen
        if (problem.category == SmartAnalyzer::ProblemCategory::Harshness ||
            problem.category == SmartAnalyzer::ProblemCategory::Mud ||
            problem.category == SmartAnalyzer::ProblemCategory::Boxiness)
        {
            // Maximal 1/3 Oktave visuell
            visualBandwidth = problem.frequency * 0.25f;
        }
        
        float halfBandwidthX = (frequencyToX(problem.frequency + visualBandwidth * 0.5f) -
                               frequencyToX(problem.frequency - visualBandwidth * 0.5f)) * 0.5f;
        halfBandwidthX = juce::jlimit(8.0f, 80.0f, halfBandwidthX);  // Min 8px, Max 80px
        
        float left = centerX - halfBandwidthX;
        float right = centerX + halfBandwidthX;
        
        // Farbe mit Opacity und optionalem Pulsieren
        juce::Colour baseColour = SmartAnalyzer::getColourForCategory(problem.category);
        float finalOpacity = opacity * problem.confidence;
        
        if (pulseEnabled && problem.severity == SmartAnalyzer::Severity::High)
        {
            float pulse = 0.5f + 0.5f * std::sin(pulsePhase);
            finalOpacity *= (0.7f + 0.3f * pulse);
        }
        
        juce::Colour colour = baseColour.withAlpha(finalOpacity);
        
        switch (displayMode)
        {
            case DisplayMode::Regions:
                drawRegion(g, left, right, height, colour, problem);
                break;
                
            case DisplayMode::Bars:
                drawBar(g, centerX, height, colour, problem);
                break;
                
            case DisplayMode::Gradient:
                drawGradient(g, centerX, halfBandwidthX, height, colour);
                break;
                
            case DisplayMode::Subtle:
                drawSubtle(g, centerX, height, colour, problem);
                break;
        }
        
        // Label zeichnen
        if (showLabels && problem.severity >= SmartAnalyzer::Severity::Medium)
        {
            drawProblemLabel(g, centerX, problem);
        }
    }
    
    void drawRegion(juce::Graphics& g, float left, float right, float height,
                    juce::Colour colour, const SmartAnalyzer::FrequencyProblem& problem)
    {
        float centerX = (left + right) * 0.5f;
        float regionWidth = right - left;
        
        // Höhe basierend auf Schweregrad - kompakter
        float regionHeight = height * 0.12f;
        if (problem.severity == SmartAnalyzer::Severity::High)
            regionHeight = height * 0.18f;
        else if (problem.severity == SmartAnalyzer::Severity::Low)
            regionHeight = height * 0.08f;
        
        // === GLOW-EFFEKT für hohe Schweregrade ===
        if (problem.severity == SmartAnalyzer::Severity::High)
        {
            // Äußerer Glow (sehr transparent, breit)
            for (int i = 3; i >= 0; i--)
            {
                float glowAlpha = 0.05f * (4 - i);
                float glowWidth = regionWidth + i * 12.0f;
                
                juce::ColourGradient outerGlow(
                    colour.withAlpha(glowAlpha), centerX, 0.0f,
                    colour.withAlpha(0.0f), centerX, regionHeight + i * 10.0f,
                    false);
                g.setGradientFill(outerGlow);
                g.fillRect(centerX - glowWidth * 0.5f, 0.0f, glowWidth, regionHeight + i * 10.0f);
            }
        }
        
        // Hauptgradient - schöner und weicher
        juce::ColourGradient gradient(
            colour.withAlpha(opacity * 0.8f), centerX, 0.0f,
            colour.withAlpha(0.0f), centerX, regionHeight * 1.5f,
            false);
        g.setGradientFill(gradient);
        
        // Abgerundete Region
        juce::Path regionPath;
        regionPath.addRoundedRectangle(left, 0.0f, regionWidth, regionHeight, 4.0f);
        g.fillPath(regionPath);
        
        // Elegante vertikale Linie in der Mitte
        float lineAlpha = std::min(0.6f, opacity * 2.0f);
        
        // Linie mit Gradient (oben stark, unten ausfadend)
        juce::ColourGradient lineGradient(
            colour.withAlpha(lineAlpha), 0, 0,
            colour.withAlpha(0.0f), 0, height * 0.5f,
            false);
        g.setGradientFill(lineGradient);
        g.fillRect(centerX - 1.0f, 0.0f, 2.0f, height * 0.5f);
        
        // Kleiner eleganter Punkt oben
        g.setColour(colour.withAlpha(std::min(1.0f, opacity * 2.5f)));
        g.fillEllipse(centerX - 4.0f, 2.0f, 8.0f, 8.0f);
    }
    
    void drawBar(juce::Graphics& g, float centerX, float height,
                 juce::Colour colour, const SmartAnalyzer::FrequencyProblem& problem)
    {
        float barWidth = problem.severity == SmartAnalyzer::Severity::High ? 4.0f :
                        problem.severity == SmartAnalyzer::Severity::Medium ? 3.0f : 2.0f;
        
        g.setColour(colour);
        g.fillRect(centerX - barWidth * 0.5f, 0.0f, barWidth, height);
    }
    
    void drawGradient(juce::Graphics& g, float centerX, float halfWidth, float height,
                      juce::Colour colour)
    {
        juce::ColourGradient gradient(
            colour, centerX, 0.0f,
            colour.withAlpha(0.0f), centerX + halfWidth, 0.0f,
            false);
        
        g.setGradientFill(gradient);
        g.fillRect(centerX, 0.0f, halfWidth, height);
        
        juce::ColourGradient gradientLeft(
            colour.withAlpha(0.0f), centerX - halfWidth, 0.0f,
            colour, centerX, 0.0f,
            false);
        
        g.setGradientFill(gradientLeft);
        g.fillRect(centerX - halfWidth, 0.0f, halfWidth, height);
    }
    
    void drawSubtle(juce::Graphics& g, float centerX, float height,
                    juce::Colour colour, const SmartAnalyzer::FrequencyProblem& /*problem*/)
    {
        // Nur oben und unten Markierungen
        float markerHeight = 15.0f;
        float markerWidth = 8.0f;
        
        g.setColour(colour.withAlpha(opacity * 1.5f));
        
        // Oben
        juce::Path topMarker;
        topMarker.addTriangle(centerX - markerWidth * 0.5f, 0.0f,
                             centerX + markerWidth * 0.5f, 0.0f,
                             centerX, markerHeight);
        g.fillPath(topMarker);
        
        // Unten
        juce::Path bottomMarker;
        bottomMarker.addTriangle(centerX - markerWidth * 0.5f, height,
                                centerX + markerWidth * 0.5f, height,
                                centerX, height - markerHeight);
        g.fillPath(bottomMarker);
    }
    
    void drawProblemLabel(juce::Graphics& g, float centerX, const SmartAnalyzer::FrequencyProblem& problem)
    {
        juce::String label = SmartAnalyzer::getCategoryName(problem.category);
        
        g.setFont(10.0f);  // Etwas kleiner
        
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(g.getCurrentFont(), label, 0.0f, 0.0f);
        float labelWidth = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth() + 12.0f;
        float labelHeight = 18.0f;
        float labelX = centerX - labelWidth * 0.5f;
        float labelY = 14.0f;
        
        // Clamp to screen bounds
        labelX = juce::jlimit(2.0f, static_cast<float>(getWidth()) - labelWidth - 2.0f, labelX);
        
        // Schatten für Tiefe
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillRoundedRectangle(labelX + 1.5f, labelY + 1.5f, labelWidth, labelHeight, 4.0f);
        
        // Hintergrund mit Gradient
        juce::Colour bgColour = SmartAnalyzer::getColourForCategory(problem.category);
        juce::ColourGradient labelGradient(
            bgColour.brighter(0.1f), labelX, labelY,
            bgColour.darker(0.2f), labelX, labelY + labelHeight,
            false);
        g.setGradientFill(labelGradient);
        g.fillRoundedRectangle(labelX, labelY, labelWidth, labelHeight, 4.0f);
        
        // Feiner Rahmen
        g.setColour(bgColour.brighter(0.3f).withAlpha(0.5f));
        g.drawRoundedRectangle(labelX, labelY, labelWidth, labelHeight, 4.0f, 1.0f);
        
        // Text mit leichtem Schatten
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawText(label, static_cast<int>(labelX + 1), static_cast<int>(labelY + 1),
                   static_cast<int>(labelWidth), static_cast<int>(labelHeight),
                   juce::Justification::centred);
        
        g.setColour(juce::Colours::white);
        g.drawText(label, static_cast<int>(labelX), static_cast<int>(labelY),
                   static_cast<int>(labelWidth), static_cast<int>(labelHeight),
                   juce::Justification::centred);
    }
    
    //==========================================================================
    // Hover-Info
    //==========================================================================
    void updateHoveredProblem(juce::Point<float> pos)
    {
        int newIndex = getProblemIndexAtPosition(pos);
        
        if (newIndex != hoveredProblemIndex)
        {
            hoveredProblemIndex = newIndex;
            repaint();
        }
    }
    
    int getProblemIndexAtPosition(juce::Point<float> pos) const
    {
        float freq = xToFrequency(pos.x);
        
        for (int i = 0; i < static_cast<int>(problems.size()); ++i)
        {
            const auto& problem = problems[static_cast<size_t>(i)];
            float halfBandwidth = problem.bandwidth * 0.5f;
            if (freq >= problem.frequency - halfBandwidth &&
                freq <= problem.frequency + halfBandwidth)
            {
                return i;
            }
        }
        
        return -1;
    }
    
    void drawHoverInfo(juce::Graphics& g, const SmartAnalyzer::FrequencyProblem& problem)
    {
        float centerX = frequencyToX(problem.frequency);
        
        juce::String info;
        info << SmartAnalyzer::getCategoryName(problem.category) << "\n";
        
        if (problem.frequency >= 1000.0f)
            info << juce::String(problem.frequency / 1000.0f, 2) << " kHz\n";
        else
            info << juce::String(static_cast<int>(problem.frequency)) << " Hz\n";
        
        info << "Empfohlen: " << juce::String(problem.suggestedGain, 1) << " dB, Q=" 
             << juce::String(problem.suggestedQ, 1);
        
        auto& theme = ThemeManager::getInstance().getCurrentTheme();
        g.setFont(12.0f);
        
        float boxWidth = 140.0f;
        float boxHeight = 55.0f;
        float boxX = juce::jlimit(5.0f, static_cast<float>(getWidth()) - boxWidth - 5.0f, 
                                  centerX - boxWidth * 0.5f);
        float boxY = 45.0f;
        
        // Hintergrund
        g.setColour(theme.backgroundMid.withAlpha(0.95f));
        g.fillRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 5.0f);
        
        // Rahmen in Problemfarbe
        g.setColour(SmartAnalyzer::getColourForCategory(problem.category));
        g.drawRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 5.0f, 1.5f);
        
        // Text
        g.setColour(theme.textColor);
        g.drawFittedText(info, static_cast<int>(boxX + 5), static_cast<int>(boxY + 5),
                        static_cast<int>(boxWidth - 10), static_cast<int>(boxHeight - 10),
                        juce::Justification::centredLeft, 3);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmartHighlightOverlay)
};
