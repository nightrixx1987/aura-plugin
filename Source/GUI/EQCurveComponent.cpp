#include "EQCurveComponent.h"

EQCurveComponent::EQCurveComponent()
{
    setOpaque(false);
    // Timer wird NICHT im Konstruktor gestartet!
    // Erst nach vollständiger Initialisierung via startCurveUpdates()
    
    // Band-Handles initialisieren
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        bandHandles[static_cast<size_t>(i)].bandIndex = i;
        bandHandles[static_cast<size_t>(i)].frequency = ParameterIDs::DEFAULT_FREQUENCIES[i];
        bandHandles[static_cast<size_t>(i)].gain = 0.0f;
        bandHandles[static_cast<size_t>(i)].q = ParameterIDs::DEFAULT_Q;
        bandHandles[static_cast<size_t>(i)].type = ParameterIDs::DEFAULT_TYPES[i];
        bandHandles[static_cast<size_t>(i)].active = false;
    }
}

EQCurveComponent::~EQCurveComponent()
{
    stopTimer();
}

void EQCurveComponent::setEQProcessor(EQProcessor* processor)
{
    eqProcessor = processor;
    updateCurvePath();
    repaint();
}

void EQCurveComponent::setBandParameters(int bandIndex, float frequency, float gain, float q,
                                          ParameterIDs::FilterType type, bool bypassed, bool active)
{
    if (bandIndex < 0 || bandIndex >= ParameterIDs::MAX_BANDS)
        return;

    auto& handle = bandHandles[static_cast<size_t>(bandIndex)];
    handle.frequency = frequency;
    handle.gain = gain;
    handle.q = q;
    handle.type = type;
    handle.bypassed = bypassed;
    handle.active = active;
    
    // Position aktualisieren
    handle.x = frequencyToX(frequency);
    handle.y = dbToY(gain);
    curvesDirty = true;
}

void EQCurveComponent::addListener(Listener* listener)
{
    listeners.add(listener);
}

void EQCurveComponent::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void EQCurveComponent::setSelectedBand(int bandIndex)
{
    if (selectedBand != bandIndex)
    {
        selectedBand = bandIndex;
        repaint();
        notifyBandSelected(bandIndex);
    }
}

juce::Point<int> EQCurveComponent::getBandScreenPosition(int bandIndex) const
{
    if (bandIndex < 0 || bandIndex >= static_cast<int>(bandHandles.size()))
        return juce::Point<int>(getWidth() / 2, getHeight() / 2);
    
    const auto& handle = bandHandles[static_cast<size_t>(bandIndex)];
    
    float x = frequencyToX(handle.frequency);
    float y = dbToY(handle.gain);
    
    // Bei Dynamic EQ: Position zum effektiven Gain verschieben
    if (eqProcessor != nullptr && !handle.bypassed)
    {
        const auto& band = eqProcessor->getBand(bandIndex);
        if (band.isDynamicMode())
        {
            float gr = band.getDynamicGainReduction();
            if (gr > 0.05f)
                y = dbToY(calcEffectiveGain(handle.gain, gr));
        }
    }
    
    return juce::Point<int>(static_cast<int>(x), static_cast<int>(y));
}

float EQCurveComponent::frequencyToX(float frequency) const
{
    if (frequency <= 0.0f || getWidth() <= 0)
        return 0.0f;
    
    float normalized = std::log(frequency / minFreq) / std::log(maxFreq / minFreq);
    return normalized * static_cast<float>(getWidth());
}

float EQCurveComponent::xToFrequency(float x) const
{
    if (getWidth() <= 0)
        return minFreq;
    
    float normalized = x / static_cast<float>(getWidth());
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return minFreq * std::pow(maxFreq / minFreq, normalized);
}

float EQCurveComponent::dbToY(float db) const
{
    float normalized = (db - minDB) / (maxDB - minDB);
    return static_cast<float>(getHeight()) * (1.0f - normalized);
}

float EQCurveComponent::yToDb(float y) const
{
    if (getHeight() <= 0)
        return 0.0f;

    float normalized = 1.0f - (y / static_cast<float>(getHeight()));
    return minDB + normalized * (maxDB - minDB);
}

void EQCurveComponent::setEQDecibelRange(float newMinDB, float newMaxDB)
{
    minDB = newMinDB;
    maxDB = newMaxDB;

    // KRITISCH: Alle Handle-Y-Positionen neu berechnen basierend auf neuem Scale!
    // Sonst werden alte Y-Positionen mit neuer Skala falsch interpretiert
    for (auto& handle : bandHandles)
    {
        if (handle.active)
        {
            handle.y = dbToY(handle.gain);
        }
    }

    repaint();
}

void EQCurveComponent::paint(juce::Graphics& g)
{
    // Kurven für einzelne Bänder zeichnen
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        if (bandHandles[static_cast<size_t>(i)].active && !bandHandles[static_cast<size_t>(i)].bypassed)
        {
            drawBandCurve(g, i);
        }
    }

    // Dynamic EQ: Reduzierte Kurven fuer Bands mit aktiver Gain Reduction
    drawDynamicEQCurves(g);

    // Gesamt-Kurve zeichnen
    drawCurve(g);

    // Band-Handles zeichnen
    drawBandHandles(g);
}

