cmake_minimum_required(VERSION 3.22)

project(FourBX VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FourBX
    PRODUCT_NAME                "4BX"
    COMPANY_NAME                "All-Meyn"
    BUNDLE_ID                   "com.allmeyn.fourbx"
    PLUGIN_MANUFACTURER_CODE    Almy
    PLUGIN_CODE                 Fbx4
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    FORMATS                     VST3 Standalone)

target_sources(FourBX
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/ThreeBandExpander.cpp
        Source/UI/PanelWidgets.cpp
        Source/UI/Panels.cpp
        Source/Diagnostics/WindowDiagnostics.cpp)

target_compile_definitions(FourBX
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FourBX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP tests (no host needed): cmake --build build --target FourBXTests && ./build/FourBXTests_artefacts/<config>/FourBXTests
option(FOURBX_BUILD_TESTS "Build the offline DSP test program" ON)
if(FOURBX_BUILD_TESTS)
    juce_add_console_app(FourBXTests PRODUCT_NAME "FourBXTests")
    target_sources(FourBXTests PRIVATE tests/DspTests.cpp Source/DSP/ThreeBandExpander.cpp)
    target_include_directories(FourBXTests PRIVATE Source)
    target_compile_definitions(FourBXTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FourBXTests
        PRIVATE juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
