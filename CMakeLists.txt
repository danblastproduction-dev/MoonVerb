cmake_minimum_required(VERSION 3.22)
project(MoonVerb VERSION 1.0.0)
set(MOONVERB_VERSION "1.0.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(MoonVerb
    COMPANY_NAME "DeadLoop"
    PLUGIN_MANUFACTURER_CODE Moon
    PLUGIN_CODE Mvrb
    FORMATS AU VST3
    PRODUCT_NAME "MoonVerb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    BUNDLE_ID "fr.deadloop.moonverb"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

juce_generate_juce_header(MoonVerb)

target_sources(MoonVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(MoonVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    MOONVERB_VERSION="${MOONVERB_VERSION}"
)

target_link_libraries(MoonVerb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
