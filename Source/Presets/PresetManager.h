#pragma once

#include <JuceHeader.h>
#include "../Parameters/ParameterIDs.h"

/**
 * PresetManager: Verwaltet vordefinierte EQ-Presets
 */
class PresetManager
{
public:
    struct BandSettings
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 0.71f;
        float slope = 12.0f;
        ParameterIDs::FilterType type = ParameterIDs::FilterType::Bell;
        bool active = false;
        bool bypass = false;
    };

    struct PresetData
    {
        juce::String name;
        juce::String category;
        std::array<BandSettings, ParameterIDs::MAX_BANDS> bands;
    };

    // Vordefinierte Presets
    static juce::Array<PresetData> getBuiltInPresets()
    {
        juce::Array<PresetData> presets;

        // ===== Vocal Presets =====
        presets.add(createVocalWarmth());
        presets.add(createVocalCrispness());
        presets.add(createVocalDeSibilance());
        presets.add(createVocalBoost());

        // ===== Bass Presets =====
        presets.add(createBassBoost());
        presets.add(createBassTighten());
        presets.add(createBassCut());

        // ===== Drums Presets =====
        presets.add(createDrumPunch());
        presets.add(createDrumCrispness());
        presets.add(createDrumWarmth());

        // ===== Mix Presets =====
        presets.add(createClassicVinyl());
        presets.add(createModernBright());
        presets.add(createWarmAnalog());

        // ===== High/Low Pass Presets =====
        presets.add(createHighPassSoft());
        presets.add(createHighPassSteep());
        presets.add(createLowPassSoft());
        presets.add(createLowPassSteep());

        return presets;
    }

