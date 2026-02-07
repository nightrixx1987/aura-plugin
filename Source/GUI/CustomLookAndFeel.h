#pragma once

#include <JuceHeader.h>

/**
 * CustomLookAndFeel: Modernes Design im FabFilter-Stil
 */
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override = default;

    // Theme-System
    void updateColors();
    
    // Farb-Accessoren (holen Farben vom aktuellen Theme)
    static juce::Colour getBackgroundDark();
    static juce::Colour getBackgroundMid();
    static juce::Colour getBackgroundLight();
    static juce::Colour getTextColor();
    static juce::Colour getAccentColor();
    static juce::Colour getGridColor();
    static juce::Colour getSpectrumColor();
    static juce::Colour getSpectrumColorPost();
    static juce::Colour getCurveColor();
    
    // Spektrum-Farbschema (User-wählbar)
    enum SpectrumColorScheme
    {
        OrangeCyan = 0,   // Orange (Pre) + Cyan (Post)
        GreenMagenta,     // Grün (Pre) + Magenta (Post)
        YellowBlue,       // Gelb (Pre) + Blau (Post)
        RedTeal,          // Rot (Pre) + Türkis (Post)
        GrayAccent,       // Grau (Pre) + Theme-Accent (Post)
        NumSchemes
    };
    
    static void setSpectrumColorScheme(SpectrumColorScheme scheme);
    static SpectrumColorScheme getSpectrumColorScheme();
    static juce::String getSpectrumColorSchemeName(SpectrumColorScheme scheme);
    
    // Band-Farben (für verschiedene EQ-Bänder)
    static juce::Colour getBandColor(int bandIndex);

    // Slider-Design
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // Button-Design
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    // ComboBox-Design
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    // Label-Design
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    // Fonts
    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeel)
};
