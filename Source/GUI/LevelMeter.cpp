#include "LevelMeter.h"

LevelMeter::LevelMeter()
{
    startTimerHz(30);  // 30 FPS - Kompromiss zwischen Fluidität und Performance
}

LevelMeter::~LevelMeter()
{
    stopTimer();
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const int width = bounds.getWidth();
    const int height = bounds.getHeight();
    
    if (width <= 0 || height <= 0)
        return;
    
    // Sehr dunkler Hintergrund (Pro-Q4 Style)
    g.fillAll(juce::Colour(0xff0d0d0d));
    
    // Subtiler innerer Bereich mit Schatten-Effekt
    auto meterBounds = bounds.reduced(2, 0);
    g.setColour(juce::Colour(0xff000000));
    g.fillRect(meterBounds);
    
    // Meter-Bereich (vertikale Anzeige)
    const int meterHeight = bounds.getHeight();
    const int meterWidth = bounds.getWidth();
    
    // Teile in L/R Kanäle (mit 2px Abstand in der Mitte)
    const int channelGap = 2;
    const int channelWidth = (meterWidth - channelGap - 4) / 2;  // -4 für padding
    
    // Header-Bereiche für L/R (oben)
    auto leftHeaderArea = juce::Rectangle<int>(2, 0, channelWidth, 22);
    auto rightHeaderArea = juce::Rectangle<int>(2 + channelWidth + channelGap, 0, channelWidth, 22);
    
    // Meter-Bereiche für L/R (darunter)
    auto leftChannelBounds = juce::Rectangle<int>(2, 22, channelWidth, meterHeight - 22);
    auto rightChannelBounds = juce::Rectangle<int>(2 + channelWidth + channelGap, 22, channelWidth, meterHeight - 22);
    
    // ===== LINKER KANAL ZEICHNEN =====
    drawChannel(g, leftChannelBounds, currentLevelLeftDB, peakLevelLeftDB);
    
    // ===== RECHTER KANAL ZEICHNEN =====
    drawChannel(g, rightChannelBounds, currentLevelRightDB, peakLevelRightDB);
    
    // ===== NUMERISCHE ANZEIGE OBEN =====
    // Linker Kanal
    juce::Colour leftDisplayColor = getColorForLevel(currentLevelLeftDB);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRect(leftHeaderArea);
    
    g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    juce::String leftLevelText = formatLevelText(currentLevelLeftDB);
    g.setColour(leftDisplayColor);
    g.drawText(leftLevelText, leftHeaderArea, juce::Justification::centred, true);
    
    // Rechter Kanal
    juce::Colour rightDisplayColor = getColorForLevel(currentLevelRightDB);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRect(rightHeaderArea);
    
    g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    juce::String rightLevelText = formatLevelText(currentLevelRightDB);
    g.setColour(rightDisplayColor);
    g.drawText(rightLevelText, rightHeaderArea, juce::Justification::centred, true);
    
    // Äußerer Rahmen
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRect(bounds, 1);
}

