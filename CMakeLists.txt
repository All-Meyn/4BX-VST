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

# Optional legacy VST2 build. Steinberg's VST2 SDK is not public and is not
# included here; you need your own copy (and the right to use it). Point this at
# the folder that contains pluginterfaces/vst2.x/aeffect.h, either the
# "vstsdk2.4" folder or the "VST2_SDK" folder from an older VST3 SDK zip, e.g.
#   cmake -S . -B build -DFOURBX_VST2_SDK_PATH=C:/SDKs/vstsdk2.4
# Leave it empty (the default) to build VST3 + Standalone only.
set(FOURBX_VST2_SDK_PATH "" CACHE PATH "Folder containing pluginterfaces/vst2.x/aeffect.h; enables the VST2 target")

set(FOURBX_FORMATS VST3 Standalone)
if(FOURBX_VST2_SDK_PATH)
    set(_fourbx_vst2_root "${FOURBX_VST2_SDK_PATH}")
    if(NOT EXISTS "${_fourbx_vst2_root}/pluginterfaces/vst2.x/aeffect.h"
       AND EXISTS "${_fourbx_vst2_root}/VST2_SDK/pluginterfaces/vst2.x/aeffect.h")
        set(_fourbx_vst2_root "${_fourbx_vst2_root}/VST2_SDK")
    endif()

    if(NOT EXISTS "${_fourbx_vst2_root}/pluginterfaces/vst2.x/aeffect.h"
       OR NOT EXISTS "${_fourbx_vst2_root}/pluginterfaces/vst2.x/aeffectx.h")
        message(FATAL_ERROR "FOURBX_VST2_SDK_PATH='${FOURBX_VST2_SDK_PATH}' does not contain "
                            "pluginterfaces/vst2.x/aeffect.h and aeffectx.h (also checked VST2_SDK/).")
    endif()

    juce_set_vst2_sdk_path("${_fourbx_vst2_root}")
    list(APPEND FOURBX_FORMATS VST)
    message(STATUS "4BX: VST2 target enabled (SDK: ${_fourbx_vst2_root})")
endif()

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
    FORMATS                     ${FOURBX_FORMATS})

target_sources(FourBX
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/ThreeBandExpander.cpp
        Source/UI/PanelWidgets.cpp
        Source/UI/Panels.cpp)

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
