cmake_minimum_required(VERSION 3.22)

project(ThePsychickMachine VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- JUCE (downloaded automatically on first configure) ---------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

# --- Embedded artwork (skull test-card) -------------------------------------
juce_add_binary_data(PsychickAssets SOURCES Assets/skull.png)
set_target_properties(PsychickAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Plugin ------------------------------------------------------------------
juce_add_plugin(PsychickMachine
    COMPANY_NAME                "Psychick Machine Works"
    PLUGIN_MANUFACTURER_CODE    Psyk
    PLUGIN_CODE                 Pmch
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "THEE PSYCHICK MACHINE"
    VST3_CATEGORIES             Fx Distortion Modulation
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(PsychickMachine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PsychickEngine.h)

target_compile_definitions(PsychickMachine PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PsychickMachine
    PRIVATE
        PsychickAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- DSP unit test (no JUCE needed): ctest ----------------------------------
enable_testing()
add_executable(engine_test Tests/engine_test.cpp)
target_include_directories(engine_test PRIVATE Source)
add_test(NAME engine_test COMMAND engine_test)
