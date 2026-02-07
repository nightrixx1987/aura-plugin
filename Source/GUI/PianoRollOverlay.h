#pragma once

#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

/**
 * PianoRollOverlay: Zeigt musikalische Noten-Markierungen über dem Analyzer
 * Inspiriert von FabFilter Pro-Q4
 * 
 * Features:
 * - Vertikale Linien für Noten C0–C10
 * - Noten-Labels (C, C#, D, etc.)
 * - Semi-transparentes Overlay
 * - Toggle On/Off
 */
class PianoRollOverlay : public juce::Component
{
public:
    PianoRollOverlay()
    {
        setInterceptsMouseClicks(false, false);
    }

    void setEnabled(bool shouldBeEnabled)
    {
        enabled = shouldBeEnabled;
        repaint();
    }

    bool isOverlayEnabled() const { return enabled; }

    void paint(juce::Graphics& g) override
    {
        if (!enabled)
            return;

        auto bounds = getLocalBounds().toFloat();
        float width = bounds.getWidth();
        float height = bounds.getHeight();

        // Noten-Frequenzen: C-Noten von C0 bis C10
        // C0 = 16.35 Hz, jede Oktave verdoppelt
        static const float cFrequencies[] = {
            16.35f,   // C0
            32.70f,   // C1
            65.41f,   // C2
            130.81f,  // C3
            261.63f,  // C4 (Middle C)
            523.25f,  // C5
            1046.50f, // C6
            2093.00f, // C7
            4186.01f, // C8
            8372.02f, // C9
            16744.0f  // C10
        };

        static const char* cNoteNames[] = {
            "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "C10"
        };

        // Alle chromatischen Noten zwischen sichtbaren C-Noten
        static const char* noteNames[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        // Zeichne C-Noten (prominenter)
        g.setFont(juce::Font(juce::FontOptions(9.0f)));

        for (int i = 0; i < 11; ++i)
        {
            float freq = cFrequencies[i];
            if (freq < 20.0f || freq > 20000.0f)
                continue;

            float x = frequencyToX(freq, width);

            // Vertikale Linie (C-Noten prominenter)
            if (i == 4)  // Middle C
            {
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawVerticalLine(static_cast<int>(x), 0.0f, height);
            }
            else
            {
                g.setColour(juce::Colours::white.withAlpha(0.12f));
                g.drawVerticalLine(static_cast<int>(x), 0.0f, height);
            }

            // Noten-Label am unteren Rand
            g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));
            g.drawText(cNoteNames[i], static_cast<int>(x) - 12, static_cast<int>(height) - 14, 24, 12,
                       juce::Justification::centred, false);
        }

        // Zeichne andere chromatische Noten (subtiler, nur im sichtbaren Bereich)
        g.setColour(juce::Colours::white.withAlpha(0.04f));

        for (int octave = 1; octave <= 9; ++octave)
        {
            for (int note = 1; note < 12; ++note)  // Überspringe C (schon gezeichnet)
            {
                float freq = cFrequencies[octave] * std::pow(2.0f, static_cast<float>(note) / 12.0f);
                if (freq < 20.0f || freq > 20000.0f)
                    continue;

                float x = frequencyToX(freq, width);
                g.drawVerticalLine(static_cast<int>(x), 0.0f, height);
            }
        }
    }

private:
    bool enabled = false;

    // Logarithmische Frequenz-zu-X-Konvertierung (20Hz – 20kHz)
    float frequencyToX(float frequency, float width) const
    {
        constexpr float minFreq = 20.0f;
        constexpr float maxFreq = 20000.0f;

        float normalized = std::log(frequency / minFreq) / std::log(maxFreq / minFreq);
        return normalized * width;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollOverlay)
};
