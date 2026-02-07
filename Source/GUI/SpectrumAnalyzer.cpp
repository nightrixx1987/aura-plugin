#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    setOpaque(false);

    // Standard: 90 dB Range (wie Pro-Q)
    setDBRange(DBRange::Range90dB);

    // Timer wird NICHT im Konstruktor gestartet!
    // Erst nach vollständiger Initialisierung via startAnalyzer()
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

void SpectrumAnalyzer::setAnalyzer(FFTAnalyzer* preAnalyzer, FFTAnalyzer* postAnalyzer)
{
    preFFT = preAnalyzer;
    postFFT = postAnalyzer;
}

void SpectrumAnalyzer::setEnabled(bool enabled)
{
    isEnabled = enabled;
    if (!enabled)
    {
        preSpectrumPath.clear();
        postSpectrumPath.clear();
    }
    repaint();
}

void SpectrumAnalyzer::setDBRange(DBRange range)
{
    settings.range = range;

    switch (range)
    {
        case DBRange::Range60dB:
            spectrumMinDB = -60.0f;
            break;
        case DBRange::Range90dB:
            spectrumMinDB = -90.0f;
            break;
        case DBRange::Range120dB:
            spectrumMinDB = -120.0f;
            break;
    }
    spectrumMaxDB = 0.0f;

    repaint();
}

void SpectrumAnalyzer::setSettings(const Settings& newSettings)
{
    settings = newSettings;
    setDBRange(settings.range);
}

void SpectrumAnalyzer::setFrequencyRange(float minHz, float maxHz)
{
    minFreq = minHz;
    maxFreq = maxHz;
    repaint();
}

void SpectrumAnalyzer::setEQDecibelRange(float minDB, float maxDB)
{
    eqMinDB = minDB;
    eqMaxDB = maxDB;
    repaint();
}

//==============================================================================
// Koordinaten-Konvertierung
//==============================================================================

float SpectrumAnalyzer::frequencyToX(float frequency) const
{
    if (frequency <= 0.0f)
        return 0.0f;

    // Logarithmische Skalierung
    float normalized = std::log(frequency / minFreq) / std::log(maxFreq / minFreq);
    return normalized * static_cast<float>(getWidth() - rightMargin);
}

float SpectrumAnalyzer::xToFrequency(float x) const
{
    float normalized = x / static_cast<float>(getWidth() - rightMargin);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return minFreq * std::pow(maxFreq / minFreq, normalized);
}

float SpectrumAnalyzer::spectrumDbToY(float db) const
{
    // Spektrum: 0 dB oben, spectrumMinDB unten
    float normalized = (db - spectrumMinDB) / (spectrumMaxDB - spectrumMinDB);
    return static_cast<float>(getHeight()) * (1.0f - normalized);
}

float SpectrumAnalyzer::yToSpectrumDb(float y) const
{
    float normalized = 1.0f - (y / static_cast<float>(getHeight()));
    return spectrumMinDB + normalized * (spectrumMaxDB - spectrumMinDB);
}

float SpectrumAnalyzer::eqDbToY(float db) const
{
    // EQ: 0 dB in der Mitte des sichtbaren Bereichs
    float normalized = (db - eqMinDB) / (eqMaxDB - eqMinDB);
    return static_cast<float>(getHeight()) * (1.0f - normalized);
}

float SpectrumAnalyzer::yToEqDb(float y) const
{
    float normalized = 1.0f - (y / static_cast<float>(getHeight()));
    return eqMinDB + normalized * (eqMaxDB - eqMinDB);
}

//==============================================================================
// Mouse-Events für Hover
//==============================================================================

void SpectrumAnalyzer::mouseMove(const juce::MouseEvent& e)
{
    mousePosition = e.position;
    hoveredFrequency = xToFrequency(mousePosition.x);

    // Aktuelle dB bei dieser Frequenz ermitteln
    if (postFFT != nullptr && showPost)
    {
        hoveredSpectrumDb = postFFT->getMagnitudeForFrequency(hoveredFrequency);
    }
    else if (preFFT != nullptr && showPre)
    {
        hoveredSpectrumDb = preFFT->getMagnitudeForFrequency(hoveredFrequency);
    }

    repaint();
}

void SpectrumAnalyzer::mouseEnter(const juce::MouseEvent& /*e*/)
{
    mouseIsOver = true;
    repaint();
}

