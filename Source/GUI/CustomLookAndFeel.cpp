#include "CustomLookAndFeel.h"
#include "ThemeManager.h"

juce::Colour CustomLookAndFeel::getBandColor(int bandIndex)
{
    auto& theme = ThemeManager::getInstance().getCurrentTheme();
    if (!theme.bandColors.empty())
        return theme.bandColors[static_cast<size_t>(bandIndex % theme.bandColors.size())];
    return juce::Colours::grey;
}

juce::Colour CustomLookAndFeel::getBackgroundDark() 
{ 
    return ThemeManager::getInstance().getCurrentTheme().backgroundDark; 
}

juce::Colour CustomLookAndFeel::getBackgroundMid() 
{ 
    return ThemeManager::getInstance().getCurrentTheme().backgroundMid; 
}

juce::Colour CustomLookAndFeel::getBackgroundLight() 
{ 
    return ThemeManager::getInstance().getCurrentTheme().backgroundLight; 
}

juce::Colour CustomLookAndFeel::getTextColor() 
{ 
    return ThemeManager::getInstance().getCurrentTheme().textColor; 
}

juce::Colour CustomLookAndFeel::getAccentColor() 
{ 
    return ThemeManager::getInstance().getCurrentTheme().accentColor; 
}

juce::Colour CustomLookAndFeel::getGridColor() 
{ 
    return ThemeManager::getInstance().getCurrentTheme().backgroundMid.brighter(0.2f); 
}

// Statische Variable für Spektrum-Farbschema
static CustomLookAndFeel::SpectrumColorScheme currentSpectrumScheme = CustomLookAndFeel::OrangeCyan;

void CustomLookAndFeel::setSpectrumColorScheme(SpectrumColorScheme scheme)
{
    currentSpectrumScheme = scheme;
}

CustomLookAndFeel::SpectrumColorScheme CustomLookAndFeel::getSpectrumColorScheme()
{
    return currentSpectrumScheme;
}

juce::String CustomLookAndFeel::getSpectrumColorSchemeName(SpectrumColorScheme scheme)
{
    switch (scheme)
    {
        case OrangeCyan:    return "Orange / Cyan";
        case GreenMagenta:  return "Green / Magenta";
        case YellowBlue:    return "Yellow / Blue";
        case RedTeal:       return "Red / Teal";
        case GrayAccent:    return "Gray / Accent";
        default:            return "Orange / Cyan";
    }
}

juce::Colour CustomLookAndFeel::getSpectrumColor() 
{ 
    switch (currentSpectrumScheme)
    {
        case OrangeCyan:    return juce::Colour(0xFFCC8844);  // Warmes Orange
        case GreenMagenta:  return juce::Colour(0xFF55BB55);  // Grün
        case YellowBlue:    return juce::Colour(0xFFDDCC44);  // Gelb
        case RedTeal:       return juce::Colour(0xFFCC5555);  // Rot
        case GrayAccent:    return juce::Colour(0xFF999999);  // Grau
        default:            return juce::Colour(0xFFCC8844);
    }
}

juce::Colour CustomLookAndFeel::getSpectrumColorPost() 
{ 
    switch (currentSpectrumScheme)
    {
        case OrangeCyan:    return juce::Colour(0xFF44CCDD);  // Cyan
        case GreenMagenta:  return juce::Colour(0xFFDD55CC);  // Magenta
        case YellowBlue:    return juce::Colour(0xFF5588EE);  // Blau
        case RedTeal:       return juce::Colour(0xFF44CCBB);  // Türkis
        case GrayAccent:
        {
            auto accentColor = ThemeManager::getInstance().getCurrentTheme().accentColor;
            return accentColor.withAlpha(1.0f);
        }
        default:            return juce::Colour(0xFF44CCDD);
    }
}

juce::Colour CustomLookAndFeel::getCurveColor() 
{ 
    return ThemeManager::getInstance().getCurrentTheme().curveColor; 
}

CustomLookAndFeel::CustomLookAndFeel()
{
    updateColors();
}