void EQCurveComponent::resized()
{
    // Band-Positionen aktualisieren
    for (auto& handle : bandHandles)
    {
        handle.x = frequencyToX(handle.frequency);
        handle.y = dbToY(handle.gain);
    }
    
    // Frequenz-Tabelle vorberechnen (1 Eintrag pro Pixel)
    const int w = getWidth();
    freqTable.resize(static_cast<size_t>(w > 0 ? w : 1));
    for (int i = 0; i < w; ++i)
        freqTable[static_cast<size_t>(i)] = xToFrequency(static_cast<float>(i));
    
    curvesDirty = true;
    updateCurvePath();
    updateBandPaths();
}

void EQCurveComponent::timerCallback()
{
    // Immer dirty setzen wenn Dynamic EQ aktiv ist (Gain-Reduction ändert sich kontinuierlich)
    // Sonst nur bei echten Parameter-Änderungen
    if (eqProcessor != nullptr)
    {
        for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
        {
            if (eqProcessor->getBand(i).isDynamicMode())
            {
                curvesDirty = true;
                break;
            }
        }
    }
    
    if (curvesDirty)
    {
        updateCurvePath();
        updateBandPaths();
        curvesDirty = false;
    }
    repaint();
}

void EQCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    int bandAtPos = getBandAtPosition(e.position);
    
    // Rechtsklick -> Popup zeigen/verstecken
    if (e.mods.isRightButtonDown())
    {
        if (bandAtPos >= 0)
        {
            // Rechtsklick auf Band
            if (bandWithOpenPopup == bandAtPos)
            {
                // Gleicher Band -> Popup schließen
                bandWithOpenPopup = -1;
                listeners.call([](Listener& l) { l.bandRightClicked(-1); }); // Signal zum Schließen
            }
            else
            {
                // Anderer Band oder kein Popup -> Neues Popup öffnen
                setSelectedBand(bandAtPos);
                bandWithOpenPopup = bandAtPos;
                listeners.call([bandAtPos](Listener& l) { l.bandRightClicked(bandAtPos); });
            }
        }
        else
        {
            // Rechtsklick daneben -> Popup schließen
            if (bandWithOpenPopup >= 0)
            {
                bandWithOpenPopup = -1;
                listeners.call([](Listener& l) { l.bandRightClicked(-1); });
            }
        }
        return;
    }
    
    // Shift-Click für Fallback (Touchpad ohne Rechtsklick)
    if (e.mods.isShiftDown() && bandAtPos >= 0)
    {
        if (bandWithOpenPopup == bandAtPos)
        {
            // Gleicher Band -> Popup schließen
            bandWithOpenPopup = -1;
            listeners.call([](Listener& l) { l.bandRightClicked(-1); });
        }
        else
        {
            // Anderer Band -> Neues Popup
            setSelectedBand(bandAtPos);
            bandWithOpenPopup = bandAtPos;
            listeners.call([bandAtPos](Listener& l) { l.bandRightClicked(bandAtPos); });
        }
        return;
    }
    
    // Linksklick
    if (bandAtPos >= 0)
    {
        // Schließe Popup bevor wir anfangen zu dragging
        if (bandWithOpenPopup >= 0)
        {
            bandWithOpenPopup = -1;
            listeners.call([](Listener& l) { l.bandRightClicked(-1); });
        }
        
        // Linksklick = Band auswählen und zum Dragging vorbereiten
        setSelectedBand(bandAtPos);
        isDragging = true;
        dragStartPos = e.position;
        dragStartFreq = bandHandles[static_cast<size_t>(bandAtPos)].frequency;
        dragStartGain = bandHandles[static_cast<size_t>(bandAtPos)].gain;
    }
    else
    {
        // Klick daneben -> Popup schließen, Selektion aufheben
        if (bandWithOpenPopup >= 0)
        {
            bandWithOpenPopup = -1;
            listeners.call([](Listener& l) { l.bandRightClicked(-1); });
        }
        setSelectedBand(-1);
    }
}

void EQCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging || selectedBand < 0)
        return;

    auto& handle = bandHandles[static_cast<size_t>(selectedBand)];
    
    // Drag-Constraint basierend auf Modifier-Tasten
    if (e.mods.isShiftDown() && !e.mods.isCtrlDown())
        dragConstraint = DragConstraint::HorizontalOnly;  // Nur Frequenz
    else if (e.mods.isCtrlDown() && !e.mods.isShiftDown())
        dragConstraint = DragConstraint::VerticalOnly;    // Nur Gain
    else
        dragConstraint = DragConstraint::None;
    
    // Neue Position berechnen
    float newFreq = handle.frequency;
    float newGain = handle.gain;
    
    // Feineinstellung mit Alt-Taste
    float scaleFactor = e.mods.isAltDown() ? 0.25f : 1.0f;
    
    if (dragConstraint != DragConstraint::VerticalOnly)
    {
        float targetX = e.mods.isAltDown() 
            ? dragStartPos.x + (e.position.x - dragStartPos.x) * scaleFactor
            : e.position.x;
        newFreq = xToFrequency(targetX);
    }
    
    if (dragConstraint != DragConstraint::HorizontalOnly)
    {
        float targetY = e.mods.isAltDown()
            ? dragStartPos.y + (e.position.y - dragStartPos.y) * scaleFactor
            : e.position.y;
        newGain = yToDb(targetY);
    }
    
    // Werte begrenzen
    newFreq = juce::jlimit(minFreq, maxFreq, newFreq);
    newGain = juce::jlimit(minDB, maxDB, newGain);
    
    // Handle aktualisieren
    handle.frequency = newFreq;
    handle.gain = newGain;
    handle.x = frequencyToX(newFreq);
    handle.y = dbToY(newGain);
    
    // EQ-Processor aktualisieren
    if (eqProcessor != nullptr)
    {
        eqProcessor->getBand(selectedBand).setFrequency(newFreq);
        eqProcessor->getBand(selectedBand).setGain(newGain);
    }
    
    notifyBandChanged(selectedBand);
    repaint();
}

void EQCurveComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    isDragging = false;
    dragConstraint = DragConstraint::None;
}

void EQCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    int newHovered = getBandAtPosition(e.position);
    
    if (newHovered != hoveredBand)
    {
        hoveredBand = newHovered;
        
        if (hoveredBand >= 0)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
        
        repaint();
    }
}

void EQCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    int bandAtPos = getBandAtPosition(e.position);
    
    if (bandAtPos >= 0)
    {
        // Doppelklick auf Band -> Band löschen
        deleteBand(bandAtPos);
    }
    else
    {
        // Doppelklick im leeren Raum -> Neues Band erstellen
        float freq = xToFrequency(e.position.x);
        
        // Erstes inaktives Band finden
        for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
        {
            if (!bandHandles[static_cast<size_t>(i)].active)
            {
                auto& handle = bandHandles[static_cast<size_t>(i)];
                handle.frequency = freq;
                handle.gain = 0.0f;
                handle.q = ParameterIDs::DEFAULT_Q;
                handle.type = ParameterIDs::FilterType::Bell;
                handle.active = true;
                handle.bypassed = false;
                handle.x = frequencyToX(freq);
                handle.y = dbToY(0.0f);
                
                if (eqProcessor != nullptr)
                {
                    auto& band = eqProcessor->getBand(i);
                    band.setParameters(freq, 0.0f, ParameterIDs::DEFAULT_Q,
                                       ParameterIDs::FilterType::Bell,
                                       ParameterIDs::ChannelMode::Stereo, false);
                    band.setActive(true);
                }
                
                setSelectedBand(i);
                
                listeners.call([i, freq](Listener& l) { l.bandCreated(i, freq); });
                
                break;
            }
        }
    }
    
    repaint();
}

void EQCurveComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int bandAtPos = getBandAtPosition(e.position);
    
    if (bandAtPos < 0 && selectedBand >= 0)
        bandAtPos = selectedBand;
    
    if (bandAtPos >= 0)
    {
        auto& handle = bandHandles[static_cast<size_t>(bandAtPos)];
        
        // Q ändern mit Mausrad
        float qDelta = wheel.deltaY * 0.5f;
        
        // Feineinstellung mit Alt-Taste
        if (e.mods.isAltDown())
            qDelta *= 0.25f;
        
        float newQ = handle.q * std::pow(2.0f, qDelta);
        newQ = juce::jlimit(ParameterIDs::MIN_Q, ParameterIDs::MAX_Q, newQ);
        
        handle.q = newQ;
        
        if (eqProcessor != nullptr)
        {
            eqProcessor->getBand(bandAtPos).setQ(newQ);
        }
        
        notifyBandChanged(bandAtPos);
        repaint();
    }
}

void EQCurveComponent::updateCurvePath()
{
    curvePath.clear();
    
    if (eqProcessor == nullptr || getWidth() <= 0 || getHeight() <= 0)
        return;

    const int numPoints = getWidth();
    if (static_cast<size_t>(numPoints) > freqTable.size())
        return;  // freqTable nicht initialisiert
    
    bool started = false;

    for (int i = 0; i < numPoints; ++i)
    {
        float freq = freqTable[static_cast<size_t>(i)];
        float magnitude = eqProcessor->getTotalMagnitudeForFrequency(freq);
        float y = dbToY(magnitude);
        
        y = juce::jlimit(0.0f, static_cast<float>(getHeight()), y);

        if (!started)
        {
            curvePath.startNewSubPath(static_cast<float>(i), y);
            started = true;
        }
        else
        {
            curvePath.lineTo(static_cast<float>(i), y);
        }
    }
}

void EQCurveComponent::updateBandPaths()
{
    if (eqProcessor == nullptr || getWidth() <= 0 || getHeight() <= 0)
        return;

    const int numPoints = getWidth();
    if (static_cast<size_t>(numPoints) > freqTable.size())
        return;  // freqTable nicht initialisiert

    for (int bandIdx = 0; bandIdx < ParameterIDs::MAX_BANDS; ++bandIdx)
    {
        bandPaths[static_cast<size_t>(bandIdx)].clear();
        
        const auto& handle = bandHandles[static_cast<size_t>(bandIdx)];
        if (!handle.active || handle.bypassed)
            continue;

        const auto& band = eqProcessor->getBand(bandIdx);
        bool started = false;

        for (int i = 0; i < numPoints; ++i)
        {
            float freq = freqTable[static_cast<size_t>(i)];
            float magnitude = band.getMagnitudeForFrequency(freq);
            float y = dbToY(magnitude);
            
            y = juce::jlimit(0.0f, static_cast<float>(getHeight()), y);

            if (!started)
            {
                bandPaths[static_cast<size_t>(bandIdx)].startNewSubPath(static_cast<float>(i), y);
                started = true;
            }
            else
            {
                bandPaths[static_cast<size_t>(bandIdx)].lineTo(static_cast<float>(i), y);
            }
        }
    }
}