void SpectrumAnalyzer::mouseExit(const juce::MouseEvent& /*e*/)
{
    mouseIsOver = false;
    repaint();
}

//==============================================================================
// Buffer-Allokation (Performance!)
//==============================================================================

void SpectrumAnalyzer::allocateBuffers(int width)
{
    if (width == lastWidth && !preYValues.empty())
        return;

    lastWidth = width;
    const size_t totalPoints = static_cast<size_t>(width * OVERSAMPLE);

    preYValues.resize(totalPoints);
    postYValues.resize(totalPoints);
    smoothingTemp.resize(totalPoints);

    std::fill(preYValues.begin(), preYValues.end(), static_cast<float>(getHeight()));
    std::fill(postYValues.begin(), postYValues.end(), static_cast<float>(getHeight()));
}

//==============================================================================
// Paint
//==============================================================================

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    // Hintergrund
    g.fillAll(CustomLookAndFeel::getBackgroundDark());

    // Grid und Skalen zeichnen
    if (settings.showGrid)
    {
        drawGrid(g);
    }
    drawDualScales(g);

    if (!isEnabled)
        return;
    
    // Reference-Spektrum (falls aktiviert) - Türkis/Cyan gestrichelt
    if (showReferenceSpectrum && !referenceSpectrumPath.isEmpty())
    {
        juce::PathStrokeType strokeType(1.5f);
        float dashLengths[] = { 4.0f, 4.0f };
        strokeType.createDashedStroke(referenceSpectrumPath, referenceSpectrumPath, 
                                       dashLengths, 2);
        g.setColour(juce::Colour(0xff00dddd).withAlpha(0.7f));  // Türkis
        g.strokePath(referenceSpectrumPath, strokeType);
    }

    // Pre-Spektrum (Input) - Grau, gestrichelt
    if (showPre && !preSpectrumPath.isEmpty())
    {
        drawSpectrum(g, preSpectrumPath, CustomLookAndFeel::getSpectrumColor(), true);
    }

    // Post-Spektrum (Output) - Accent-Farbe, gefüllt
    if (showPost && !postSpectrumPath.isEmpty())
    {
        drawSpectrum(g, postSpectrumPath, CustomLookAndFeel::getSpectrumColorPost(), false);
    }
    
    // NEU: Match-Kurve (Korrektur vom SpectralMatcher)
    if (showMatchCurve && !matchCurvePath.isEmpty())
    {
        drawMatchCurve(g);
    }

    // NEU: Soothe Gain-Reduktion Overlay
    if (showSootheCurve && !sootheCurvePath.isEmpty())
    {
        drawSootheCurve(g);
    }

    // Peak-Labels
    if (settings.showPeakLabels)
    {
        drawPeakLabels(g);
    }

    // Hover-Info
    if (settings.showHoverInfo && mouseIsOver)
    {
        drawHoverInfo(g);
    }

    // Legende (PRE/POST)
    if (showPre || showPost)
    {
        drawLegend(g);
    }
}

void SpectrumAnalyzer::resized()
{
    const int width = getWidth();
    if (width > 0)
    {
        allocateBuffers(width);
    }
}

void SpectrumAnalyzer::timerCallback()
{
    if (isEnabled && (preFFT != nullptr || postFFT != nullptr))
    {
        updatePaths();

        if (settings.showPeakLabels)
        {
            detectPeaks();
        }

        repaint();
    }
}

//==============================================================================
// Path-Update (optimiert mit pre-allozierten Buffers)
//==============================================================================

void SpectrumAnalyzer::updatePaths()
{
    const int width = getWidth();
    const int height = getHeight();

    if (width <= 0 || height <= 0)
        return;

    // Buffers sicherstellen
    allocateBuffers(width);

    // Pre-Spektrum
    if (preFFT != nullptr && showPre)
    {
        updateSinglePath(preFFT, preSpectrumPath, preYValues);
    }
    else
    {
        preSpectrumPath.clear();
    }

    // Post-Spektrum
    if (postFFT != nullptr && showPost)
    {
        updateSinglePath(postFFT, postSpectrumPath, postYValues);
    }
    else
    {
        postSpectrumPath.clear();
    }
    
    // Reference-Spektrum (aus geladener Audio-Datei)
    if (showReferenceSpectrum && !referenceSpectrumData.empty())
    {
        updateReferenceSpectrumPath();
    }
    else
    {
        referenceSpectrumPath.clear();
    }
    
    // NEU: Match-Kurve (Korrektur vom SpectralMatcher)
    if (showMatchCurve && !matchCurveData.empty())
    {
        updateMatchCurvePath();
    }
    else
    {
        matchCurvePath.clear();
    }
}