private:
    // ===== Vocal Presets =====
    static PresetData createVocalWarmth()
    {
        PresetData preset;
        preset.name = "Vocal Warmth";
        preset.category = "Vocals";

        preset.bands[0] = { 80.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 250.0f, 2.0f, 1.0f, 12.0f, ParameterIDs::FilterType::LowShelf, true };
        preset.bands[2] = { 1000.0f, 1.5f, 0.8f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[3] = { 5000.0f, -1.0f, 1.5f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[7] = { 16000.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    static PresetData createVocalCrispness()
    {
        PresetData preset;
        preset.name = "Vocal Crispness";
        preset.category = "Vocals";

        preset.bands[0] = { 80.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 200.0f, -1.0f, 1.2f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[3] = { 2500.0f, 3.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[4] = { 4000.0f, 2.0f, 0.8f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[5] = { 8000.0f, 1.0f, 1.5f, 12.0f, ParameterIDs::FilterType::Bell, true };

        return preset;
    }

    static PresetData createVocalDeSibilance()
    {
        PresetData preset;
        preset.name = "Vocal De-Sibilance";
        preset.category = "Vocals";

        preset.bands[0] = { 80.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[4] = { 5000.0f, -2.0f, 1.5f, 12.0f, ParameterIDs::FilterType::Notch, true };
        preset.bands[5] = { 7000.0f, -3.0f, 2.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[6] = { 10000.0f, -1.5f, 1.0f, 12.0f, ParameterIDs::FilterType::HighShelf, true };

        return preset;
    }

    static PresetData createVocalBoost()
    {
        PresetData preset;
        preset.name = "Vocal Boost";
        preset.category = "Vocals";

        preset.bands[0] = { 80.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 150.0f, -2.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[2] = { 500.0f, 1.0f, 0.8f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[3] = { 2000.0f, 2.5f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[4] = { 5000.0f, 3.0f, 0.9f, 12.0f, ParameterIDs::FilterType::Bell, true };

        return preset;
    }

    // ===== Bass Presets =====
    static PresetData createBassBoost()
    {
        PresetData preset;
        preset.name = "Bass Boost";
        preset.category = "Bass";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 80.0f, 3.0f, 0.8f, 12.0f, ParameterIDs::FilterType::LowShelf, true };
        preset.bands[2] = { 200.0f, 2.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[7] = { 16000.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    static PresetData createBassTighten()
    {
        PresetData preset;
        preset.name = "Bass Tighten";
        preset.category = "Bass";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 80.0f, -1.0f, 1.2f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[2] = { 250.0f, -2.0f, 1.5f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[3] = { 600.0f, 1.0f, 0.9f, 12.0f, ParameterIDs::FilterType::Bell, true };

        return preset;
    }

    static PresetData createBassCut()
    {
        PresetData preset;
        preset.name = "Bass Cut";
        preset.category = "Bass";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 100.0f, -3.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[2] = { 250.0f, -2.0f, 0.8f, 12.0f, ParameterIDs::FilterType::Bell, true };

        return preset;
    }

    // ===== Drum Presets =====
    static PresetData createDrumPunch()
    {
        PresetData preset;
        preset.name = "Drum Punch";
        preset.category = "Drums";

        preset.bands[1] = { 80.0f, 2.0f, 0.9f, 12.0f, ParameterIDs::FilterType::LowShelf, true };
        preset.bands[2] = { 250.0f, 1.5f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[3] = { 2000.0f, 2.0f, 1.2f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[4] = { 4000.0f, 1.5f, 0.8f, 12.0f, ParameterIDs::FilterType::Bell, true };

        return preset;
    }

    static PresetData createDrumCrispness()
    {
        PresetData preset;
        preset.name = "Drum Crispness";
        preset.category = "Drums";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[3] = { 1500.0f, 2.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[4] = { 5000.0f, 3.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[5] = { 10000.0f, 2.0f, 0.9f, 12.0f, ParameterIDs::FilterType::HighShelf, true };

        return preset;
    }

    static PresetData createDrumWarmth()
    {
        PresetData preset;
        preset.name = "Drum Warmth";
        preset.category = "Drums";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 100.0f, 2.0f, 1.0f, 12.0f, ParameterIDs::FilterType::LowShelf, true };
        preset.bands[2] = { 300.0f, 1.5f, 0.9f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[7] = { 16000.0f, 0.0f, 0.71f, 18.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    // ===== Mix Presets =====
    static PresetData createClassicVinyl()
    {
        PresetData preset;
        preset.name = "Classic Vinyl";
        preset.category = "Mix";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 80.0f, 1.0f, 0.8f, 12.0f, ParameterIDs::FilterType::LowShelf, true };
        preset.bands[2] = { 500.0f, -1.5f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[5] = { 8000.0f, 2.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[7] = { 16000.0f, 0.0f, 0.71f, 18.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    static PresetData createModernBright()
    {
        PresetData preset;
        preset.name = "Modern Bright";
        preset.category = "Mix";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 24.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 100.0f, -1.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[3] = { 2000.0f, 1.0f, 0.9f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[4] = { 5000.0f, 2.0f, 1.0f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[5] = { 10000.0f, 1.5f, 0.8f, 12.0f, ParameterIDs::FilterType::HighShelf, true };

        return preset;
    }

    static PresetData createWarmAnalog()
    {
        PresetData preset;
        preset.name = "Warm Analog";
        preset.category = "Mix";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[1] = { 80.0f, 2.0f, 0.8f, 12.0f, ParameterIDs::FilterType::LowShelf, true };
        preset.bands[2] = { 250.0f, 0.5f, 0.9f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[5] = { 8000.0f, -1.0f, 1.5f, 12.0f, ParameterIDs::FilterType::Bell, true };
        preset.bands[7] = { 16000.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    // ===== High/Low Pass Presets =====
    static PresetData createHighPassSoft()
    {
        PresetData preset;
        preset.name = "High Pass Soft (12dB)";
        preset.category = "Filters";

        preset.bands[0] = { 80.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[7] = { 16000.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    static PresetData createHighPassSteep()
    {
        PresetData preset;
        preset.name = "High Pass Steep (48dB)";
        preset.category = "Filters";

        preset.bands[0] = { 80.0f, 0.0f, 0.71f, 48.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[7] = { 16000.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    static PresetData createLowPassSoft()
    {
        PresetData preset;
        preset.name = "Low Pass Soft (12dB)";
        preset.category = "Filters";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[7] = { 8000.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }

    static PresetData createLowPassSteep()
    {
        PresetData preset;
        preset.name = "Low Pass Steep (48dB)";
        preset.category = "Filters";

        preset.bands[0] = { 30.0f, 0.0f, 0.71f, 12.0f, ParameterIDs::FilterType::LowCut, true };
        preset.bands[7] = { 5000.0f, 0.0f, 0.71f, 48.0f, ParameterIDs::FilterType::HighCut, true };

        return preset;
    }
};
