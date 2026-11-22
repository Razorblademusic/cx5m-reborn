cmake_minimum_required(VERSION 3.22)
project(CX5MReborn VERSION 0.6.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
if(DEFINED JUCE_DIR)
  add_subdirectory("${JUCE_DIR}" JUCE)
else()
  find_package(JUCE CONFIG REQUIRED)
endif()
# GitHub Actions downloads the BSD-3-Clause ymfm source here.
if(NOT DEFINED YMFM_DIR OR NOT EXISTS "${YMFM_DIR}/src/ymfm_opm.cpp")
  message(FATAL_ERROR "Set -DYMFM_DIR to a checkout of https://github.com/aaronsgiles/ymfm")
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
juce_generate_juce_header(CX5MReborn)
# Embed artwork at configure time without invoking juceaide's binarydata command.
set(artwork_directory "${CMAKE_CURRENT_BINARY_DIR}/CX5MArtwork")
file(MAKE_DIRECTORY "${artwork_directory}")
set(artwork_header "#pragma once\nnamespace BinaryData {\n")
foreach(asset IN ITEMS wordmark charcoal)
  set(asset_path "${CMAKE_CURRENT_SOURCE_DIR}/Assets/${asset}.png")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${asset_path}")
  file(READ "${asset_path}" asset_hex HEX)
  # Group sixteen bytes per line to keep generated source lines short.
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," asset_bytes "${asset_hex}")
  string(REPEAT "0x[0-9a-f][0-9a-f]," 16 asset_line_pattern)
  string(REGEX REPLACE "(${asset_line_pattern})" "\\1\n" asset_bytes "${asset_bytes}")
  string(APPEND artwork_header
    "inline constexpr unsigned char ${asset}_png[] = {\n${asset_bytes}\n};\n"
    "inline constexpr int ${asset}_pngSize = static_cast<int>(sizeof(${asset}_png));\n")
endforeach()
string(APPEND artwork_header "}\n")
file(CONFIGURE OUTPUT "${artwork_directory}/BinaryData.h" CONTENT "${artwork_header}" @ONLY)
target_include_directories(CX5MReborn PRIVATE "${artwork_directory}")
target_sources(CX5MReborn PRIVATE
  Source/PluginProcessor.cpp Source/PluginEditor.cpp
  "${YMFM_DIR}/src/ymfm_misc.cpp"
  "${YMFM_DIR}/src/ymfm_opm.cpp")
target_include_directories(CX5MReborn PRIVATE "${YMFM_DIR}/src")
target_compile_definitions(CX5MReborn PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CX5MReborn PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