void SpectrumAnalyzer::updateSinglePath(FFTAnalyzer* fft, juce::Path& path, std::vector<float>& yValues)
{
    const int width = getWidth() - rightMargin;
    const int height = getHeight();
    const int totalPoints = width * OVERSAMPLE;

    if (width <= 0 || height <= 0 || totalPoints <= 0)
        return;

    // KRITISCH: Buffer-Groesse pruefen
    const size_t requiredSize = static_cast<size_t>(totalPoints);
    if (yValues.size() < requiredSize || smoothingTemp.size() < requiredSize)
    {
        allocateBuffers(getWidth());
        if (yValues.size() < requiredSize || smoothingTemp.size() < requiredSize)
            return;
    }

    path.clear();
    float maxDbSeen = -200.0f;

    // Y-Werte berechnen (verwendet pre-allozierte Buffers!)
    for (int i = 0; i < totalPoints; ++i)
    {
        float x = static_cast<float>(i) / static_cast<float>(OVERSAMPLE);
        float freq = xToFrequency(x);
        float dbRaw = fft->getMagnitudeForFrequency(freq);
        maxDbSeen = juce::jmax(maxDbSeen, dbRaw);

        // Auf sichtbaren Bereich clippen
        float db = juce::jlimit(spectrumMinDB, spectrumMaxDB, dbRaw);
        float y = spectrumDbToY(db);
        y = juce::jlimit(0.0f, static_cast<float>(height), y);
        yValues[static_cast<size_t>(i)] = y;
    }

    // Gaussian-Glättung (2 Durchläufe)
    if (maxDbSeen > spectrumMinDB && !yValues.empty())
    {
        for (int pass = 0; pass < 2; ++pass)
        {
            for (size_t i = 0; i < static_cast<size_t>(totalPoints); ++i)
            {
                float sum = 0.0f;
                float weightSum = 0.0f;

                // 7-Punkt Moving Average mit Gaussian-Gewichtung
                for (int j = -3; j <= 3; ++j)
                {
                    int idx = static_cast<int>(i) + j;
                    if (idx >= 0 && idx < totalPoints)
                    {
                        float weight = std::exp(-static_cast<float>(j * j) / 8.0f);
                        sum += yValues[static_cast<size_t>(idx)] * weight;
                        weightSum += weight;
                    }
                }
                smoothingTemp[i] = sum / weightSum;
            }
            std::copy(smoothingTemp.begin(), smoothingTemp.begin() + totalPoints, yValues.begin());
        }

        // Pfad erstellen
        path.startNewSubPath(0.0f, yValues[0]);

        for (int i = 1; i < totalPoints; ++i)
        {
            float x = static_cast<float>(i) / static_cast<float>(OVERSAMPLE);
            path.lineTo(x, yValues[static_cast<size_t>(i)]);
        }

        // Zum unteren Rand für Füllung
        path.lineTo(static_cast<float>(width), static_cast<float>(height));
        path.lineTo(0.0f, static_cast<float>(height));
        path.closeSubPath();
    }
}

//==============================================================================
// Reference Spectrum Path
//==============================================================================

void SpectrumAnalyzer::updateReferenceSpectrumPath()
{
    const int width = getWidth() - rightMargin;
    const int height = getHeight();
    
    if (width <= 0 || height <= 0 || referenceSpectrumData.empty())
    {
        referenceSpectrumPath.clear();
        return;
    }
    
    referenceSpectrumPath.clear();
    
    const size_t numBins = referenceSpectrumData.size();
    const float sampleRate = (preFFT != nullptr) ? static_cast<float>(preFFT->getSampleRate()) 
                           : (postFFT != nullptr) ? static_cast<float>(postFFT->getSampleRate()) 
                           : 44100.0f;
    const float binWidth = sampleRate / (2.0f * static_cast<float>(numBins));
    
    // Offset um das Reference-Spektrum auf ähnliche Höhe wie Live-Spektrum zu bringen
    // FFT-Magnitudes aus Datei sind typischerweise höher als Echtzeit-FFT
    const float referenceOffset = -45.0f;
    
    bool pathStarted = false;
    
    for (int pixelX = 0; pixelX < width; ++pixelX)
    {
        float freq = xToFrequency(static_cast<float>(pixelX));
        
        // Bin-Index für diese Frequenz finden
        int binIndex = static_cast<int>(freq / binWidth);
        if (binIndex < 0 || binIndex >= static_cast<int>(numBins))
            continue;
        
        float dbValue = referenceSpectrumData[static_cast<size_t>(binIndex)] + referenceOffset;
        float y = spectrumDbToY(dbValue);
        
        if (!pathStarted)
        {
            referenceSpectrumPath.startNewSubPath(static_cast<float>(pixelX), y);
            pathStarted = true;
        }
        else
        {
            referenceSpectrumPath.lineTo(static_cast<float>(pixelX), y);
        }
    }
}

