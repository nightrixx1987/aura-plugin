#include "BiquadFilter.h"
#include <cmath>

BiquadFilter::BiquadFilter()
{
    reset();
}

void BiquadFilter::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    reset();
}

void BiquadFilter::reset()
{
    z1 = 0.0;
    z2 = 0.0;
    
    // Smoothing-Koeffizienten sofort auf Zielwerte setzen
    // um Einschwingeffekte zu vermeiden
    smoothedB0 = nb0;
    smoothedB1 = nb1;
    smoothedB2 = nb2;
    smoothedA1 = na1;
    smoothedA2 = na2;
    coefficientsInitialized = true;
}

void BiquadFilter::updateCoefficients(ParameterIDs::FilterType type,
                                       float frequency,
                                       float gainDB,
                                       float Q,
                                       int /*slope*/)
{
    currentType = type;
    currentFrequency = frequency;
    currentGain = gainDB;
    currentQ = Q;

    // Frequenz auf gültigen Bereich begrenzen
    frequency = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.499), frequency);
    Q = juce::jlimit(0.1f, 18.0f, Q);

    switch (type)
    {
        case ParameterIDs::FilterType::Bell:
            calculateBellCoefficients(frequency, gainDB, Q);
            break;
        case ParameterIDs::FilterType::LowShelf:
            calculateLowShelfCoefficients(frequency, gainDB, Q);
            break;
        case ParameterIDs::FilterType::HighShelf:
            calculateHighShelfCoefficients(frequency, gainDB, Q);
            break;
        case ParameterIDs::FilterType::LowCut:
            calculateLowCutCoefficients(frequency, Q);
            break;
        case ParameterIDs::FilterType::HighCut:
            calculateHighCutCoefficients(frequency, Q);
            break;
        case ParameterIDs::FilterType::Notch:
            calculateNotchCoefficients(frequency, Q);
            break;
        case ParameterIDs::FilterType::BandPass:
            calculateBandPassCoefficients(frequency, Q);
            break;
        case ParameterIDs::FilterType::TiltShelf:
            calculateTiltShelfCoefficients(frequency, gainDB);
            break;
        case ParameterIDs::FilterType::AllPass:
            calculateAllPassCoefficients(frequency, Q);
            break;
        case ParameterIDs::FilterType::FlatTilt:
            calculateFlatTiltCoefficients(frequency, gainDB);
            break;
        default:
            // Bypass (Unity)
            b0 = 1.0; b1 = 0.0; b2 = 0.0;
            a0 = 1.0; a1 = 0.0; a2 = 0.0;
            break;
    }

    normalizeCoefficients();
}

float BiquadFilter::processSample(float input) noexcept
{
    // OPTIMIERUNG: Conditional Smoothing - nur wenn Koeffizienten sich ändern
    if (needsSmoothing)
    {
        smoothedB0 = smoothingCoeff * smoothedB0 + (1.0 - smoothingCoeff) * nb0;
        smoothedB1 = smoothingCoeff * smoothedB1 + (1.0 - smoothingCoeff) * nb1;
        smoothedB2 = smoothingCoeff * smoothedB2 + (1.0 - smoothingCoeff) * nb2;
        smoothedA1 = smoothingCoeff * smoothedA1 + (1.0 - smoothingCoeff) * na1;
        smoothedA2 = smoothingCoeff * smoothedA2 + (1.0 - smoothingCoeff) * na2;
        
        // Prüfe ob Konvergenz erreicht ist
        if (std::abs(smoothedB0 - nb0) < smoothingEpsilon &&
            std::abs(smoothedB1 - nb1) < smoothingEpsilon &&
            std::abs(smoothedB2 - nb2) < smoothingEpsilon &&
            std::abs(smoothedA1 - na1) < smoothingEpsilon &&
            std::abs(smoothedA2 - na2) < smoothingEpsilon)
        {
            // Konvergiert - setze exakte Werte und deaktiviere Smoothing
            smoothedB0 = nb0;
            smoothedB1 = nb1;
            smoothedB2 = nb2;
            smoothedA1 = na1;
            smoothedA2 = na2;
            needsSmoothing = false;
        }
    }
    
    // Transposed Direct Form II mit geglätteten Koeffizienten
    double output = smoothedB0 * input + z1;
    z1 = smoothedB1 * input - smoothedA1 * output + z2;
    z2 = smoothedB2 * input - smoothedA2 * output;
    
    // Anti-Denormal Protection (verhindert CPU-Spikes bei sehr kleinen Werten)
    if (std::abs(z1) < ANTI_DENORMAL) z1 = 0.0;
    if (std::abs(z2) < ANTI_DENORMAL) z2 = 0.0;
    
    return static_cast<float>(output);
}

