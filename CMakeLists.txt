cmake_minimum_required(VERSION 3.22)

project(Aura VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LTO standardmäßig AUS für schnelle Dev-Builds
# Für finale Releases: cmake -B build3 [...] -DAURA_ENABLE_LTO=ON
option(AURA_ENABLE_LTO "Enable Link-Time Optimization (slow linking, ~5% faster runtime)" OFF)

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
    Source/DSP/AdvancedProcessing.cpp
    Source/DSP/AdvancedProcessing.h
    Source/DSP/AutoGainCompensation.h
    Source/DSP/BiquadFilter.cpp
    Source/DSP/BiquadFilter.h
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
    Source/DSP/PsychoAcousticModel.h
    Source/DSP/SVFFilter.h
    Source/DSP/ReferenceAudioPlayer.h
    Source/DSP/SmartAnalyzer.cpp
    Source/DSP/SmartAnalyzer.h
    Source/DSP/SmartEQRecommendation.h
    Source/DSP/SpectralAnalysis.h
    Source/DSP/SpectralMatcher.h
    
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
    Source/GUI/UpdateNotification.h
    
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
)

# JUCE Plugin konfigurieren
juce_add_plugin(Aura
    COMPANY_NAME "Unproved Audio"
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
        # LTO (Link-Time Optimization) nur für finale Releases aktivieren:
        #   cmake -B build3 -G "Visual Studio 18 2026" -A x64 -DAURA_ENABLE_LTO=ON
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