void LevelMeter::drawChannel(juce::Graphics& g, juce::Rectangle<int> channelBounds, float levelDB, float peakDB)
{
    const int meterHeight = channelBounds.getHeight();
    const int meterWidth = channelBounds.getWidth();
    
    // ===== LED-SEGMENTE ZEICHNEN (von unten nach oben) =====
    const int segmentHeight = 3;  // Höhe jedes LED-Segments
    const int segmentGap = 1;     // Abstand zwischen Segmenten
    const int totalSegmentHeight = segmentHeight + segmentGap;
    
    float normalizedLevel = juce::jlimit(0.0f, 1.0f, 
        (levelDB - MIN_DB) / (MAX_DB - MIN_DB));
    
    int fillPixels = static_cast<int>(normalizedLevel * meterHeight);
    
    // Segment-Loop: Von unten nach oben zeichnen
    for (int y = meterHeight; y > 0; y -= totalSegmentHeight)
    {
        int segmentY = channelBounds.getY() + y - segmentHeight;
        int pixelsFromBottom = meterHeight - y;
        
        // Ist dieses Segment aktiv (gefüllt)?
        bool isActive = pixelsFromBottom < fillPixels;
        
        if (isActive)
        {
            // Farbe basierend auf Position bestimmen
            float segmentNormalized = static_cast<float>(pixelsFromBottom) / meterHeight;
            float segmentDB = MIN_DB + segmentNormalized * (MAX_DB - MIN_DB);
            
            juce::Colour segmentColor;
            
            // Farbverlauf mit mehr Nuancen
            if (segmentDB >= 3.0f)  // Rot (Clipping Zone)
                segmentColor = juce::Colour(0xffff3333);
            else if (segmentDB >= 0.0f)  // Orange/Rot
                segmentColor = juce::Colour::fromHSV(0.05f, 0.85f, 1.0f, 1.0f);
            else if (segmentDB >= -3.0f)  // Orange
                segmentColor = juce::Colour(0xffff9933);
            else if (segmentDB >= -6.0f)  // Gelb-Orange
                segmentColor = juce::Colour(0xffffcc33);
            else if (segmentDB >= -12.0f) // Gelb
                segmentColor = juce::Colour(0xffffee44);
            else if (segmentDB >= -18.0f) // Gelb-Grün
                segmentColor = juce::Colour(0xffccff44);
            else if (segmentDB >= -24.0f) // Hellgrün
                segmentColor = juce::Colour(0xff99ff55);
            else  // Dunkelgrün
                segmentColor = juce::Colour(0xff55dd55);
            
            // Segment mit Gradient-Effekt zeichnen
            juce::ColourGradient gradient(
                segmentColor.brighter(0.15f), 
                static_cast<float>(channelBounds.getX()), 
                static_cast<float>(segmentY),
                segmentColor.darker(0.1f), 
                static_cast<float>(channelBounds.getRight()), 
                static_cast<float>(segmentY),
                false
            );
            
            g.setGradientFill(gradient);
            g.fillRect(channelBounds.getX() + 1, segmentY, meterWidth - 2, segmentHeight);
        }
        else
        {
            // Inaktives Segment (sehr dunkel)
            g.setColour(juce::Colour(0xff1a1a1a));
            g.fillRect(channelBounds.getX() + 1, segmentY, meterWidth - 2, segmentHeight);
        }
    }
    
    // ===== PEAK-HOLD LINIE =====
    if (peakDB > MIN_DB + 5.0f)
    {
        float normalizedPeak = juce::jlimit(0.0f, 1.0f, 
            (peakDB - MIN_DB) / (MAX_DB - MIN_DB));
        int peakY = channelBounds.getY() + meterHeight - static_cast<int>(normalizedPeak * meterHeight);
        
        // Weiße Peak-Linie mit Glow-Effekt
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillRect(channelBounds.getX() + 1, peakY - 1, meterWidth - 2, 2);
    }
    
    // ===== dB-SCALE MARKIERUNGEN (subtil) =====
    g.setColour(juce::Colour(0xff2a2a2a));
    
    // Wichtige dB-Werte
    float dbValues[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f };
    for (float db : dbValues)
    {
        float normalized = (db - MIN_DB) / (MAX_DB - MIN_DB);
        int y = channelBounds.getY() + meterHeight - static_cast<int>(normalized * meterHeight);
        
        // Horizontale Linie (sehr subtil)
        g.drawLine(static_cast<float>(channelBounds.getX()), 
                   static_cast<float>(y), 
                   static_cast<float>(channelBounds.getRight()), 
                   static_cast<float>(y), 
                   0.5f);
    }
}

juce::String LevelMeter::formatLevelText(float levelDB)
{
    if (levelDB < MIN_DB + 5.0f)
        return "-∞";
    else if (levelDB >= 0.0f)
        return "+" + juce::String(levelDB, 1);
    else
        return juce::String(levelDB, 1);
}

void LevelMeter::resized()
{
    // Keine speziellen Resize-Operationen erforderlich
}