void EQCurveComponent::drawCurve(juce::Graphics& g)
{
    if (curvePath.isEmpty())
        return;

    // Gefüllte Fläche unter/über 0dB
    juce::Path fillPath = curvePath;
    float zeroY = dbToY(0.0f);

    fillPath.lineTo(static_cast<float>(getWidth()), zeroY);
    fillPath.lineTo(0.0f, zeroY);
    fillPath.closeSubPath();

    // Gradient-Füllung (Pro-Q Style)
    juce::ColourGradient gradient(
        CustomLookAndFeel::getCurveColor().withAlpha(0.18f), 0, 0,
        CustomLookAndFeel::getCurveColor().withAlpha(0.03f), 0, static_cast<float>(getHeight()),
        false);
    g.setGradientFill(gradient);
    g.fillPath(fillPath);

    // ==========================================
    // Pro-Q Style Glow-Effekt für die Kurve
    // ==========================================

    // 1. Äußerer Glow (breit, sehr transparent)
    g.setColour(CustomLookAndFeel::getCurveColor().withAlpha(0.15f));
    g.strokePath(curvePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // 2. Mittlerer Glow
    g.setColour(CustomLookAndFeel::getCurveColor().withAlpha(0.3f));
    g.strokePath(curvePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // 3. Innerer Glow
    g.setColour(CustomLookAndFeel::getCurveColor().withAlpha(0.5f));
    g.strokePath(curvePath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // 4. Hauptkurve (volle Farbe, schmal)
    g.setColour(CustomLookAndFeel::getCurveColor());
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
}

void EQCurveComponent::drawBandCurve(juce::Graphics& g, int bandIndex)
{
    const auto& path = bandPaths[static_cast<size_t>(bandIndex)];
    if (path.isEmpty())
        return;

    juce::Colour bandColour = CustomLookAndFeel::getBandColor(bandIndex);
    
    // Transparente Füllung für das Band
    juce::Path fillPath = path;
    float zeroY = dbToY(0.0f);
    
    fillPath.lineTo(static_cast<float>(getWidth()), zeroY);
    fillPath.lineTo(0.0f, zeroY);
    fillPath.closeSubPath();
    
    g.setColour(bandColour.withAlpha(0.1f));
    g.fillPath(fillPath);
    
    // Band-Kurve (dünner als Hauptkurve)
    if (bandIndex == selectedBand)
    {
        g.setColour(bandColour.withAlpha(0.8f));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
}

void EQCurveComponent::drawBandHandles(juce::Graphics& g)
{
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        const auto& handle = bandHandles[static_cast<size_t>(i)];
        if (!handle.active)
            continue;

        juce::Colour bandColour = CustomLookAndFeel::getBandColor(i);
        float x = handle.x;
        float staticY = handle.y;
        float y = staticY;

        // Dynamic EQ State einmal abfragen
        bool isDynamic = false;
        bool hasDynamicGR = false;
        float gr = 0.0f;

        if (eqProcessor != nullptr && !handle.bypassed)
        {
            const auto& band = eqProcessor->getBand(i);
            isDynamic = band.isDynamicMode();
            if (isDynamic)
            {
                gr = band.getDynamicGainReduction();
                if (gr > 0.05f)
                {
                    hasDynamicGR = true;
                    y = dbToY(calcEffectiveGain(handle.gain, gr));
                }
            }
        }

        // Farbcodierung nach Boost/Cut
        juce::Colour gainColour = bandColour;
        if (!handle.bypassed && std::abs(handle.gain) > 0.1f)
        {
            gainColour = bandColour.interpolatedWith(
                handle.gain > 0 ? juce::Colours::green : juce::Colours::red, 0.3f);
        }

        // Größe basierend auf Selektion/Hover
        float radius = HANDLE_RADIUS;
        bool isSelected = (i == selectedBand);
        bool isHovered = (i == hoveredBand);

        if (isSelected)       radius *= 1.3f;
        else if (isHovered)   radius *= 1.15f;

        // Ghost-Handle an statischer Position (Pro-Q 3 Style)
        if (hasDynamicGR)
        {
            // Gestrichelte Verbindungslinie Ghost → Handle
            juce::Path connPath;
            connPath.startNewSubPath(x, staticY);
            connPath.lineTo(x, y);
            
            float dashes[] = { 3.0f, 3.0f };
            juce::Path dashedConn;
            juce::PathStrokeType(1.0f).createDashedStroke(dashedConn, connPath, dashes, 2);
            g.setColour(juce::Colour(0xFFFF9500).withAlpha(0.5f));
            g.fillPath(dashedConn);
            
            // Ghost-Ring + Center-Dot
            float ghostR = HANDLE_RADIUS * 0.8f;
            g.setColour(bandColour.withAlpha(0.25f));
            g.drawEllipse(x - ghostR, staticY - ghostR, ghostR * 2.0f, ghostR * 2.0f, 1.5f);
            g.setColour(bandColour.withAlpha(0.15f));
            g.fillEllipse(x - 2.0f, staticY - 2.0f, 4.0f, 4.0f);
        }

        // Hilfslinie zur 0dB-Linie
        if ((isSelected || isHovered) && !handle.bypassed && std::abs(handle.gain) > 0.5f)
            drawDragGuideLine(g, i);

        // Glow-Effekt (Selektion/Hover)
        if (isSelected || isHovered)
        {
            g.setColour(gainColour.withAlpha(0.2f));
            g.fillEllipse(x - radius - 6, y - radius - 6, (radius + 6) * 2.0f, (radius + 6) * 2.0f);
            g.setColour(gainColour.withAlpha(0.3f));
            g.fillEllipse(x - radius - 3, y - radius - 3, (radius + 3) * 2.0f, (radius + 3) * 2.0f);
        }

        // Äußerer Ring
        if (handle.bypassed)
        {
            g.setColour(bandColour.withAlpha(0.3f));
            g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
        }
        else
        {
            g.setColour(gainColour.withAlpha(0.6f));
            g.drawEllipse(x - radius - 1, y - radius - 1, (radius + 1) * 2.0f, (radius + 1) * 2.0f, 3.0f);
            g.setColour(gainColour);
            g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, 2.5f);
        }

        // Innerer Kreis
        if (isSelected || isHovered)
        {
            g.setColour(gainColour.withAlpha(isSelected ? 0.6f : 0.3f));
            g.fillEllipse(x - radius + 3, y - radius + 3, (radius - 3) * 2.0f, (radius - 3) * 2.0f);
        }

        // Dynamic EQ: GR-Ring + DYN-Label
        if (hasDynamicGR)
        {
            float grNorm = juce::jlimit(0.0f, 1.0f, gr / 12.0f);
            float grRadius = radius + 3.0f + grNorm * 6.0f;
            g.setColour(juce::Colour::fromHSV(0.08f, 0.9f, 1.0f, 0.3f + grNorm * 0.5f));
            g.drawEllipse(x - grRadius, y - grRadius, grRadius * 2.0f, grRadius * 2.0f, 1.5f);
        }

        // Dynamic EQ: Signal-Level + Threshold Indikatoren (Pro-Q Style)
        if (isDynamic && (isSelected || isHovered))
        {
            const auto& band = eqProcessor->getBand(i);
            float signalLevelDB = band.getEnvelopeLevelDB();
            float thresholdDB = band.getThreshold();
            
            // Vertikaler Bereich: Kleine Skala neben dem Handle
            float barX = x + radius + 6.0f;
            float barW = 4.0f;
            float barTop = y - 30.0f;
            float barBottom = y + 30.0f;
            float barH = barBottom - barTop;
            
            // Hintergrund-Balken (dB-Skala von -60 bis 0 dBFS)
            float dbMin = -60.0f;
            float dbMax = 0.0f;
            
            g.setColour(juce::Colour(0x40000000));
            g.fillRoundedRectangle(barX, barTop, barW, barH, 2.0f);
            
            // Threshold-Marker (horizontal, cyan)
            float threshNorm = juce::jlimit(0.0f, 1.0f, (thresholdDB - dbMin) / (dbMax - dbMin));
            float threshY = barBottom - threshNorm * barH;
            g.setColour(juce::Colour(0xCC00CCFF)); // Cyan
            g.fillRect(barX - 3.0f, threshY - 1.0f, barW + 6.0f, 2.0f);
            
            // "T" Label am Threshold
            g.setFont(7.0f);
            g.drawText("T", static_cast<int>(barX + barW + 2), static_cast<int>(threshY - 5), 8, 10,
                       juce::Justification::centredLeft);
            
            // Signal-Level Balken (gr\u00fcn/gelb/rot je nach N\u00e4he zum Threshold)
            if (signalLevelDB > dbMin)
            {
                float levelNorm = juce::jlimit(0.0f, 1.0f, (signalLevelDB - dbMin) / (dbMax - dbMin));
                float levelY = barBottom - levelNorm * barH;
                float levelH = barBottom - levelY;
                
                juce::Colour levelColour;
                if (signalLevelDB >= thresholdDB)
                    levelColour = juce::Colour(0xCCFF9500); // Orange - \u00fcber Threshold
                else if (signalLevelDB >= thresholdDB - 6.0f)
                    levelColour = juce::Colour(0xCCFFCC00); // Gelb - nah am Threshold
                else
                    levelColour = juce::Colour(0xCC44CC44); // Gr\u00fcn - unter Threshold
                
                g.setColour(levelColour);
                g.fillRoundedRectangle(barX, levelY, barW, levelH, 1.0f);
            }
        }
        else if (isDynamic && !isSelected && !isHovered)
        {
            // Kompakter Threshold-Tick auch ohne Hover (nur kleiner Strich)
            const auto& band = eqProcessor->getBand(i);
            float signalLevelDB = band.getEnvelopeLevelDB();
            float thresholdDB = band.getThreshold();
            
            // Kleiner Signal-Punkt rechts vom Handle
            if (signalLevelDB > -60.0f)
            {
                juce::Colour dotColour = (signalLevelDB >= thresholdDB)
                    ? juce::Colour(0xCCFF9500)   // Orange
                    : juce::Colour(0x8844CC44);   // Gr\u00fcn (gedimmt)
                g.setColour(dotColour);
                g.fillEllipse(x + radius + 3.0f, y - 2.0f, 4.0f, 4.0f);
            }
        }

        if (isDynamic)
        {
            g.setColour(juce::Colour(0xFFFF9500));
            g.setFont(8.0f);
            g.drawText("DYN", static_cast<int>(x - 12), static_cast<int>(y + radius + 2), 24, 10,
                       juce::Justification::centred);
        }

        // Band-Nummer
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(juce::String(i + 1),
                   static_cast<int>(x - 5), static_cast<int>(y - 5), 10, 10,
                   juce::Justification::centred);

        // Parameter-Anzeige
        if (isSelected || isHovered)
            drawParameterDisplay(g, i);
    }
}

int EQCurveComponent::getBandAtPosition(juce::Point<float> pos) const
{
    // Zuerst prüfen, ob die dynamische (gezeichnete) Position getroffen wird
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        const auto& handle = bandHandles[static_cast<size_t>(i)];
        if (!handle.active)
            continue;

        float handleY = handle.y; // Statische Position
        
        // Bei Dynamic EQ: Auch die effektive (gezeichnete) Position prüfen
        if (eqProcessor != nullptr && !handle.bypassed)
        {
            const auto& band = eqProcessor->getBand(i);
            if (band.isDynamicMode())
            {
                float gr = band.getDynamicGainReduction();
                if (gr > 0.05f)
                {
                    float dynY = dbToY(calcEffectiveGain(handle.gain, gr));
                    float dxDyn = pos.x - handle.x;
                    float dyDyn = pos.y - dynY;
                    if (std::sqrt(dxDyn * dxDyn + dyDyn * dyDyn) <= HANDLE_HIT_RADIUS)
                        return i;
                }
            }
        }
        
        // Statische Position prüfen
        float dx = pos.x - handle.x;
        float dy = pos.y - handleY;
        if (std::sqrt(dx * dx + dy * dy) <= HANDLE_HIT_RADIUS)
            return i;
    }
    
    return -1;
}

void EQCurveComponent::notifyBandChanged(int bandIndex)
{
    const auto& handle = bandHandles[static_cast<size_t>(bandIndex)];
    curvesDirty = true;
    
    listeners.call([bandIndex, &handle](Listener& l) {
        l.bandParameterChanged(bandIndex, handle.frequency, handle.gain, handle.q);
    });
}

void EQCurveComponent::notifyBandSelected(int bandIndex)
{
    listeners.call([bandIndex](Listener& l) { l.bandSelected(bandIndex); });
}

void EQCurveComponent::drawParameterDisplay(juce::Graphics& g, int bandIndex)
{
    const auto& handle = bandHandles[static_cast<size_t>(bandIndex)];
    juce::Colour bandColour = CustomLookAndFeel::getBandColor(bandIndex);
    
    juce::String freqText = formatFrequency(handle.frequency);
    juce::String gainText = formatGain(handle.gain);
    juce::String qText = "Q: " + formatQ(handle.q);
    
    // Dynamic EQ State einmal abfragen
    bool isDynamic = false;
    float gr = 0.0f;
    if (eqProcessor != nullptr)
    {
        const auto& band = eqProcessor->getBand(bandIndex);
        isDynamic = band.isDynamicMode();
        if (isDynamic)
            gr = band.getDynamicGainReduction();
    }
    bool hasDynamicGR = isDynamic && gr > 0.05f;
    
    // Box-Größe
    float boxWidth = 85.0f;
    float boxHeight = isDynamic ? 90.0f : 48.0f;
    float padding = 15.0f;
    
    float boxX = handle.x + padding;
    float boxY = handle.y - boxHeight / 2.0f;
    
    // Clamp an Rändern
    if (boxX + boxWidth > static_cast<float>(getWidth()) - 10.0f)
        boxX = handle.x - boxWidth - padding;
    boxY = juce::jlimit(5.0f, static_cast<float>(getHeight()) - boxHeight - 5.0f, boxY);
    
    // Hintergrund-Box
    juce::Rectangle<float> boxBounds(boxX, boxY, boxWidth, boxHeight);
    g.setColour(juce::Colour(0xE0202020));
    g.fillRoundedRectangle(boxBounds, 4.0f);
    g.setColour(bandColour.withAlpha(0.6f));
    g.drawRoundedRectangle(boxBounds, 4.0f, 1.0f);
    
    // Text-Bereich
    float textY = boxY + 4.0f;
    float lineHeight = 14.0f;
    int textX = static_cast<int>(boxX + 5);
    int textW = static_cast<int>(boxWidth - 10);
    
    // Frequenz
    g.setFont(12.0f);
    g.setColour(CustomLookAndFeel::getTextColor());
    g.drawText(freqText, textX, static_cast<int>(textY), textW, static_cast<int>(lineHeight),
               juce::Justification::centred);
    
    // Gain
    textY += lineHeight;
    if (hasDynamicGR)
    {
        // Effektiver Gain (orange)
        g.setColour(juce::Colour(0xFFFF9500));
        g.drawText(formatGain(calcEffectiveGain(handle.gain, gr)),
                   textX, static_cast<int>(textY), textW, static_cast<int>(lineHeight),
                   juce::Justification::centred);
        
        // Target-Gain (grau, klein)
        textY += lineHeight;
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));
        g.setFont(9.0f);
        g.drawText("(" + gainText + ")", textX, static_cast<int>(textY), textW, 
                   static_cast<int>(lineHeight), juce::Justification::centred);
        g.setFont(12.0f);
    }
    else
    {
        if (handle.gain > 0.1f)        g.setColour(juce::Colours::lightgreen);
        else if (handle.gain < -0.1f)  g.setColour(juce::Colours::salmon);
        else                           g.setColour(CustomLookAndFeel::getTextColor());
        g.drawText(gainText, textX, static_cast<int>(textY), textW, 
                   static_cast<int>(lineHeight), juce::Justification::centred);
    }
    
    // Q-Wert
    textY += lineHeight;
    g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.7f));
    g.setFont(10.0f);
    g.drawText(qText, textX, static_cast<int>(textY), textW, 
               static_cast<int>(lineHeight), juce::Justification::centred);
    
    // Dynamic EQ: GR-Balken + Text
    if (isDynamic)
    {
        textY += lineHeight;
        float grBarW = boxWidth - 14.0f;
        float grBarH = 4.0f;
        float grBarX = boxX + 7.0f;
        float grBarY = textY + 3.0f;
        
        g.setColour(juce::Colour(0xFF333333));
        g.fillRoundedRectangle(grBarX, grBarY, grBarW, grBarH, 2.0f);
        
        float grNorm = juce::jlimit(0.0f, 1.0f, gr / 12.0f);
        if (grNorm > 0.01f)
        {
            g.setColour(juce::Colour(0xFFFF9500));
            g.fillRoundedRectangle(grBarX, grBarY, grBarW * grNorm, grBarH, 2.0f);
        }
        
        textY += grBarH + 2.0f;
        g.setColour(juce::Colour(0xFFFF9500));
        g.setFont(9.0f);
        g.drawText("GR: -" + juce::String(gr, 1) + " dB", textX, static_cast<int>(textY),
                   textW, static_cast<int>(lineHeight), juce::Justification::centred);
    }
}