void BiquadFilter::processBlock(float* data, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        data[i] = processSample(data[i]);
    }
}

float BiquadFilter::getMagnitudeForFrequency(float frequency) const noexcept
{
    // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (a0 + a1*z^-1 + a2*z^-2)
    // Bei z = e^(j*omega), omega = 2*pi*f/fs
    
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double cosw = std::cos(omega);
    const double sinw = std::sin(omega);
    const double cos2w = std::cos(2.0 * omega);
    const double sin2w = std::sin(2.0 * omega);

    // Zähler: b0 + b1*e^(-jw) + b2*e^(-j2w)
    double numReal = nb0 + nb1 * cosw + nb2 * cos2w;
    double numImag = -nb1 * sinw - nb2 * sin2w;

    // Nenner: 1 + a1*e^(-jw) + a2*e^(-j2w)
    double denReal = 1.0 + na1 * cosw + na2 * cos2w;
    double denImag = -na1 * sinw - na2 * sin2w;

    // |H(jw)|^2
    double numMag2 = numReal * numReal + numImag * numImag;
    double denMag2 = denReal * denReal + denImag * denImag;

    if (denMag2 < 1e-10)
        denMag2 = 1e-10;

    double magnitude = std::sqrt(numMag2 / denMag2);
    
    // In dB umwandeln
    return static_cast<float>(20.0 * std::log10(std::max(magnitude, 1e-10)));
}

float BiquadFilter::getPhaseForFrequency(float frequency) const noexcept
{
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double cosw = std::cos(omega);
    const double sinw = std::sin(omega);
    const double cos2w = std::cos(2.0 * omega);
    const double sin2w = std::sin(2.0 * omega);

    double numReal = nb0 + nb1 * cosw + nb2 * cos2w;
    double numImag = -nb1 * sinw - nb2 * sin2w;
    double denReal = 1.0 + na1 * cosw + na2 * cos2w;
    double denImag = -na1 * sinw - na2 * sin2w;

    double numPhase = std::atan2(numImag, numReal);
    double denPhase = std::atan2(denImag, denReal);

    return static_cast<float>(numPhase - denPhase);
}

void BiquadFilter::normalizeCoefficients()
{
    if (std::abs(a0) < 1e-10)
        a0 = 1.0;

    nb0 = b0 / a0;
    nb1 = b1 / a0;
    nb2 = b2 / a0;
    na1 = a1 / a0;
    na2 = a2 / a0;
    
    // Beim allerersten Aufruf die smoothed Koeffizienten sofort setzen
    // um Einschwingartefakte zu vermeiden
    if (!coefficientsInitialized)
    {
        smoothedB0 = nb0;
        smoothedB1 = nb1;
        smoothedB2 = nb2;
        smoothedA1 = na1;
        smoothedA2 = na2;
        coefficientsInitialized = true;
        needsSmoothing = false;
    }
    else
    {
        // OPTIMIERUNG: Smoothing nur aktivieren wenn Koeffizienten sich signifikant ändern
        needsSmoothing = (std::abs(smoothedB0 - nb0) > smoothingEpsilon ||
                          std::abs(smoothedB1 - nb1) > smoothingEpsilon ||
                          std::abs(smoothedB2 - nb2) > smoothingEpsilon ||
                          std::abs(smoothedA1 - na1) > smoothingEpsilon ||
                          std::abs(smoothedA2 - na2) > smoothingEpsilon);
    }
}

// ============================================================================
// Koeffizienten-Berechnungen nach Robert Bristow-Johnson's Audio EQ Cookbook
// ============================================================================

void BiquadFilter::calculateBellCoefficients(float frequency, float gainDB, float Q)
{
    const double A = std::pow(10.0, gainDB / 40.0);  // sqrt(10^(dB/20))
    
    // Bilinear Transform Frequency Prewarping
    // Verwende tan(omega/2) direkt mit Halbwinkel-Identitäten:
    //   sin(omega) = 2*t/(1+t²),  cos(omega) = (1-t²)/(1+t²)  wo t = tan(omega/2)
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);

    b0 = 1.0 + alpha * A;
    b1 = -2.0 * cosw;
    b2 = 1.0 - alpha * A;
    a0 = 1.0 + alpha / A;
    a1 = -2.0 * cosw;
    a2 = 1.0 - alpha / A;
}