//==============================================================================
// NEU: Match Curve Path (Korrektur-Kurve)
//==============================================================================

void SpectrumAnalyzer::updateMatchCurvePath()
{
    const int width = getWidth() - rightMargin;
    const int height = getHeight();
    
    if (width <= 0 || height <= 0 || matchCurveData.empty())
    {
        matchCurvePath.clear();
        return;
    }
    
    matchCurvePath.clear();
    
    const size_t numBins = matchCurveData.size();
    const float sampleRate = (preFFT != nullptr) ? static_cast<float>(preFFT->getSampleRate()) 
                           : (postFFT != nullptr) ? static_cast<float>(postFFT->getSampleRate()) 
                           : 44100.0f;
    const float binWidth = sampleRate / (2.0f * static_cast<float>(numBins));
    
    // Für Match-Kurve: 0 dB = Mitte, ±12 dB sichtbar
    const float centerY = static_cast<float>(height) / 2.0f;
    const float dbRange = 12.0f;  // ±12 dB
    const float pixelsPerDb = (static_cast<float>(height) / 4.0f) / dbRange;  // 1/4 des Bildschirms für ±12dB
    
    bool pathStarted = false;
    
    for (int pixelX = 0; pixelX < width; ++pixelX)
    {
        float freq = xToFrequency(static_cast<float>(pixelX));
        
        // Bin-Index für diese Frequenz finden
        int binIndex = static_cast<int>(freq / binWidth);
        if (binIndex < 0 || binIndex >= static_cast<int>(numBins))
            continue;
        
        float dbCorrection = matchCurveData[static_cast<size_t>(binIndex)];
        
        // Clippen auf ±12 dB
        dbCorrection = juce::jlimit(-dbRange, dbRange, dbCorrection);
        
        // Y-Position berechnen (positive dB = nach oben)
        float y = centerY - (dbCorrection * pixelsPerDb);
        
        if (!pathStarted)
        {
            matchCurvePath.startNewSubPath(static_cast<float>(pixelX), y);
            pathStarted = true;
        }
        else
        {
            matchCurvePath.lineTo(static_cast<float>(pixelX), y);
        }
    }
}

void SpectrumAnalyzer::drawMatchCurve(juce::Graphics& g)
{
    if (!showMatchCurve || matchCurvePath.isEmpty())
        return;
    
    // Farbe: Gelb/Gold für Match-Kurve (unterscheidet sich von Reference=Cyan)
    juce::Colour matchColour(0xffddaa00);  // Gold
    
    // Gestrichelte Linie
    juce::Path strokedPath;
    juce::PathStrokeType stroke(2.0f, juce::PathStrokeType::curved);
    
    float dashes[] = { 6.0f, 4.0f };
    stroke.createDashedStroke(strokedPath, matchCurvePath, dashes, 2);
    
    // Zeichnen mit Semi-Transparenz
    g.setColour(matchColour.withAlpha(0.8f));
    g.fillPath(strokedPath);
    
    // Beschriftung "MATCH" oben links
    g.setFont(10.0f);
    g.setColour(matchColour);
    g.drawText("MATCH EQ", 5, 5, 60, 15, juce::Justification::left);
}

//==============================================================================
// Peak-Detection
//==============================================================================

