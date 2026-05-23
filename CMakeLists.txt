cmake_minimum_required(VERSION 3.22)

project(Aura VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LTO standardmäßig AN für Release-Builds
# Für Dev-Builds: cmake -B build [...] -DAURA_ENABLE_LTO=OFF
option(AURA_ENABLE_LTO "Enable Link-Time Optimization (slow linking, ~5% faster runtime)" ON)
option(AURA_ENABLE_BENCHMARKS "Build Aura benchmark executables" ON)

# ===== Build-Optimierungen =====
if(MSVC)
    # Parallele Kompilierung (nutzt alle CPU-Kerne)
    add_compile_options(/MP)
    
    # Schnelleres Preprocessing
    add_compile_options(/Zc:preprocessor /Zc:inline)
endif()

# Unity Build: Nicht kompatibel mit JUCE-Modulen (interne Include-Guards)
# PCH + /MP liefern bereits den Hauptgeschwindigkeitsgewinn
set(CMAKE_UNITY_BUILD OFF)

# JUCE hinzufügen
add_subdirectory(JUCE)

# CLAP-Format Support (clap-juce-extensions)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/clap-juce-extensions/CMakeLists.txt")
    add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)
    set(CLAP_SUPPORT ON)
    message(STATUS "CLAP support enabled")
else()
    set(CLAP_SUPPORT OFF)
    message(STATUS "CLAP support disabled (clap-juce-extensions not found)")
endif()

# Plugin-Quellen definieren
set(PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    
    # DSP
    Source/DSP/ABComparison.h
    Source/DSP/AdvancedFilterDesigns.h
    Source/DSP/AutoGainCompensation.h
    Source/DSP/BiquadFilter.cpp
    Source/DSP/BiquadFilter.h
    Source/DSP/CharacterProcessor.h
    Source/DSP/DynamicResonanceSuppressor.h
    Source/DSP/EQBand.cpp
    Source/DSP/EQBand.h
    Source/DSP/EQProcessor.cpp
    Source/DSP/EQProcessor.h
    Source/DSP/FFTAnalyzer.cpp
    Source/DSP/FFTAnalyzer.h
    Source/DSP/HighQualityOversampler.h
    Source/DSP/InstrumentProfiles.h
    Source/DSP/LinearPhaseEQ.h
    Source/DSP/LiveSmartEQ.h
    Source/DSP/MixedPhaseEQ.h
    Source/DSP/PsychoAcousticModel.h
    Source/DSP/SVFFilter.h
    Source/DSP/ReferenceAudioPlayer.h
    Source/DSP/SmartAnalyzer.cpp
    Source/DSP/SmartAnalyzer.h
    Source/DSP/SmartEQRecommendation.h
    Source/DSP/SpectralAnalysis.h
    Source/DSP/SpectralMatcher.h
    
    # v2.0: Neue DSP Module
    Source/DSP/AIExplanationEngine.h
    Source/DSP/CrossChannelManager.h
    Source/DSP/CustomProfileManager.h
    Source/DSP/GenreMorphSlider.h
    Source/DSP/LUFSAutoGain.h
    Source/DSP/MultiReferenceManager.h
    Source/DSP/ParallelProcessor.h
    Source/DSP/SidechainDynamicEQ.h
    Source/DSP/SpectralDynamicsProcessor.h
    Source/DSP/TemporalPatternDetector.h
    Source/DSP/TransientDetector.h
    Source/DSP/WebSearchEngine.h
    
    # GUI
    Source/GUI/AudioSourceSelector.h
    Source/GUI/BandControls.cpp
    Source/GUI/BandControls.h
    Source/GUI/BandPopup.cpp
    Source/GUI/BandPopup.h
    Source/GUI/CustomLookAndFeel.cpp
    Source/GUI/CustomLookAndFeel.h
    Source/GUI/EQCurveComponent.cpp
    Source/GUI/EQCurveComponent.h
    Source/GUI/KeyboardShortcutManager.h
    Source/GUI/LevelMeter.cpp
    Source/GUI/LevelMeter.h
    Source/GUI/LiveSmartEQPanel.h
    Source/GUI/PianoRollOverlay.h
    Source/GUI/PresetComponent.h
    Source/GUI/ReferenceTrackPanel.h
    Source/GUI/SmartHighlightOverlay.h
    Source/GUI/SmartRecommendationPanel.h
    Source/GUI/SpectrumAnalyzer.cpp
    Source/GUI/SpectrumAnalyzer.h
    Source/GUI/SpectrumGrabTool.cpp
    Source/GUI/SpectrumGrabTool.h
    Source/GUI/ThemeManager.h
    Source/GUI/ThemeSelector.h
    Source/GUI/AuraSettingsPanel.h
    Source/GUI/UpdateNotification.h
    
    # v2.0: Neue GUI Module
    Source/GUI/CrossChannelPanel.h
    Source/GUI/CustomProfilePanel.h
    Source/GUI/EQSketchTool.h
    Source/GUI/GenreMorphWidget.h
    Source/GUI/MultiReferencePanel.h
    Source/GUI/SpectralDynamicsPanel.h
    Source/GUI/TutorialOverlay.h
    Source/GUI/WebSearchPanel.h
    
    # Parameters
    Source/Parameters/ParameterIDs.h
    Source/Parameters/ParameterLayout.cpp
    Source/Parameters/ParameterLayout.h
    
    # Presets
    Source/Presets/PresetManager.h
    
    # Licensing
    Source/Licensing/LicenseManager.cpp
    Source/Licensing/LicenseManager.h
    Source/Licensing/LicenseDialog.h
    Source/Licensing/OnlineLicenseValidator.h
    
    # Utils
    Source/Utils/UpdateChecker.h
    Source/Utils/VersionInfo.h
    Source/Utils/VirtualAudioDeviceDetector.h
    Source/Utils/WASAPILoopbackCapture.h
    
    # Folgende Dateien wurden entfernt (v2.5):
    #   Source/DSP/AdvancedProcessing.h/.cpp  (veraltet, durch MidSideProcessor ersetzt)
    #   Source/GUI/LicenseDialog.cpp          (veraltet, durch Licensing/LicenseDialog.h ersetzt)
)