void CustomLookAndFeel::updateColors()
{
    auto& theme = ThemeManager::getInstance().getCurrentTheme();
    
    setColour(juce::ResizableWindow::backgroundColourId, theme.backgroundDark);
    setColour(juce::Label::textColourId, theme.textColor);
    setColour(juce::Slider::thumbColourId, theme.accentColor);
    setColour(juce::Slider::rotarySliderFillColourId, theme.accentColor);
    setColour(juce::Slider::trackColourId, theme.backgroundLight);
    setColour(juce::ComboBox::backgroundColourId, theme.backgroundMid);
    setColour(juce::ComboBox::textColourId, theme.textColor);
    setColour(juce::ComboBox::outlineColourId, theme.backgroundLight);
    setColour(juce::PopupMenu::backgroundColourId, theme.backgroundMid);
    setColour(juce::PopupMenu::textColourId, theme.textColor);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme.accentColor.withAlpha(0.3f));
    setColour(juce::TextButton::buttonColourId, theme.backgroundMid);
    setColour(juce::TextButton::textColourOnId, theme.textColor);
    setColour(juce::TextButton::textColourOffId, theme.textColor);
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& /*slider*/)
{
    const float radius = static_cast<float>(juce::jmin(width, height)) * 0.4f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour(getBackgroundMid());
    g.fillEllipse(rx, ry, rw, rw);

    g.setColour(getBackgroundLight());
    g.drawEllipse(rx, ry, rw, rw, 2.0f);

    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, radius - 4.0f, radius - 4.0f,
                           0.0f, rotaryStartAngle, angle, true);
    
    g.setColour(getAccentColor());
    g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    juce::Path pointer;
    const float pointerLength = radius * 0.6f;
    const float pointerThickness = 3.0f;
    
    pointer.addRectangle(-pointerThickness * 0.5f, -radius + 6.0f, pointerThickness, pointerLength);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    
    g.setColour(getAccentColor());
    g.fillPath(pointer);

    g.fillEllipse(centreX - 4.0f, centreY - 4.0f, 8.0f, 8.0f);
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                          juce::Slider::SliderStyle style, juce::Slider& /*slider*/)
{
    const bool isHorizontal = (style == juce::Slider::LinearHorizontal ||
                               style == juce::Slider::LinearBar);

    const float trackWidth = isHorizontal ? static_cast<float>(width) : static_cast<float>(height);
    const float trackHeight = 4.0f;

    juce::Rectangle<float> track;
    
    if (isHorizontal)
    {
        track = juce::Rectangle<float>(static_cast<float>(x), 
                                        static_cast<float>(y) + (static_cast<float>(height) - trackHeight) * 0.5f,
                                        trackWidth, trackHeight);
    }
    else
    {
        track = juce::Rectangle<float>(static_cast<float>(x) + (static_cast<float>(width) - trackHeight) * 0.5f,
                                        static_cast<float>(y),
                                        trackHeight, trackWidth);
    }

    g.setColour(getBackgroundLight());
    g.fillRoundedRectangle(track, 2.0f);

    juce::Rectangle<float> fill = track;
    if (isHorizontal)
        fill.setWidth(sliderPos - static_cast<float>(x));
    else
        fill.setTop(sliderPos);
    
    g.setColour(getAccentColor());
    g.fillRoundedRectangle(fill, 2.0f);

    const float thumbSize = 14.0f;
    juce::Rectangle<float> thumb;
    
    if (isHorizontal)
    {
        thumb = juce::Rectangle<float>(sliderPos - thumbSize * 0.5f,
                                        static_cast<float>(y) + (static_cast<float>(height) - thumbSize) * 0.5f,
                                        thumbSize, thumbSize);
    }
    else
    {
        thumb = juce::Rectangle<float>(static_cast<float>(x) + (static_cast<float>(width) - thumbSize) * 0.5f,
                                        sliderPos - thumbSize * 0.5f,
                                        thumbSize, thumbSize);
    }
    
    g.setColour(getTextColor());
    g.fillEllipse(thumb);
    g.setColour(getAccentColor());
    g.drawEllipse(thumb, 2.0f);
}

void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& /*backgroundColour*/,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    
    juce::Colour baseColour = getBackgroundMid();
    
    if (shouldDrawButtonAsDown)
        baseColour = getAccentColor().withAlpha(0.6f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = getBackgroundLight();
    
    if (button.getToggleState())
        baseColour = getAccentColor().withAlpha(0.4f);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    g.setColour(getBackgroundLight().brighter(0.1f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                      juce::ComboBox& /*box*/)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
    
    g.setColour(getBackgroundMid());
    g.fillRoundedRectangle(bounds, 4.0f);
    
    g.setColour(getBackgroundLight());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    const float arrowSize = 6.0f;
    const float arrowX = static_cast<float>(width) - 15.0f;
    const float arrowY = (static_cast<float>(height) - arrowSize * 0.5f) * 0.5f;
    
    juce::Path arrow;
    arrow.addTriangle(arrowX, arrowY,
                      arrowX + arrowSize, arrowY,
                      arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f);
    
    g.setColour(getTextColor());
    g.fillPath(arrow);
}

void CustomLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        g.setColour(getTextColor());
        g.setFont(getLabelFont(label));
        
        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / 
                                                         getLabelFont(label).getHeight())),
                         label.getMinimumHorizontalScale());
    }
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label& /*label*/)
{
    return juce::Font(juce::FontOptions(14.0f));
}

juce::Font CustomLookAndFeel::getComboBoxFont(juce::ComboBox& /*box*/)
{
    return juce::Font(juce::FontOptions(15.0f).withStyle("Bold"));
}

juce::Font CustomLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions(15.0f));
}
