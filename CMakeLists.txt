cmake_minimum_required(VERSION 3.22)

project(Gamaka VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GAMAKA_BUILD_PLUGIN "Build the VST3 plugin" ON)
option(GAMAKA_BUILD_TESTS "Build the DSP core tests" ON)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (otherwise it is downloaded)")

#==============================================================================
# DSP core: plain C++, no JUCE dependency.
add_library(gamaka_core STATIC
    Source/dsp/Raga.cpp
    Source/dsp/Voices.cpp
    Source/dsp/Synth.cpp
    Source/dsp/ExternalDriver.cpp
    Source/dsp/Sketch.cpp)
target_include_directories(gamaka_core PUBLIC Source/dsp)
set_target_properties(gamaka_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(GAMAKA_BUILD_TESTS)
    enable_testing()
    add_executable(gamaka_tests tests/core_tests.cpp)
    target_link_libraries(gamaka_tests PRIVATE gamaka_core)
    add_test(NAME gamaka_tests COMMAND gamaka_tests)
endif()

#==============================================================================
if(GAMAKA_BUILD_PLUGIN)
    if(JUCE_DIR)
        add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.15
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    juce_add_plugin(Gamaka
        COMPANY_NAME "Gamaka"
        PRODUCT_NAME "Gamaka"
        PLUGIN_MANUFACTURER_CODE Gmka
        PLUGIN_CODE Gmk1
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Synth
        FORMATS VST3)

    target_sources(Gamaka PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/PitchDisplay.cpp
        Source/ui/GamakaLookAndFeel.cpp
        Source/ui/SketchEditor.cpp)

    target_compile_definitions(Gamaka PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(Gamaka
        PRIVATE
            gamaka_core
            juce::juce_audio_utils
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
