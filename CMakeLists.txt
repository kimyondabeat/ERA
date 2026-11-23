cmake_minimum_required(VERSION 3.22)
project(ERA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically, unless you point FETCHCONTENT_SOURCE_DIR_JUCE at a local copy.
# Pro Tools (AAX): cmake -DERA_AAX=ON uses JUCE 8, which ships the AAX SDK.
option(ERA_AAX "Build the AAX plugin for Pro Tools (needs JUCE 8)" OFF)
if(ERA_AAX)
    set(ERA_JUCE_TAG "8.0.17-security" CACHE STRING "JUCE version")
else()
    set(ERA_JUCE_TAG "7.0.12" CACHE STRING "JUCE version")
endif()
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${ERA_JUCE_TAG}
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Windows/Linux: VST3 + app.  Mac: AU (Logic, GarageBand...) + VST3 + app, Intel and Apple Silicon.
set(ERA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ERA_FORMATS AU)
endif()
if(ERA_AAX)
    set(ERA_FORMATS AAX)
endif()

juce_add_plugin(ERA
    COMPANY_NAME                "kimyondabeat"
    PRODUCT_NAME                "ERA"
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    PLUGIN_MANUFACTURER_CODE    Kimy
    PLUGIN_CODE                 Erar
    FORMATS                     ${ERA_FORMATS}
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    AAX_CATEGORY                AAX_ePlugInCategory_SWGenerators
    BUNDLE_ID                   com.kimyondabeat.era
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Drum
    COPY_PLUGIN_AFTER_BUILD     FALSE)

# The drum kit: every file in Kit/ is embedded in the plugin.
# Names must look like  <category>__<NN>__<name>.flac  (category = kick snare rim percs vox fx clap tom).
file(GLOB ERA_KIT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Kit/*.flac" "${CMAKE_CURRENT_SOURCE_DIR}/Kit/*.wav")
juce_add_binary_data(EraData
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES     ${ERA_KIT_FILES}
                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/SpaceGrotesk-Bold.ttf"
                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/SpaceGrotesk-Medium.ttf"
                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/JetBrainsMono-Bold.ttf")

target_sources(ERA PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/HomePage.cpp
    Source/MixFxPages.cpp
    Source/Library.cpp
    Source/SampleEdit.cpp
    Source/Fx.cpp)

target_compile_definitions(ERA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_FLAC=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ERA
    PRIVATE
        EraData
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Offline tests (Linux/Mac dev only):  cmake -DERA_TESTS=ON ... && ./build/EraTest_artefacts/Release/EraTest
option(ERA_TESTS "Build the offline test program" OFF)
if(ERA_TESTS)
    juce_add_console_app(EraTest PRODUCT_NAME "EraTest")
    target_sources(EraTest PRIVATE tests/EraTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Source/HomePage.cpp Source/MixFxPages.cpp Source/Library.cpp Source/SampleEdit.cpp Source/Fx.cpp)
    target_compile_definitions(EraTest PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_FLAC=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(EraTest PRIVATE EraData juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