# JUCE Plugin konfigurieren
juce_add_plugin(Aura
    COMPANY_NAME "Unproved Audio"
    BUNDLE_ID "com.unproved-audio.aura"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Unpr
    PLUGIN_CODE Aura
    FORMATS Standalone VST3
    PRODUCT_NAME "Aura"
    VST3_AUTO_MANIFEST FALSE
)

target_sources(Aura PRIVATE ${PLUGIN_SOURCES})

# Precompiled Header: Standard-Library und System-Header cachen (nur C++)
# (JuceHeader.h kann nicht als PCH genutzt werden — JUCE-Module haben interne Include-Guards)
target_precompile_headers(Aura PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Source/pch.h>")

target_compile_definitions(Aura
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Aura
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        # LTO (Link-Time Optimization) standardmäßig aktiviert für Release-Builds:
        #   cmake -B build -G "Visual Studio 18 2026" -A x64
        $<$<BOOL:${AURA_ENABLE_LTO}>:juce::juce_recommended_lto_flags>
        juce::juce_recommended_warning_flags
)

target_include_directories(Aura PRIVATE Source)

# JuceHeader.h generieren
juce_generate_juce_header(Aura)

# CLAP-Target erstellen (wenn verfügbar)
if(CLAP_SUPPORT)
    clap_juce_extensions_plugin(
        TARGET Aura
        CLAP_ID "com.unproved-audio.aura"
        CLAP_FEATURES "audio-effect" "equalizer" "analyzer"
    )
endif()

# Mikro-Benchmarks für reproduzierbare Performance-Messungen
if(AURA_ENABLE_BENCHMARKS)
    juce_add_console_app(AuraBenchmarks
        PRODUCT_NAME "AuraBenchmarks"
    )

    target_sources(AuraBenchmarks PRIVATE
        tools/benchmarks/EQCurveBenchmark.cpp
        Source/DSP/BiquadFilter.cpp
        Source/DSP/EQBand.cpp
        Source/DSP/EQProcessor.cpp
        Source/DSP/FFTAnalyzer.cpp
        Source/DSP/SmartAnalyzer.cpp
    )

    target_compile_definitions(AuraBenchmarks
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    target_link_libraries(AuraBenchmarks
        PRIVATE
            juce::juce_audio_basics
            juce::juce_core
            juce::juce_dsp
            juce::juce_graphics
        PUBLIC
            juce::juce_recommended_config_flags
            $<$<BOOL:${AURA_ENABLE_LTO}>:juce::juce_recommended_lto_flags>
            juce::juce_recommended_warning_flags
    )

    target_include_directories(AuraBenchmarks PRIVATE Source)

    juce_generate_juce_header(AuraBenchmarks)
endif()

# ===== Unit Tests (JUCE built-in test framework) =====
option(AURA_ENABLE_TESTS "Build Aura unit tests" ON)

if(AURA_ENABLE_TESTS)
    juce_add_console_app(AuraTests
        PRODUCT_NAME "AuraTests"
    )

    target_sources(AuraTests PRIVATE
        tests/AuraTests.cpp
        Source/DSP/BiquadFilter.cpp
        Source/DSP/EQBand.cpp
        Source/DSP/EQProcessor.cpp
        Source/DSP/FFTAnalyzer.cpp
        Source/DSP/SmartAnalyzer.cpp
        Source/DSP/WebSearchEngine.h
    )

    target_compile_definitions(AuraTests
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODULE_AVAILABLE_juce_audio_basics=1
            JUCE_MODULE_AVAILABLE_juce_audio_utils=1
            JUCE_MODULE_AVAILABLE_juce_dsp=1
    )

    target_link_libraries(AuraTests
        PRIVATE
            juce::juce_audio_basics
            juce::juce_audio_utils
            juce::juce_core
            juce::juce_dsp
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            $<$<BOOL:${AURA_ENABLE_LTO}>:juce::juce_recommended_lto_flags>
            juce::juce_recommended_warning_flags
    )

    target_include_directories(AuraTests PRIVATE Source)

    juce_generate_juce_header(AuraTests)

    # Add CTest support
    enable_testing()
    add_test(NAME AuraTests COMMAND AuraTests)
endif()

