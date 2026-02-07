#pragma once

#include <JuceHeader.h>

namespace ParameterIDs
{
    // Maximale Anzahl der EQ-Bänder
    constexpr int MAX_BANDS = 12;

    // Parameter-ID Generator für EQ-Bänder
    inline juce::String getBandFreqID(int bandIndex) { return "band" + juce::String(bandIndex) + "_freq"; }
    inline juce::String getBandGainID(int bandIndex) { return "band" + juce::String(bandIndex) + "_gain"; }
    inline juce::String getBandQID(int bandIndex) { return "band" + juce::String(bandIndex) + "_q"; }
    inline juce::String getBandTypeID(int bandIndex) { return "band" + juce::String(bandIndex) + "_type"; }
    inline juce::String getBandBypassID(int bandIndex) { return "band" + juce::String(bandIndex) + "_bypass"; }
    inline juce::String getBandChannelID(int bandIndex) { return "band" + juce::String(bandIndex) + "_channel"; }
    inline juce::String getBandActiveID(int bandIndex) { return "band" + juce::String(bandIndex) + "_active"; }
    inline juce::String getBandSlopeID(int bandIndex) { return "band" + juce::String(bandIndex) + "_slope"; }

    // Dynamic EQ Parameter-ID Generator pro Band
    inline juce::String getBandDynEnabledID(int bandIndex) { return "band" + juce::String(bandIndex) + "_dyn_enabled"; }
    inline juce::String getBandDynThresholdID(int bandIndex) { return "band" + juce::String(bandIndex) + "_dyn_threshold"; }
    inline juce::String getBandDynRatioID(int bandIndex) { return "band" + juce::String(bandIndex) + "_dyn_ratio"; }
    inline juce::String getBandDynAttackID(int bandIndex) { return "band" + juce::String(bandIndex) + "_dyn_attack"; }
    inline juce::String getBandDynReleaseID(int bandIndex) { return "band" + juce::String(bandIndex) + "_dyn_release"; }

    // Globale Parameter
    const juce::String OUTPUT_GAIN = "output_gain";
    const juce::String INPUT_GAIN = "input_gain";
    const juce::String LINEAR_PHASE_MODE = "linear_phase";
    const juce::String MID_SIDE_MODE = "mid_side";
    const juce::String ANALYZER_PRE_POST = "analyzer_prepost";
    const juce::String ANALYZER_ON = "analyzer_on";

    //==========================================================================
    // Global Processing Parameters
    //==========================================================================
    const juce::String WET_DRY_MIX = "wet_dry_mix";
    const juce::String OVERSAMPLING_FACTOR = "oversampling_factor";
    const juce::String DELTA_MODE = "delta_mode";
    
    // Resonance Suppressor (Soothe-Style)
    const juce::String SUPPRESSOR_ENABLED = "suppressor_enabled";
    const juce::String SUPPRESSOR_DEPTH = "suppressor_depth";
    const juce::String SUPPRESSOR_SPEED = "suppressor_speed";
    const juce::String SUPPRESSOR_SELECTIVITY = "suppressor_selectivity";
    
    // Per-Band Solo
    inline juce::String getBandSoloID(int bandIndex) { return "band" + juce::String(bandIndex) + "_solo"; }

    //==========================================================================
    // Smart EQ Analyzer Parameter
    //==========================================================================
    const juce::String SMART_MODE_ENABLED = "smart_mode_enabled";

    //==========================================================================
    // LiveSmartEQ-Parameter
    //==========================================================================
    const juce::String LIVE_SMART_EQ_ENABLED = "live_smart_eq_enabled";
    const juce::String LIVE_SMART_EQ_DEPTH = "live_smart_eq_depth";
    const juce::String LIVE_SMART_EQ_ATTACK = "live_smart_eq_attack";
    const juce::String LIVE_SMART_EQ_RELEASE = "live_smart_eq_release";
    const juce::String LIVE_SMART_EQ_THRESHOLD = "live_smart_eq_threshold";
    const juce::String LIVE_SMART_EQ_MODE = "live_smart_eq_mode";
    const juce::String LIVE_SMART_EQ_MAX_REDUCTION = "live_smart_eq_max_reduction";
    const juce::String LIVE_SMART_EQ_TRANSIENT_PROTECT = "live_smart_eq_transient";
    const juce::String LIVE_SMART_EQ_MS_MODE = "live_smart_eq_ms_mode";
    const juce::String LIVE_SMART_EQ_PROFILE = "live_smart_eq_profile";

    // LiveSmartEQ-Modi
    inline juce::StringArray getLiveSmartEQModeNames()
    {
        return { "Gentle", "Normal", "Aggressive", "Custom" };
    }
    