void EQCurveComponent::drawDragGuideLine(juce::Graphics& g, int bandIndex)
{
    const auto& handle = bandHandles[static_cast<size_t>(bandIndex)];
    juce::Colour bandColour = CustomLookAndFeel::getBandColor(bandIndex);
    
    float zeroY = dbToY(0.0f);
    float x = handle.x;
    float y = handle.y;
    
    // Gestrichelte Linie vom Handle zur 0dB-Linie
    g.setColour(bandColour.withAlpha(0.5f));
    
    // Gestrichelter Linientyp
    juce::Path linePath;
    linePath.startNewSubPath(x, y);
    linePath.lineTo(x, zeroY);
    
    float dashLengths[] = { 4.0f, 4.0f };
    juce::PathStrokeType strokeType(1.0f);
    strokeType.createDashedStroke(linePath, linePath, dashLengths, 2);
    
    g.strokePath(linePath, juce::PathStrokeType(1.0f));
    
    // Kleiner Marker bei 0dB
    g.fillEllipse(x - 3.0f, zeroY - 3.0f, 6.0f, 6.0f);
    
    // Gain-Differenz-Indikator (Pfeil)
    if (handle.gain > 0.5f)
    {
        // Pfeil nach oben
        juce::Path arrow;
        arrow.startNewSubPath(x, y + HANDLE_RADIUS + 2);
        arrow.lineTo(x - 4, y + HANDLE_RADIUS + 8);
        arrow.lineTo(x + 4, y + HANDLE_RADIUS + 8);
        arrow.closeSubPath();
        g.setColour(juce::Colours::lightgreen.withAlpha(0.7f));
        g.fillPath(arrow);
    }
    else if (handle.gain < -0.5f)
    {
        // Pfeil nach unten
        juce::Path arrow;
        arrow.startNewSubPath(x, y - HANDLE_RADIUS - 2);
        arrow.lineTo(x - 4, y - HANDLE_RADIUS - 8);
        arrow.lineTo(x + 4, y - HANDLE_RADIUS - 8);
        arrow.closeSubPath();
        g.setColour(juce::Colours::salmon.withAlpha(0.7f));
        g.fillPath(arrow);
    }
}