void LevelMeter::timerCallback()
{
    // Peak Hold Time verringern - Linker Kanal
    if (peakHoldTimeRemainingLeft > 0.0)
    {
        peakHoldTimeRemainingLeft -= (1.0 / 60.0);
    }
    else
    {
        // Peak langsam abnehmen (Decay)
        float decayPerFrame = decayRate / 60.0f;
        peakLevelLeftDB -= decayPerFrame;
        
        if (peakLevelLeftDB < currentLevelLeftDB - 5.0f)
            peakLevelLeftDB = currentLevelLeftDB - 5.0f;
    }
    
    // Peak Hold Time verringern - Rechter Kanal
    if (peakHoldTimeRemainingRight > 0.0)
    {
        peakHoldTimeRemainingRight -= (1.0 / 60.0);
    }
    else
    {
        // Peak langsam abnehmen (Decay)
        float decayPerFrame = decayRate / 60.0f;
        peakLevelRightDB -= decayPerFrame;
        
        if (peakLevelRightDB < currentLevelRightDB - 5.0f)
            peakLevelRightDB = currentLevelRightDB - 5.0f;
    }
    
    repaint();
}

void LevelMeter::setLevel(float leftDB, float rightDB)
{
    // Exponentielles Smoothing für visuellen Pegel - Linker Kanal
    float alphaLeft = (leftDB > currentLevelLeftDB) ? 0.6f : 0.08f;  // Schneller Attack, langsamer Release
    currentLevelLeftDB = alphaLeft * leftDB + (1.0f - alphaLeft) * currentLevelLeftDB;
    
    // Peak aktualisieren - Links
    if (currentLevelLeftDB > peakLevelLeftDB)
    {
        peakLevelLeftDB = currentLevelLeftDB;
        peakHoldTimeRemainingLeft = peakHoldDuration;
    }
    
    // Exponentielles Smoothing für visuellen Pegel - Rechter Kanal
    float alphaRight = (rightDB > currentLevelRightDB) ? 0.6f : 0.08f;
    currentLevelRightDB = alphaRight * rightDB + (1.0f - alphaRight) * currentLevelRightDB;
    
    // Peak aktualisieren - Rechts
    if (currentLevelRightDB > peakLevelRightDB)
    {
        peakLevelRightDB = currentLevelRightDB;
        peakHoldTimeRemainingRight = peakHoldDuration;
    }
}

void LevelMeter::updateLevelFromBuffer(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() <= 0)
        return;
    
    // Separate RMS-Berechnung für jeden Kanal
    float leftRMS = 0.0f;
    float rightRMS = 0.0f;
    
    if (buffer.getNumChannels() >= 1)
    {
        leftRMS = calculateRMS(buffer.getReadPointer(0), buffer.getNumSamples());
    }
    
    if (buffer.getNumChannels() >= 2)
    {
        rightRMS = calculateRMS(buffer.getReadPointer(1), buffer.getNumSamples());
    }
    else
    {
        // Mono: Beide Kanäle zeigen dasselbe
        rightRMS = leftRMS;
    }
    
    // In dB umwandeln
    float leftDB = (leftRMS < 1e-10f) ? -80.0f : 20.0f * std::log10(leftRMS);
    float rightDB = (rightRMS < 1e-10f) ? -80.0f : 20.0f * std::log10(rightRMS);
    
    // Pegel setzen
    setLevel(leftDB, rightDB);
}

float LevelMeter::calculateRMS(const float* data, int numSamples)
{
    if (numSamples <= 0)
        return 0.0f;
    
    double rmsSum = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        double sample = data[i];
        rmsSum += sample * sample;
    }
    
    return static_cast<float>(std::sqrt(rmsSum / numSamples));
}

juce::Colour LevelMeter::getColorForLevel(float levelDB)
{
    if (levelDB >= RED_LIMIT)
        return colorRed;          // Rot (Clipping)
    else if (levelDB >= ORANGE_LIMIT)
        return colorOrange;       // Orange
    else if (levelDB >= YELLOW_LIMIT)
        return colorYellow;       // Gelb
    else if (levelDB >= GREEN_LIMIT)
        return colorGreen;        // Grün
    else
        return colorGreen;        // Grün (unter Minimum)
}

int LevelMeter::dbToPixels(float levelDB) const
{
    // Konvertiert dB-Wert zu Pixel-Höhe
    float normalized = (levelDB - MIN_DB) / (MAX_DB - MIN_DB);
    return static_cast<int>(normalized * (getHeight() - 22));
}
