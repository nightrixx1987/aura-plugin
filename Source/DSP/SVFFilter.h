#pragma once

#include <JuceHeader.h>
#include "../Parameters/ParameterIDs.h"
#include <cmath>

/**
 * SVFFilter: State Variable Filter (Topology-Preserving Transform)
 * 
 * Basiert auf Vadim Zavalishin / Cytomic SVF Paper:
 * "The Art of VA Filter Design" und Andrew Simper's TPT SVF.
 * 
 * Vorteile gegenüber Biquad für Dynamic EQ:
 * - Modulationsstabil: Parameter können sample-genau geändert werden
 *   ohne Artefakte (kein Zipper-Rauschen, keine Clicks)
 * - Interne State-Variables bleiben konsistent bei Parameteränderungen
 * - Kein aufwändiges Koeffizienten-Smoothing nötig
 * - Numerisch stabiler bei hohen Frequenzen nahe Nyquist
 * 
 * Unterstützte Filtertypen: Bell, LowShelf, HighShelf, LowPass, HighPass,
 * BandPass, Notch, AllPass
 * 
 * Dieses Filter wird primär für Dynamic EQ Bands verwendet, wo sich
 * der Gain auf Basis des Envelope Followers ständig ändert.
 */
class SVFFilter
{
public:
    SVFFilter() = default;
    ~SVFFilter() = default;

    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        ic1eq = 0.0;
        ic2eq = 0.0;
    }

    /**
     * Setzt alle Filter-Parameter auf einmal.
     * Kann direkt im Audio-Thread aufgerufen werden — SVF ist modulationsstabil.
     */
    void setParameters(ParameterIDs::FilterType type, float frequency, float gainDB, float Q)
    {
        currentType = type;
        currentGainDB = gainDB;
        currentQ = Q;
        currentFrequency = frequency;
        
        // Frequenz auf gültigen Bereich begrenzen
        frequency = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.499), frequency);
        Q = juce::jlimit(0.1f, 18.0f, Q);

        // SVF Kernkoeffizienten nach Cytomic/Zavalishin
        // g = tan(pi * fc / fs) — Bilinear-Warping ist inhärent
        cachedG = std::tan(juce::MathConstants<double>::pi * frequency / sampleRate);
        double k = 1.0 / static_cast<double>(Q);
        
        computeMixCoefficients(type, gainDB, Q, cachedG, k);
    }

    /**
     * Effiziente Gain-Only-Aktualisierung für Dynamic EQ.
     * Überspringt die teure tan()-Berechnung — nur Mix-Koeffizienten werden neu berechnet.
     * Voraussetzung: setParameters() wurde mindestens einmal mit richtiger Frequenz/Q aufgerufen.
     */
    void updateGainOnly(float gainDB)
    {
        if (std::abs(gainDB - currentGainDB) < 0.01f)
            return;  // Keine signifikante Änderung
        
        currentGainDB = gainDB;
        double k = 1.0 / static_cast<double>(currentQ);
        computeMixCoefficients(currentType, gainDB, currentQ, cachedG, k);
    }

    /**
     * Prüft ob sich Frequenz, Q oder Filtertyp geändert haben.
     * Wenn ja, muss setParameters() aufgerufen werden (inkl. teurem tan()).
     * Wenn nur der Gain sich ändert, reicht updateGainOnly().
     */
    bool needsFullUpdate(ParameterIDs::FilterType type, float frequency, float Q) const noexcept
    {
        return type != currentType
            || std::abs(frequency - currentFrequency) > 0.01f
            || std::abs(Q - currentQ) > 0.001f;
    }

    /**
     * Verarbeitet ein einzelnes Sample.
     * Topology-Preserving Transform: interne Zustände bleiben
     * auch bei Parameteränderungen zwischen Samples konsistent.
     */
    float processSample(float input) noexcept
    {
        double v0 = static_cast<double>(input);
        
        // TPT SVF Tick (zero-delay feedback)
        double v3 = v0 - ic2eq;
        double v1 = a1 * ic1eq + a2 * v3;
        double v2 = ic2eq + a2 * ic1eq + a3 * v3;
        
        // State Update
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;
        
        // Output Mix: HP*m0 + BP*m1 + LP*m2
        // v0 - k*v1 - v2 = HP, v1 = BP, v2 = LP
        double output = m0 * v0 + m1 * v1 + m2 * v2;
        
        // Anti-Denormal Protection
        if (std::abs(ic1eq) < 1e-15) ic1eq = 0.0;
        if (std::abs(ic2eq) < 1e-15) ic2eq = 0.0;
        
        return static_cast<float>(output);
    }

    /**
     * Block-Processing für Effizienz.
     */
    void processBlock(float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            data[i] = processSample(data[i]);
        }
    }

    /**
     * Berechnet die Magnitude-Antwort für eine gegebene Frequenz (in dB).
     * Verwendet die analytische Übertragungsfunktion des SVF.
     */
    float getMagnitudeForFrequency(float frequency) const noexcept
    {
        const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
        const double cosw = std::cos(omega);
        const double sinw = std::sin(omega);

        // Berechne g und k aus den gespeicherten Koeffizienten
        // Direkte Z-Domain Analyse des SVF
        // Nutze die gespeicherten Mix-Koeffizienten m0, m1, m2
        // H(z) = m0*HP(z) + m1*BP(z) + m2*LP(z) wobei HP+BP+LP = Input
        
        // Für die Magnitude-Berechnung verwenden wir die Biquad-äquivalente
        // Übertragungsfunktion, da SVF und Biquad identische Frequenzgänge haben
        // Dies ist eine Approximation, die für Anzeigequalität ausreichend ist
        
        // Vereinfachte Berechnung über die SVF Koeffizienten
        double g2 = a3 / (a1 > 1e-10 ? a1 : 1e-10);  // g² = a3/a1 (aus SVF-Struktur)
        double g = std::sqrt(std::abs(g2));
        
        // Analog-Prototyp Frequenz-Antwort evaluieren
        double s = sinw / (1.0 + cosw);  // tan(omega/2) ≈ Bilinear-Analogie
        double s2 = s * s;
        
        double k_val = a2 / (a1 > 1e-10 ? a1 : 1e-10) / g;  // k = a2/(a1*g)
        
        // LP = g²/(1 + k*g*s + g²*s²)  (vereinfacht)
        // BP = g*s/(...)
        // HP = s²/(...)
        double denomReal = 1.0 - g2 * s2;
        double denomImag = k_val * g * s;
        double denomMag2 = denomReal * denomReal + denomImag * denomImag;
        
        if (denomMag2 < 1e-20) denomMag2 = 1e-20;
        
        // Numerator basierend auf Mix-Koeffizienten
        double numReal = m0 * s2 + m2 * g2 - m0 * g2 * s2;
        double numImag = m1 * g * s;
        double numMag2 = numReal * numReal + numImag * numImag;
        
        double magnitude = std::sqrt(numMag2 / denomMag2);
        return static_cast<float>(20.0 * std::log10(std::max(magnitude, 1e-10)));
    }