juce::String EQCurveComponent::formatFrequency(float freq) const
{
    if (freq >= 1000.0f)
        return juce::String(freq / 1000.0f, 2) + " kHz";
    else
        return juce::String(static_cast<int>(freq)) + " Hz";
}

juce::String EQCurveComponent::formatGain(float gain) const
{
    juce::String sign = gain >= 0.0f ? "+" : "";
    return sign + juce::String(gain, 1) + " dB";
}

juce::String EQCurveComponent::formatQ(float q) const
{
    return juce::String(q, 2);
}

void EQCurveComponent::showFilterTypeMenu(int bandIndex, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    auto typeNames = ParameterIDs::getFilterTypeNames();
    
    menu.addSectionHeader("Filter Type");
    
    for (int i = 0; i < static_cast<int>(ParameterIDs::FilterType::NumTypes); ++i)
    {
        bool isCurrent = (bandHandles[static_cast<size_t>(bandIndex)].type == static_cast<ParameterIDs::FilterType>(i));
        menu.addItem(i + 1, typeNames[i], true, isCurrent);
    }
    
    menu.addSeparator();
    menu.addItem(100, "Delete Band", true, false);
    menu.addItem(101, bandHandles[static_cast<size_t>(bandIndex)].bypassed ? "Enable Band" : "Bypass Band", true, false);
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
        [this, bandIndex](int result)
        {
            if (result == 100)
            {
                // Band löschen (deaktivieren)
                bandHandles[static_cast<size_t>(bandIndex)].active = false;
                bandHandles[static_cast<size_t>(bandIndex)].gain = 0.0f;
                
                if (eqProcessor != nullptr)
                {
                    eqProcessor->getBand(bandIndex).setActive(false);
                    eqProcessor->getBand(bandIndex).setGain(0.0f);
                }
                
                if (selectedBand == bandIndex)
                    setSelectedBand(-1);
                    
                // Listener informieren für Gain = 0 und Active = false
                notifyBandChanged(bandIndex);
            }
            else if (result == 101)
            {
                // Bypass umschalten
                bool newBypass = !bandHandles[static_cast<size_t>(bandIndex)].bypassed;
                bandHandles[static_cast<size_t>(bandIndex)].bypassed = newBypass;
                
                if (eqProcessor != nullptr)
                    eqProcessor->getBand(bandIndex).setBypassed(newBypass);
            }
            else if (result > 0 && result <= static_cast<int>(ParameterIDs::FilterType::NumTypes))
            {
                auto newType = static_cast<ParameterIDs::FilterType>(result - 1);
                bandHandles[static_cast<size_t>(bandIndex)].type = newType;
                
                if (eqProcessor != nullptr)
                    eqProcessor->getBand(bandIndex).setType(newType);
                
                notifyFilterTypeChanged(bandIndex, newType);
            }
            repaint();
        });
}

