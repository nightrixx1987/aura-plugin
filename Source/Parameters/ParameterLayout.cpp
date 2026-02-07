#include "ParameterLayout.h"

namespace ParameterLayout
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // Frequenz-Skew für logarithmische Skalierung
        auto freqRange = juce::NormalisableRange<float>(
            ParameterIDs::MIN_FREQUENCY,
            ParameterIDs::MAX_FREQUENCY,
            [](float start, float end, float normalised) {
                return start * std::pow(end / start, normalised);
            },
            [](float start, float end, float value) {
                return std::log(value / start) / std::log(end / start);
            }
        );

        // Q-Skew für logarithmische Skalierung
        auto qRange = juce::NormalisableRange<float>(
            ParameterIDs::MIN_Q,
            ParameterIDs::MAX_Q,
            [](float start, float end, float normalised) {
                return start * std::pow(end / start, normalised);
            },
            [](float start, float end, float value) {
                return std::log(value / start) / std::log(end / start);
            }
        );

        // Gain-Range (linear)
        auto gainRange = juce::NormalisableRange<float>(
            ParameterIDs::MIN_GAIN,
            ParameterIDs::MAX_GAIN,
            0.1f  // Step size
        );

        // Parameter für jedes EQ-Band erstellen
        for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
        {
            // Frequenz-Parameter
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandFreqID(i), 1),
                "Band " + juce::String(i + 1) + " Frequency",
                freqRange,
                ParameterIDs::DEFAULT_FREQUENCIES[i],
                juce::AudioParameterFloatAttributes()
                    .withLabel("Hz")
                    .withStringFromValueFunction([](float value, int) {
                        if (value >= 1000.0f)
                            return juce::String(value / 1000.0f, 2) + " kHz";
                        return juce::String(static_cast<int>(value)) + " Hz";
                    })
            ));

            // Gain-Parameter
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandGainID(i), 1),
                "Band " + juce::String(i + 1) + " Gain",
                gainRange,
                ParameterIDs::DEFAULT_GAIN,
                juce::AudioParameterFloatAttributes()
                    .withLabel("dB")
                    .withStringFromValueFunction([](float value, int) {
                        return juce::String(value, 1) + " dB";
                    })
            ));

            // Q-Parameter
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandQID(i), 1),
                "Band " + juce::String(i + 1) + " Q",
                qRange,
                ParameterIDs::DEFAULT_Q,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([](float value, int) {
                        return juce::String(value, 2);
                    })
            ));

            // Filtertyp-Parameter
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(ParameterIDs::getBandTypeID(i), 1),
                "Band " + juce::String(i + 1) + " Type",
                ParameterIDs::getFilterTypeNames(),
                static_cast<int>(ParameterIDs::DEFAULT_TYPES[i])
            ));

            // Bypass-Parameter
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(ParameterIDs::getBandBypassID(i), 1),
                "Band " + juce::String(i + 1) + " Bypass",
                false
            ));

            // Kanal-Parameter (Stereo, L, R, Mid, Side)
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(ParameterIDs::getBandChannelID(i), 1),
                "Band " + juce::String(i + 1) + " Channel",
                ParameterIDs::getChannelModeNames(),
                static_cast<int>(ParameterIDs::ChannelMode::Stereo)
            ));

            // Aktiv-Parameter (KRITISCH für Band-Sichtbarkeit)
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(ParameterIDs::getBandActiveID(i), 1),
                "Band " + juce::String(i + 1) + " Active",
                false  // Default: inaktiv bis der Benutzer es bearbeitet
            ));

            // Slope-Parameter für Cut-Filter (steilheit)
            auto slopeRange = juce::NormalisableRange<float>(
                ParameterIDs::MIN_SLOPE,
                ParameterIDs::MAX_SLOPE,
                [](float start, float end, float normalised) {
                    return start * std::pow(end / start, normalised);
                },
                [](float start, float end, float value) {
                    return std::log(value / start) / std::log(end / start);
                }
            );

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandSlopeID(i), 1),
                "Band " + juce::String(i + 1) + " Slope",
                slopeRange,
                ParameterIDs::DEFAULT_SLOPE,
                juce::AudioParameterFloatAttributes()
                    .withLabel("dB/oct")
                    .withStringFromValueFunction([](float value, int) {
                        return juce::String(static_cast<int>(value)) + " dB/oct";
                    })
            ));

            // ===== Dynamic EQ Parameter pro Band =====
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(ParameterIDs::getBandDynEnabledID(i), 1),
                "Band " + juce::String(i + 1) + " Dynamic",
                false
            ));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandDynThresholdID(i), 1),
                "Band " + juce::String(i + 1) + " Dyn Threshold",
                juce::NormalisableRange<float>(-60.0f, 0.0f, 0.5f),
                -20.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel("dB")
                    .withStringFromValueFunction([](float value, int) {
                        return juce::String(value, 1) + " dB";
                    })
            ));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandDynRatioID(i), 1),
                "Band " + juce::String(i + 1) + " Dyn Ratio",
                juce::NormalisableRange<float>(1.0f, 10.0f, 0.1f),
                2.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([](float value, int) {
                        return juce::String(value, 1) + ":1";
                    })
            ));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandDynAttackID(i), 1),
                "Band " + juce::String(i + 1) + " Dyn Attack",
                juce::NormalisableRange<float>(0.1f, 500.0f, 0.1f, 0.4f),
                10.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel("ms")
                    .withStringFromValueFunction([](float value, int) {
                        return juce::String(value, 1) + " ms";
                    })
            ));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ParameterIDs::getBandDynReleaseID(i), 1),
                "Band " + juce::String(i + 1) + " Dyn Release",
                juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.4f),
                100.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel("ms")
                    .withStringFromValueFunction([](float value, int) {
                        return juce::String(static_cast<int>(value)) + " ms";
                    })
            ));

            // ===== Per-Band Solo =====
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(ParameterIDs::getBandSoloID(i), 1),
                "Band " + juce::String(i + 1) + " Solo",
                false
            ));
        }

        // Globaler Output-Gain
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::OUTPUT_GAIN, 1),
            "Output Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(value, 1) + " dB";
                })
        ));

        // Globaler Input-Gain
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::INPUT_GAIN, 1),
            "Input Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(value, 1) + " dB";
                })
        ));

        // Linear Phase Mode
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::LINEAR_PHASE_MODE, 1),
            "Linear Phase",
            false
        ));

        // Mid/Side Mode
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::MID_SIDE_MODE, 1),
            "Mid/Side",
            false
        ));

        // Analyzer Pre/Post
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(ParameterIDs::ANALYZER_PRE_POST, 1),
            "Analyzer Mode",
            juce::StringArray { "Pre", "Post", "Both" },
            2  // Default: Both
        ));

        // Analyzer On/Off
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::ANALYZER_ON, 1),
            "Analyzer On",
            true
        ));

        // Analyzer Resolution
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(ParameterIDs::ANALYZER_RESOLUTION, 1),
            "Analyzer Resolution",
            ParameterIDs::getAnalyzerResolutionNames(),
            1 // Default: Medium (id 2 -> index 1)
        ));

        // Analyzer Range
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(ParameterIDs::ANALYZER_RANGE, 1),
            "Analyzer Range",
            ParameterIDs::getAnalyzerRangeNames(),
            1 // Default: 90 dB (id 2 -> index 1)
        ));

        // Analyzer Speed
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(ParameterIDs::ANALYZER_SPEED, 1),
            "Analyzer Speed",
            ParameterIDs::getAnalyzerSpeedNames(),
            2 // Default: Medium (id 3 -> index 2)
        ));

        // Analyzer Tilt Enable
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::ANALYZER_TILT_ENABLED, 1),
            "Analyzer Tilt Enabled",
            true
        ));

        // Analyzer Tilt Amount
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::ANALYZER_TILT, 1),
            "Analyzer Tilt",
            juce::NormalisableRange<float>(ParameterIDs::MIN_ANALYZER_TILT, ParameterIDs::MAX_ANALYZER_TILT, 0.5f),
            ParameterIDs::DEFAULT_ANALYZER_TILT
        ));

        // Analyzer Freeze
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::ANALYZER_FREEZE, 1),
            "Analyzer Freeze",
            false
        ));

        // Analyzer Peak Labels
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::ANALYZER_SHOW_PEAKS, 1),
            "Analyzer Show Peaks",
            true
        ));

        //==========================================================================
        // Wet/Dry Mix (Parallel Processing)
        //==========================================================================
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::WET_DRY_MIX, 1),
            "Wet/Dry Mix",
            juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
            100.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(static_cast<int>(value)) + "%";
                })
        ));

        //==========================================================================
        // Oversampling Factor
        //==========================================================================
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(ParameterIDs::OVERSAMPLING_FACTOR, 1),
            "Oversampling",
            juce::StringArray { "Off", "2x", "4x" },
            0  // Default: Off
        ));

        //==========================================================================
        // Delta Mode (nur EQ-Änderung hören)
        //==========================================================================
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::DELTA_MODE, 1),
            "Delta Mode",
            false
        ));

        //==========================================================================
        // Resonance Suppressor (Soothe-Style)
        //==========================================================================
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::SUPPRESSOR_ENABLED, 1),
            "Resonance Suppressor",
            false
        ));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::SUPPRESSOR_DEPTH, 1),
            "Suppressor Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.5f,
            juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(static_cast<int>(value * 100)) + "%";
                })
        ));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::SUPPRESSOR_SPEED, 1),
            "Suppressor Speed",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.5f,
            juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(static_cast<int>(value * 100)) + "%";
                })
        ));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::SUPPRESSOR_SELECTIVITY, 1),
            "Suppressor Selectivity",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.5f,
            juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(static_cast<int>(value * 100)) + "%";
                })
        ));

        //==========================================================================
        // Smart EQ Analyzer Parameter
        //==========================================================================
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::SMART_MODE_ENABLED, 1),
            "Smart Mode",
            false
        ));

        //==========================================================================
        // LiveSmartEQ-Parameter
        //==========================================================================
        
        // LiveSmartEQ Aktiviert
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_ENABLED, 1),
            "Live Smart EQ",
            false
        ));

        // LiveSmartEQ Tiefe (0-100%)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_DEPTH, 1),
            "Live EQ Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.5f,
            juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(static_cast<int>(value * 100)) + "%";
                })
        ));

        // LiveSmartEQ Attack (1-100ms)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_ATTACK, 1),
            "Live EQ Attack",
            juce::NormalisableRange<float>(1.0f, 100.0f, 1.0f, 0.5f),  // Skew for faster access to low values
            20.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("ms")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(static_cast<int>(value)) + " ms";
                })
        ));

        // LiveSmartEQ Release (50-1000ms)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_RELEASE, 1),
            "Live EQ Release",
            juce::NormalisableRange<float>(50.0f, 1000.0f, 1.0f, 0.5f),
            200.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("ms")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(static_cast<int>(value)) + " ms";
                })
        ));

        // LiveSmartEQ Threshold (0.3-6 dB)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_THRESHOLD, 1),
            "Live EQ Threshold",
            juce::NormalisableRange<float>(0.3f, 6.0f, 0.1f),
            1.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(value, 1) + " dB";
                })
        ));

        // LiveSmartEQ Modus
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_MODE, 1),
            "Live EQ Mode",
            ParameterIDs::getLiveSmartEQModeNames(),
            1  // Default: Normal
        ));

        // LiveSmartEQ Max Reduction (-3 to -24 dB)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_MAX_REDUCTION, 1),
            "Live EQ Max Reduction",
            juce::NormalisableRange<float>(-24.0f, -3.0f, 0.5f),
            -12.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction([](float value, int) {
                    return juce::String(value, 1) + " dB";
                })
        ));

        // LiveSmartEQ Transient Protection
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_TRANSIENT_PROTECT, 1),
            "Live EQ Transient Protect",
            true
        ));

        // LiveSmartEQ Mid/Side Mode (0=Stereo, 1=Mid, 2=Side, 3=Auto M/S)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_MS_MODE, 1),
            "Live EQ M/S Mode",
            ParameterIDs::getLiveSmartEQMSModeNames(),
            0  // Default: Stereo
        ));

        // LiveSmartEQ Profil (0=Default, dann alphabetisch Profile)
        {
            juce::StringArray profileNames;
            profileNames.add("Default");
            profileNames.add("Vocals Lead");
            profileNames.add("Vocals Backing");
            profileNames.add("Kick");
            profileNames.add("Snare");
            profileNames.add("Hi-Hat/Cymbals");
            profileNames.add("Bass Electric");
            profileNames.add("Bass Synth");
            profileNames.add("Piano");
            profileNames.add("Synth Pad");
            profileNames.add("Guitar Acoustic");
            profileNames.add("Guitar Electric");
            profileNames.add("Mix Bus");
            profileNames.add("Master");
            profileNames.add("Dialogue");
            
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(ParameterIDs::LIVE_SMART_EQ_PROFILE, 1),
                "Live EQ Profile",
                profileNames,
                0  // Default
            ));
        }

        return { params.begin(), params.end() };
    }
}