void SpectrumAnalyzer::detectPeaks()
{
    // Reset peaks
    for (auto& peak : detectedPeaks)
    {
        peak.valid = false;
    }

    FFTAnalyzer* fft = (showPost && postFFT != nullptr) ? postFFT : preFFT;
    if (fft == nullptr)
        return;

    const int numSamples = 256;  // Anzahl der Samples für Peak-Detection
    const float minPeakDb = spectrumMinDB + 20.0f;  // Mindestens 20 dB über Floor

    struct LocalPeak {
        float frequency;
        float magnitude;
    };
    std::vector<LocalPeak> localPeaks;

    // Lokale Maxima finden
    float prevDb = fft->getMagnitudeForFrequency(minFreq);
    float currDb = prevDb;

    for (int i = 1; i < numSamples - 1; ++i)
    {
        float freq = minFreq * std::pow(maxFreq / minFreq, static_cast<float>(i) / static_cast<float>(numSamples - 1));
        float nextFreq = minFreq * std::pow(maxFreq / minFreq, static_cast<float>(i + 1) / static_cast<float>(numSamples - 1));

        float nextDb = fft->getMagnitudeForFrequency(nextFreq);

        // Lokales Maximum?
        if (currDb > prevDb && currDb > nextDb && currDb > minPeakDb)
        {
            localPeaks.push_back({ freq, currDb });
        }

        prevDb = currDb;
        currDb = nextDb;
    }

    // Nach Magnitude sortieren (größte zuerst)
    std::sort(localPeaks.begin(), localPeaks.end(),
              [](const LocalPeak& a, const LocalPeak& b) {
                  return a.magnitude > b.magnitude;
              });

    // Top N Peaks übernehmen (mit Mindestabstand)
    int peakCount = 0;
    const float minFreqDistance = 1.2f;  // Mindestens 1.2 Oktaven Abstand

    for (const auto& localPeak : localPeaks)
    {
        if (peakCount >= MAX_PEAKS)
            break;

        // Prüfen ob zu nah an bereits gefundenem Peak
        bool tooClose = false;
        for (int j = 0; j < peakCount; ++j)
        {
            float octaveDistance = std::abs(std::log2(localPeak.frequency / detectedPeaks[j].frequency));
            if (octaveDistance < minFreqDistance)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
        {
            auto& peak = detectedPeaks[static_cast<size_t>(peakCount)];
            peak.frequency = localPeak.frequency;
            peak.magnitude = localPeak.magnitude;
            peak.x = frequencyToX(localPeak.frequency);
            peak.y = spectrumDbToY(localPeak.magnitude);
            peak.valid = true;
            ++peakCount;
        }
    }
}

//==============================================================================
// Draw-Methoden
//==============================================================================

void SpectrumAnalyzer::drawGrid(juce::Graphics& g)
{
    const int width = getWidth();
    const int height = getHeight();

    g.setColour(CustomLookAndFeel::getGridColor());

    // Vertikale Linien (Frequenzen)
    const std::array<float, 9> freqLines = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

    g.setFont(10.0f);

    for (float freq : freqLines)
    {
        float x = frequencyToX(freq);
        if (x > 0 && x < width - rightMargin)
        {
            g.setColour(CustomLookAndFeel::getGridColor());
            g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(height));

            // Frequenz-Label
            if (settings.showFrequencyLabels)
            {
                juce::String label = formatFrequency(freq);
                g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));
                g.drawText(label, static_cast<int>(x) - 15, height - 15, 30, 12, juce::Justification::centred);
            }
        }
    }

    // Horizontale Linien (dB) - basierend auf EQ-Grid (nur Hauptlinien alle 6 dB)
    g.setColour(CustomLookAndFeel::getGridColor());
    for (float db = eqMinDB; db <= eqMaxDB; db += 6.0f)
    {
        float y = eqDbToY(db);
        if (y > 0 && y < height)
        {
            // 0 dB Linie hervorheben
            if (std::abs(db) < 0.1f)
            {
                g.setColour(CustomLookAndFeel::getGridColor().brighter(0.5f));
                g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(width - rightMargin));
            }
            else
            {
                g.setColour(CustomLookAndFeel::getGridColor());
                g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(width - rightMargin));
            }
        }
    }

    // Vertikale Trennlinie für Skalen
    g.setColour(CustomLookAndFeel::getGridColor());
    g.drawVerticalLine(width - rightMargin, 0.0f, static_cast<float>(height));
}