void BiquadFilter::calculateLowShelfCoefficients(float frequency, float gainDB, float Q)
{
    const double A = std::pow(10.0, gainDB / 40.0);
    
    // Bilinear Transform Frequency Prewarping
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);
    const double sqrtA = std::sqrt(A);

    b0 = A * ((A + 1.0) - (A - 1.0) * cosw + 2.0 * sqrtA * alpha);
    b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw);
    b2 = A * ((A + 1.0) - (A - 1.0) * cosw - 2.0 * sqrtA * alpha);
    a0 = (A + 1.0) + (A - 1.0) * cosw + 2.0 * sqrtA * alpha;
    a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw);
    a2 = (A + 1.0) + (A - 1.0) * cosw - 2.0 * sqrtA * alpha;
}

void BiquadFilter::calculateHighShelfCoefficients(float frequency, float gainDB, float Q)
{
    const double A = std::pow(10.0, gainDB / 40.0);
    
    // Bilinear Transform Frequency Prewarping
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);
    const double sqrtA = std::sqrt(A);

    b0 = A * ((A + 1.0) + (A - 1.0) * cosw + 2.0 * sqrtA * alpha);
    b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw);
    b2 = A * ((A + 1.0) + (A - 1.0) * cosw - 2.0 * sqrtA * alpha);
    a0 = (A + 1.0) - (A - 1.0) * cosw + 2.0 * sqrtA * alpha;
    a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosw);
    a2 = (A + 1.0) - (A - 1.0) * cosw - 2.0 * sqrtA * alpha;
}

void BiquadFilter::calculateLowCutCoefficients(float frequency, float Q)
{
    // High-Pass Filter (2nd order) mit Bilinear Prewarping
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);

    b0 = (1.0 + cosw) / 2.0;
    b1 = -(1.0 + cosw);
    b2 = (1.0 + cosw) / 2.0;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosw;
    a2 = 1.0 - alpha;
}

void BiquadFilter::calculateHighCutCoefficients(float frequency, float Q)
{
    // Low-Pass Filter (2nd order) mit Bilinear Prewarping
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);

    b0 = (1.0 - cosw) / 2.0;
    b1 = 1.0 - cosw;
    b2 = (1.0 - cosw) / 2.0;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosw;
    a2 = 1.0 - alpha;
}

void BiquadFilter::calculateNotchCoefficients(float frequency, float Q)
{
    // Bilinear Transform Frequency Prewarping
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);

    b0 = 1.0;
    b1 = -2.0 * cosw;
    b2 = 1.0;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosw;
    a2 = 1.0 - alpha;
}

void BiquadFilter::calculateBandPassCoefficients(float frequency, float Q)
{
    // BPF mit konstantem Peak-Gain + Bilinear Prewarping
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);

    b0 = alpha;
    b1 = 0.0;
    b2 = -alpha;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosw;
    a2 = 1.0 - alpha;
}

void BiquadFilter::calculateTiltShelfCoefficients(float frequency, float gainDB)
{
    // Tilt EQ: Low Shelf bei frequency mit +gainDB, kombiniert mit
    // implizitem High Shelf bei gleicher Frequenz mit -gainDB
    // Vereinfachte Implementierung als Low Shelf mit breitem Q
    calculateLowShelfCoefficients(frequency, gainDB, 0.5f);
}

void BiquadFilter::calculateAllPassCoefficients(float frequency, float Q)
{
    // All-Pass Filter: Einheits-Amplitude, nur Phasenverschiebung
    // Bilinear Transform Frequency Prewarping
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double t = std::tan(omega / 2.0);
    const double t2 = t * t;
    const double denom = 1.0 + t2;
    const double sinw = 2.0 * t / denom;
    const double cosw = (1.0 - t2) / denom;
    const double alpha = sinw / (2.0 * Q);

    b0 = 1.0 - alpha;
    b1 = -2.0 * cosw;
    b2 = 1.0 + alpha;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosw;
    a2 = 1.0 - alpha;
}

void BiquadFilter::calculateFlatTiltCoefficients(float frequency, float gainDB)
{
    // Flat Tilt: Symmetrisches Frequenzkippen um die Zielfrequenz
    // Implementiert als 1st-order Shelf mit sanftem 3 dB/oct Slope
    // gainDB > 0: Höhen boost, Tiefen cut  |  gainDB < 0: umgekehrt
    const double A = std::pow(10.0, gainDB / 40.0);
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double tanOmegaHalf = std::tan(omega / 2.0);
    
    // 1st-order shelving filter (sanfterer Slope als TiltShelf)
    const double sqrtA = std::sqrt(A);
    const double t = tanOmegaHalf;
    
    b0 = sqrtA * t + A;
    b1 = sqrtA * t - A;
    b2 = 0.0;
    a0 = sqrtA * t + 1.0;
    a1 = sqrtA * t - 1.0;
    a2 = 0.0;
}