void EQCurveComponent::notifyFilterTypeChanged(int bandIndex, ParameterIDs::FilterType type)
{
    listeners.call([bandIndex, type](Listener& l) { l.filterTypeChanged(bandIndex, type); });
}

void EQCurveComponent::deleteBand(int bandIndex)
{
    if (bandIndex < 0 || bandIndex >= ParameterIDs::MAX_BANDS)
        return;

    auto& handle = bandHandles[static_cast<size_t>(bandIndex)];
    
    if (!handle.active)
        return;

    // Band deaktivieren
    handle.active = false;
    handle.gain = 0.0f;
    handle.q = ParameterIDs::DEFAULT_Q;
    handle.frequency = 1000.0f;
    handle.bypassed = false;
    handle.type = ParameterIDs::FilterType::Bell;

    // Im Processor auch deaktivieren und Parameter zurücksetzen
    if (eqProcessor != nullptr)
    {
        auto& band = eqProcessor->getBand(bandIndex);
        band.setActive(false);
        // Parameter auf Standard zurücksetzen
        band.setParameters(1000.0f, 0.0f, ParameterIDs::DEFAULT_Q,
                          ParameterIDs::FilterType::Bell);
    }

    // Selection aufräumen
    if (selectedBand == bandIndex)
        setSelectedBand(-1);
    if (hoveredBand == bandIndex)
        hoveredBand = -1;

    // Benachrichtige alle Listener über Band-Löschung
    listeners.call([bandIndex](Listener& l) { l.bandDeleted(bandIndex); });

    // Repaint
    repaint();
}