void SpectrumAnalyzer::drawDualScales(juce::Graphics& g)
{
    const int width = getWidth();
    const int height = getHeight();

    // ==========================================
    // Linke Skala: EQ Gain (Gelb) - Pro-Q Style
    // ==========================================
    g.setFont(9.0f);

    for (float db = eqMinDB; db <= eqMaxDB; db += 6.0f)
    {
        float y = eqDbToY(db);
        if (y > 5 && y < height - 5)
        {
            // Gelbe Farbe für EQ-Skala (Pro-Q Style)
            g.setColour(CustomLookAndFeel::getCurveColor().withAlpha(0.7f));

            juce::String label = (db >= 0 ? "+" : "") + juce::String(static_cast<int>(db));
            g.drawText(label, 3, static_cast<int>(y) - 6, 25, 12, juce::Justification::left);
        }
    }

    // ==========================================
    // Rechte Skala: Spektrum (Grau) - Pro-Q Style
    // ==========================================

    // Basierend auf der aktuellen Range
    float dbStep = 10.0f;
    if (settings.range == DBRange::Range120dB)
        dbStep = 20.0f;
    else if (settings.range == DBRange::Range60dB)
        dbStep = 10.0f;

    for (float db = spectrumMinDB; db <= spectrumMaxDB; db += dbStep)
    {
        float y = spectrumDbToY(db);
        if (y > 5 && y < height - 5)
        {
            g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));

            juce::String label = juce::String(static_cast<int>(db));
            g.drawText(label, width - rightMargin + 5, static_cast<int>(y) - 5, 40, 10, juce::Justification::left);
        }
    }
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, const juce::Path& path, juce::Colour colour, bool isPre)
{
    // Anti-Aliasing aktivieren
    juce::Graphics::ScopedSaveState state(g);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    if (isPre)
    {
        // =============================================
        // PRE-Spektrum: Eigene Farbe, sichtbarer Fill, dünnere Linie
        // =============================================
        juce::ColourGradient gradient(
            colour.withAlpha(0.3f), 0, 0,
            colour.withAlpha(0.03f), 0, static_cast<float>(getHeight()),
            false);
        g.setGradientFill(gradient);
        g.fillPath(path);

        // Solide Linie
        g.setColour(colour.withAlpha(0.65f));
        g.strokePath(path, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
    else
    {
        // =============================================
        // POST-Spektrum: Andere Farbe, kräftiger Fill, dickere Linie
        // =============================================
        juce::ColourGradient gradient(
            colour.withAlpha(0.5f), 0, 0,
            colour.withAlpha(0.04f), 0, static_cast<float>(getHeight()),
            false);
        g.setGradientFill(gradient);
        g.fillPath(path);

        // Dickere solide Linie
        g.setColour(colour.withAlpha(0.9f));
        g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
}

//==============================================================================
// Soothe/Suppressor Visualization
//==============================================================================

void SpectrumAnalyzer::setSootheCurveData(const float* gainReductions, int numBins, double sr, int fftSz)
{
    if (gainReductions == nullptr || numBins <= 0 || getWidth() <= 0 || fftSz <= 0 || sr <= 0.0)
    {
        sootheCurvePath.clear();
        return;
    }

    const float height = static_cast<float>(getHeight());
    const float width = static_cast<float>(getWidth() - rightMargin);
    const float zeroY = eqDbToY(0.0f);  // 0 dB Linie (EQ-Skala)
    
    // Schritt 1: Rohdaten pro Pixel sammeln
    const int numPixels = static_cast<int>(width);
    std::vector<float> rawGr(numPixels, 0.0f);
    
    for (int px = 0; px < numPixels; ++px)
    {
        float freq = xToFrequency(static_cast<float>(px));
        if (freq < 20.0f || freq > 20000.0f)
            continue;
        
        // Frequenz zu FFT-Bin (mit Interpolation zwischen benachbarten Bins)
        float exactBin = freq * static_cast<float>(fftSz) / static_cast<float>(sr);
        int bin0 = static_cast<int>(exactBin);
        int bin1 = bin0 + 1;
        float frac = exactBin - static_cast<float>(bin0);
        
        if (bin0 < 0 || bin1 >= numBins)
            continue;
        
        // Interpolierter Gain-Reduction Wert
        float gr = gainReductions[bin0] * (1.0f - frac) + gainReductions[bin1] * frac;
        rawGr[px] = gr;
    }
    
    // Schritt 2: Glättung über benachbarte Pixel (Gauss-artig)
    std::vector<float> smoothedGr(numPixels, 0.0f);
    const int smoothRadius = 4;  // Pixel-Radius für Glättung
    for (int px = 0; px < numPixels; ++px)
    {
        float sum = 0.0f;
        float weight = 0.0f;
        for (int dx = -smoothRadius; dx <= smoothRadius; ++dx)
        {
            int idx = px + dx;
            if (idx >= 0 && idx < numPixels)
            {
                float w = 1.0f / (1.0f + static_cast<float>(dx * dx) * 0.25f);
                sum += rawGr[idx] * w;
                weight += w;
            }
        }
        smoothedGr[px] = (weight > 0.0f) ? sum / weight : 0.0f;
    }
    
    // Schritt 3: Schwellwert anwenden — unter -0.5 dB zählt als "keine Reduktion"
    const float displayThreshold = -0.5f;
    for (int px = 0; px < numPixels; ++px)
    {
        if (smoothedGr[px] > displayThreshold)
            smoothedGr[px] = 0.0f;
    }
    
    // Prüfen ob überhaupt irgendwo Reduktion stattfindet
    bool hasReduction = false;
    for (int px = 0; px < numPixels; ++px)
    {
        if (smoothedGr[px] < displayThreshold)
        {
            hasReduction = true;
            break;
        }
    }
    
    if (!hasReduction)
    {
        sootheCurvePath.clear();
        return;
    }
    
    // Schritt 4: Pfad aufbauen
    sootheCurvePath.clear();
    sootheCurvePath.startNewSubPath(0.0f, zeroY);
    
    for (int px = 0; px < numPixels; ++px)
    {
        float y = eqDbToY(smoothedGr[px]);
        y = juce::jlimit(0.0f, height, y);
        sootheCurvePath.lineTo(static_cast<float>(px), y);
    }
    
    // Pfad zurück zur 0dB-Linie schließen für Fill
    sootheCurvePath.lineTo(static_cast<float>(numPixels - 1), zeroY);
    sootheCurvePath.closeSubPath();
}

void SpectrumAnalyzer::drawSootheCurve(juce::Graphics& g)
{
    if (sootheCurvePath.isEmpty())
        return;

    // Roter/Orange Semi-transparenter Fill für die Reduktions-Zone
    juce::Colour sootheColor(0xFFFF6644);  // Orange-Rot

    // Fill
    g.setColour(sootheColor.withAlpha(0.2f));
    g.fillPath(sootheCurvePath);

    // Obere Linie (die Reduktions-Kontur)
    g.setColour(sootheColor.withAlpha(0.7f));
    
    // Erstelle einen Stroke-Path nur für die obere Kante (nicht den geschlossenen Fill-Pfad)
    // Wir nutzen den sootheCurvePath aber zeichnen nur den Stroke darüber
    g.strokePath(sootheCurvePath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

    // "SOOTHE" Label oben links wenn aktiv
    g.setFont(9.0f);
    g.setColour(sootheColor.withAlpha(0.6f));
    g.drawText("SOOTHE", 8, 42, 50, 12, juce::Justification::centredLeft);
}

void SpectrumAnalyzer::drawHoverInfo(juce::Graphics& g)
{
    if (!mouseIsOver || mousePosition.x > getWidth() - rightMargin)
        return;

    const int height = getHeight();

    // Vertikale Linie bei Cursor
    g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.4f));
    g.drawVerticalLine(static_cast<int>(mousePosition.x), 0.0f, static_cast<float>(height));

    // Horizontale Linie bei dB-Level
    float dbY = spectrumDbToY(hoveredSpectrumDb);
    if (dbY > 0 && dbY < height)
    {
        g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.3f));
        g.drawHorizontalLine(static_cast<int>(dbY), 0.0f, mousePosition.x);
    }

    // Tooltip-Box
    juce::String freqText = formatFrequency(hoveredFrequency);
    juce::String dbText = formatDb(hoveredSpectrumDb);

    float boxWidth = 80.0f;
    float boxHeight = 36.0f;
    float boxX = mousePosition.x + 10.0f;
    float boxY = mousePosition.y - boxHeight - 10.0f;

    // Box im sichtbaren Bereich halten
    if (boxX + boxWidth > getWidth() - rightMargin - 5)
        boxX = mousePosition.x - boxWidth - 10.0f;
    if (boxY < 5.0f)
        boxY = mousePosition.y + 15.0f;

    juce::Rectangle<float> boxBounds(boxX, boxY, boxWidth, boxHeight);

    // Hintergrund
    g.setColour(juce::Colour(0xE0202020));
    g.fillRoundedRectangle(boxBounds, 4.0f);

    // Rahmen
    g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.6f));
    g.drawRoundedRectangle(boxBounds, 4.0f, 1.0f);

    // Text
    g.setFont(11.0f);
    g.setColour(CustomLookAndFeel::getTextColor());
    g.drawText(freqText, static_cast<int>(boxX + 5), static_cast<int>(boxY + 4),
               static_cast<int>(boxWidth - 10), 14, juce::Justification::centred);
    g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.7f));
    g.drawText(dbText, static_cast<int>(boxX + 5), static_cast<int>(boxY + 18),
               static_cast<int>(boxWidth - 10), 14, juce::Justification::centred);
}