private:
    void computeMixCoefficients(ParameterIDs::FilterType type, float gainDB, float Q, double g, double k)
    {
        switch (type)
        {
            case ParameterIDs::FilterType::Bell:
            {
                double A = std::pow(10.0, static_cast<double>(gainDB) / 40.0);
                double kBoost = 1.0 / (static_cast<double>(Q) * A);
                double kCut = 1.0 / (static_cast<double>(Q) / A);
                
                if (gainDB >= 0.0f)
                {
                    a1 = 1.0 / (1.0 + g * kBoost + g * g);
                    a2 = g * a1;
                    a3 = g * a2;
                    m0 = 1.0;
                    m1 = kBoost * (A * A - 1.0);
                    m2 = 0.0;
                }
                else
                {
                    a1 = 1.0 / (1.0 + g * kCut + g * g);
                    a2 = g * a1;
                    a3 = g * a2;
                    m0 = 1.0;
                    m1 = kCut * (1.0 / (A * A) - 1.0);
                    m2 = 0.0;
                }
                break;
            }
            
            case ParameterIDs::FilterType::LowShelf:
            {
                double A = std::pow(10.0, static_cast<double>(gainDB) / 40.0);
                if (gainDB >= 0.0f)
                {
                    a1 = 1.0 / (1.0 + g * k + g * g);
                    a2 = g * a1;
                    a3 = g * a2;
                    m0 = 1.0;
                    m1 = k * (A - 1.0);
                    m2 = A * A - 1.0;
                }
                else
                {
                    double invA = 1.0 / A;
                    double gA = g * std::sqrt(A);
                    a1 = 1.0 / (1.0 + gA * k + gA * gA);
                    a2 = gA * a1;
                    a3 = gA * a2;
                    m0 = 1.0;
                    m1 = k * (invA - 1.0);
                    m2 = (1.0 / (A * A)) - 1.0;
                }
                break;
            }
            
            case ParameterIDs::FilterType::HighShelf:
            {
                double A = std::pow(10.0, static_cast<double>(gainDB) / 40.0);
                if (gainDB >= 0.0f)
                {
                    a1 = 1.0 / (1.0 + g * k + g * g);
                    a2 = g * a1;
                    a3 = g * a2;
                    m0 = A * A;
                    m1 = k * (1.0 - A) * A;
                    m2 = 1.0 - A * A;
                }
                else
                {
                    double invA = 1.0 / A;
                    double gIA = g * std::sqrt(invA);
                    a1 = 1.0 / (1.0 + gIA * k + gIA * gIA);
                    a2 = gIA * a1;
                    a3 = gIA * a2;
                    m0 = 1.0 / (A * A);
                    m1 = k * (A - 1.0) / A;
                    m2 = A * A - 1.0;
                }
                break;
            }
            
            case ParameterIDs::FilterType::LowCut:
            {
                a1 = 1.0 / (1.0 + g * k + g * g);
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0; m1 = -k; m2 = -1.0;
                break;
            }
            
            case ParameterIDs::FilterType::HighCut:
            {
                a1 = 1.0 / (1.0 + g * k + g * g);
                a2 = g * a1;
                a3 = g * a2;
                m0 = 0.0; m1 = 0.0; m2 = 1.0;
                break;
            }
            
            case ParameterIDs::FilterType::Notch:
            {
                a1 = 1.0 / (1.0 + g * k + g * g);
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0; m1 = -k; m2 = 0.0;
                break;
            }
            
            case ParameterIDs::FilterType::BandPass:
            {
                a1 = 1.0 / (1.0 + g * k + g * g);
                a2 = g * a1;
                a3 = g * a2;
                m0 = 0.0; m1 = 1.0; m2 = 0.0;
                break;
            }
            
            case ParameterIDs::FilterType::AllPass:
            {
                a1 = 1.0 / (1.0 + g * k + g * g);
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0; m1 = -2.0 * k; m2 = 0.0;
                break;
            }
            
            default:
            {
                a1 = 1.0; a2 = 0.0; a3 = 0.0;
                m0 = 1.0; m1 = 0.0; m2 = 0.0;
                break;
            }
        }
    }

    double sampleRate = 44100.0;
    ParameterIDs::FilterType currentType = ParameterIDs::FilterType::Bell;
    float currentGainDB = 0.0f;
    float currentQ = 0.71f;
    float currentFrequency = 1000.0f;
    double cachedG = 0.0;  // tan(pi * fc / fs) — gecacht für updateGainOnly()
    
    // SVF Koeffizienten
    double a1 = 1.0, a2 = 0.0, a3 = 0.0;
    
    // Output-Mix-Koeffizienten: Output = m0*v0 + m1*v1 + m2*v2
    // (HP, BP, LP Anteile)
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;
    
    // State Variables (Integrator-Zustände)
    double ic1eq = 0.0;  // 1st Integrator state
    double ic2eq = 0.0;  // 2nd Integrator state

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SVFFilter)
};