    // LiveSmartEQ Mid/Side Modi
    inline juce::StringArray getLiveSmartEQMSModeNames()
    {
        return { "Stereo", "Mid", "Side", "Auto M/S" };
    }

    //==========================================================================
    // Analyzer-Parameter (Pro-Q Style)
    //==========================================================================
    const juce::String ANALYZER_RESOLUTION = "analyzer_resolution";
    const juce::String ANALYZER_RANGE = "analyzer_range";
    const juce::String ANALYZER_SPEED = "analyzer_speed";
    const juce::String ANALYZER_TILT = "analyzer_tilt";
    const juce::String ANALYZER_TILT_ENABLED = "analyzer_tilt_enabled";
    const juce::String ANALYZER_FREEZE = "analyzer_freeze";
    const juce::String ANALYZER_SHOW_PEAKS = "analyzer_show_peaks";

    // Analyzer-Resolution-Optionen
    inline juce::StringArray getAnalyzerResolutionNames()
    {
        return { "Low (1024)", "Medium (2048)", "High (4096)", "Maximum (8192)" };
    }

    // Analyzer-Range-Optionen (dB)
    inline juce::StringArray getAnalyzerRangeNames()
    {
        return { "60 dB", "90 dB", "120 dB" };
    }

    // Analyzer-Speed-Optionen
    inline juce::StringArray getAnalyzerSpeedNames()
    {
        return { "Very Slow", "Slow", "Medium", "Fast", "Very Fast" };
    }

    // Standard-Tilt (Pro-Q Style: 4.5 dB/Oktave)
    constexpr float DEFAULT_ANALYZER_TILT = 4.5f;
    constexpr float MIN_ANALYZER_TILT = -6.0f;
    constexpr float MAX_ANALYZER_TILT = 6.0f;

    // Filtertypen
    enum class FilterType
    {
        Bell = 0,
        LowShelf,
        HighShelf,
        LowCut,
        HighCut,
        Notch,
        BandPass,
        TiltShelf,
        AllPass,
        FlatTilt,
        NumTypes
    };

    // Filtertyp-Namen
    inline juce::StringArray getFilterTypeNames()
    {
        return { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "All Pass", "Flat Tilt" };
    }

    // Kanal-Modi
    enum class ChannelMode
    {
        Stereo = 0,
        Left,
        Right,
        Mid,
        Side,
        NumModes
    };

    inline juce::StringArray getChannelModeNames()
    {
        return { "Stereo", "Left", "Right", "Mid", "Side" };
    }

    // Frequenzbereich
    constexpr float MIN_FREQUENCY = 20.0f;
    constexpr float MAX_FREQUENCY = 20000.0f;
    constexpr float DEFAULT_FREQUENCY = 1000.0f;

    // Gain-Bereich
    constexpr float MIN_GAIN = -30.0f;
    constexpr float MAX_GAIN = 30.0f;
    constexpr float DEFAULT_GAIN = 0.0f;

    // Q-Bereich
    constexpr float MIN_Q = 0.1f;
    constexpr float MAX_Q = 18.0f;
    constexpr float DEFAULT_Q = 0.71f;

    // Slope-Bereich für Cut-Filter (in dB/Oktave)
    // 12dB = 1.Ordnung, 24dB = 2.Ordnung, 48dB = 4.Ordnung, 96dB = 8.Ordnung
    constexpr float MIN_SLOPE = 6.0f;   // 6 dB/oct
    constexpr float MAX_SLOPE = 96.0f;  // 96 dB/oct (sehr steil)
    constexpr float DEFAULT_SLOPE = 12.0f;  // 12 dB/oct (1. Ordnung)

    inline juce::StringArray getSlopeNames()
    {
        return { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct" };
    }

    // Standard-Frequenzen für die 12 Bänder (beim Start nicht verwendet)
    // Bänder 1-4: Manuell, Bänder 5-12: Smart/Live EQ
    constexpr float DEFAULT_FREQUENCIES[MAX_BANDS] = {
        100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 12000.0f,
        60.0f, 300.0f, 1500.0f, 6000.0f
    };

    // Standard-Filtertypen für die 12 Bänder (alle Bell, da nicht voreingestellt)
    constexpr FilterType DEFAULT_TYPES[MAX_BANDS] = {
        FilterType::Bell, FilterType::Bell, FilterType::Bell, FilterType::Bell,
        FilterType::Bell, FilterType::Bell, FilterType::Bell, FilterType::Bell,
        FilterType::Bell, FilterType::Bell, FilterType::Bell, FilterType::Bell
    };
}