void SpectrumAnalyzer::drawPeakLabels(juce::Graphics& g)
{
    g.setFont(9.0f);

    for (const auto& peak : detectedPeaks)
    {
        if (!peak.valid)
            continue;

        // Kleiner Punkt am Peak
        g.setColour(CustomLookAndFeel::getAccentColor());
        g.fillEllipse(peak.x - 3.0f, peak.y - 3.0f, 6.0f, 6.0f);

        // Frequenz-Label
        juce::String label = formatFrequency(peak.frequency);

        float labelWidth = 45.0f;
        float labelX = peak.x - labelWidth / 2.0f;
        float labelY = peak.y - 18.0f;

        // Im sichtbaren Bereich halten
        if (labelX < 5.0f) labelX = 5.0f;
        if (labelX + labelWidth > getWidth() - rightMargin - 5)
            labelX = static_cast<float>(getWidth() - rightMargin) - labelWidth - 5.0f;
        if (labelY < 5.0f) labelY = peak.y + 8.0f;

        // Hintergrund
        g.setColour(juce::Colour(0xC0181818));
        g.fillRoundedRectangle(labelX - 2, labelY - 1, labelWidth + 4, 13.0f, 2.0f);

        // Text
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.9f));
        g.drawText(label, static_cast<int>(labelX), static_cast<int>(labelY),
                   static_cast<int>(labelWidth), 12, juce::Justification::centred);
    }
}

