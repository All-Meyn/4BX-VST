cmake_minimum_required(VERSION 3.22)

project(TriExpand VERSION 1.0.0 LANGUAGES C CXX)

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

# Two plugins share all source code; the band count is set per target with
# EXPANDER_NUM_BANDS (see Source/DSP/ExpanderConstants.h).
#   Tri-Expand    3 bands (LF / MF / HF)
#   Hepta-Expand  7 bands (crossovers 60 / 150 / 400 / 1k / 2.5k / 6k Hz)
# Each has its own plugin code, so hosts treat them as separate plugins.
function(add_expander_plugin target product_name bundle_id plugin_code num_bands)
    juce_add_plugin(${target}
        PRODUCT_NAME                "${product_name}"
        COMPANY_NAME                "All-Meyn"
        BUNDLE_ID                   "${bundle_id}"
        PLUGIN_MANUFACTURER_CODE    Almy
        PLUGIN_CODE                 ${plugin_code}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        FORMATS                     VST3 Standalone)

    target_sources(${target}
        PRIVATE
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/DSP/MultiBandExpander.cpp
            Source/UI/PanelWidgets.cpp
            Source/UI/Panels.cpp)

    target_compile_definitions(${target}
        PUBLIC
            EXPANDER_NUM_BANDS=${num_bands}
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

add_expander_plugin(TriExpand   "Tri-Expand"   "com.allmeyn.triexpand"   Trx3 3)
add_expander_plugin(HeptaExpand "Hepta-Expand" "com.allmeyn.heptaexpand" Hpx7 7)

# Offline DSP tests (no host needed), one program per band count:
#   cmake --build build --target TriExpandTests HeptaExpandTests
#   ./build/TriExpandTests_artefacts/<config>/TriExpandTests
#   ./build/HeptaExpandTests_artefacts/<config>/HeptaExpandTests
option(TRIEXPAND_BUILD_TESTS "Build the offline DSP test programs" ON)
if(TRIEXPAND_BUILD_TESTS)
    foreach(cfg "TriExpandTests;3" "HeptaExpandTests;7")
        list(GET cfg 0 test_target)
        list(GET cfg 1 num_bands)
        juce_add_console_app(${test_target} PRODUCT_NAME "${test_target}")
        target_sources(${test_target} PRIVATE tests/DspTests.cpp Source/DSP/MultiBandExpander.cpp)
        target_include_directories(${test_target} PRIVATE Source)
        target_compile_definitions(${test_target} PRIVATE EXPANDER_NUM_BANDS=${num_bands} JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        target_link_libraries(${test_target}
            PRIVATE juce::juce_dsp
            PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endforeach()
endif()