void EQCurveComponent::drawDynamicEQCurves(juce::Graphics& g)
{
    if (eqProcessor == nullptr || getWidth() <= 0 || getHeight() <= 0)
        return;
    
    for (int bandIdx = 0; bandIdx < ParameterIDs::MAX_BANDS; ++bandIdx)
    {
        const auto& handle = bandHandles[static_cast<size_t>(bandIdx)];
        if (!handle.active || handle.bypassed)
            continue;
        
        const auto& band = eqProcessor->getBand(bandIdx);
        if (!band.isDynamicMode())
            continue;
        
        float gr = band.getDynamicGainReduction();
        if (gr < 0.1f)
            continue;
        
        // Berechne die dynamisch reduzierte Kurve fuer dieses Band
        float grFactor = juce::jlimit(0.0f, 1.0f, 1.0f - (gr / (std::abs(handle.gain) + 0.01f)));
        
        juce::Path dynamicPath;
        const int numPoints = getWidth() / 2; // Weniger Punkte fuer Performance
        bool started = false;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float xPos = static_cast<float>(i * 2);
            float freq = xToFrequency(xPos);
            
            // Magnitude dieses Bands bei der Frequenz holen
            float bandMag = band.getMagnitudeForFrequency(freq);
            
            // Skalieren um die Gain Reduction zu zeigen
            float dynamicMag = bandMag * grFactor;
            float y = dbToY(dynamicMag);
            y = juce::jlimit(0.0f, static_cast<float>(getHeight()), y);
            
            if (!started)
            {
                dynamicPath.startNewSubPath(xPos, y);
                started = true;
            }
            else
            {
                dynamicPath.lineTo(xPos, y);
            }
        }
        
        if (!dynamicPath.isEmpty())
        {
            juce::Colour bandColour = CustomLookAndFeel::getBandColor(bandIdx);
            
            // Gefuellte Flaeche zwischen statischer und dynamischer Kurve (GR-Zone)
            juce::Path grZone;
            float zeroY = dbToY(0.0f);
            
            // Die dynamische Kurve als gefuellte Flaeche
            grZone = dynamicPath;
            grZone.lineTo(static_cast<float>(getWidth()), zeroY);
            grZone.lineTo(0.0f, zeroY);
            grZone.closeSubPath();
            
            // Orange-farbene GR-Zone
            juce::Colour grColour = juce::Colour(0xFFFF9500);
            g.setColour(grColour.withAlpha(0.12f));
            g.fillPath(grZone);
            
            // Dynamische Kurve als gestrichelte Linie
            g.setColour(grColour.withAlpha(0.6f));
            
            float dashLengths[] = { 5.0f, 3.0f };
            juce::Path dashedPath;
            juce::PathStrokeType strokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            strokeType.createDashedStroke(dashedPath, dynamicPath, dashLengths, 2);
            g.fillPath(dashedPath);
        }
    }
}

float EQCurveComponent::calcEffectiveGain(float targetGain, float gainReduction) const
{
    // Bei Boost: Gain wird reduziert (z.B. +6dB mit 3dB GR -> +3dB)
    // Bei Cut: Gain wird weniger negativ (z.B. -6dB mit 3dB GR -> -3dB)
    if (targetGain > 0.0f)
        return juce::jmax(0.0f, targetGain - gainReduction);
    else
        return juce::jmin(0.0f, targetGain + gainReduction);
}