void SpectrumAnalyzer::drawLegend(juce::Graphics& g)
{
    // Nur anzeigen wenn beide aktiv sind (sonst ist es offensichtlich)
    if (!showPre || !showPost)
        return;

    const float legendX = 8.0f;
    const float legendY = 6.0f;
    const float lineHeight = 14.0f;
    const float barWidth = 15.0f;
    const float barHeight = 3.0f;
    const float textOffsetX = barWidth + 4.0f;

    g.setFont(10.0f);

    // PRE (Input) - Orange/Gold Farbbalken
    auto preColor = CustomLookAndFeel::getSpectrumColor();
    float preY = legendY;

    g.setColour(preColor.withAlpha(0.7f));
    g.fillRoundedRectangle(legendX, preY + 5.0f, barWidth, barHeight, 1.0f);

    g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));
    g.drawText("IN", static_cast<int>(legendX + textOffsetX), static_cast<int>(preY),
               30, static_cast<int>(lineHeight), juce::Justification::centredLeft);

    // POST (Output) - Accent Farbbalken
    auto postColor = CustomLookAndFeel::getSpectrumColorPost();
    float postY = legendY + lineHeight;

    g.setColour(postColor.withAlpha(0.9f));
    g.fillRoundedRectangle(legendX, postY + 5.0f, barWidth, barHeight, 1.0f);

    g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));
    g.drawText("OUT", static_cast<int>(legendX + textOffsetX), static_cast<int>(postY),
               30, static_cast<int>(lineHeight), juce::Justification::centredLeft);
}

//==============================================================================
// Hilfsfunktionen
//==============================================================================

juce::String SpectrumAnalyzer::formatFrequency(float freq) const
{
    if (freq >= 1000.0f)
        return juce::String(freq / 1000.0f, 1) + " kHz";
    else
        return juce::String(static_cast<int>(freq)) + " Hz";
}

juce::String SpectrumAnalyzer::formatDb(float db) const
{
    return juce::String(db, 1) + " dB";
}
