cmake_minimum_required(VERSION 3.22)
project(CX5MReborn VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
# Supply JUCE via -DJUCE_DIR=path/to/JUCE or install JUCE as a CMake package.
if(DEFINED JUCE_DIR)
  add_subdirectory("${JUCE_DIR}" JUCE)
else()
  find_package(JUCE CONFIG REQUIRED)
endif()
juce_add_plugin(CX5MReborn
  COMPANY_NAME "CX5M Reborn Project"
  BUNDLE_ID "org.cx5mreborn.synth"
  PLUGIN_MANUFACTURER_CODE CxRb
  PLUGIN_CODE CxR1
  FORMATS VST3
  PRODUCT_NAME "CX5M Reborn"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(CX5MReborn PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(CX5MReborn PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(CX5MReborn PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
