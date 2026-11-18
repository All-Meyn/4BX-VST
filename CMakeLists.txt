cmake_minimum_required(VERSION 3.22)

project(TriExpand VERSION 0.1.0 LANGUAGES C CXX)

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

juce_add_plugin(TriExpand
    PRODUCT_NAME                "Tri-Expand"
    COMPANY_NAME                "All-Meyn"
    BUNDLE_ID                   "com.allmeyn.triexpand"
    PLUGIN_MANUFACTURER_CODE    Almy
    PLUGIN_CODE                 Trx3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    FORMATS                     VST3 Standalone)

target_sources(TriExpand
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/ThreeBandExpander.cpp
        Source/UI/PanelWidgets.cpp
        Source/UI/Panels.cpp)

target_compile_definitions(TriExpand
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TriExpand
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP tests (no host needed): cmake --build build --target TriExpandTests && ./build/TriExpandTests_artefacts/<config>/TriExpandTests
option(TRIEXPAND_BUILD_TESTS "Build the offline DSP test program" ON)
if(TRIEXPAND_BUILD_TESTS)
    juce_add_console_app(TriExpandTests PRODUCT_NAME "TriExpandTests")
    target_sources(TriExpandTests PRIVATE tests/DspTests.cpp Source/DSP/ThreeBandExpander.cpp)
    target_include_directories(TriExpandTests PRIVATE Source)
    target_compile_definitions(TriExpandTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TriExpandTests
        PRIVATE juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
