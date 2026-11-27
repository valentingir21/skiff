# SPDX-License-Identifier: GPL-3.0-only
# Gir Editor as a plugin (AU, VST3, CLAP): the Gir Editor web build in JUCE's WebBrowserComponent, the FM-1 reached by
# the plugin's own CoreMIDI ports (WKWebView has no Web MIDI). JUCE 8 under the AGPLv3. Built by build-mac.sh.
cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "macOS 12 at least")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "universal")
project(GirEditorPlugin VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)
set(CMAKE_CXX_STANDARD 17)

option(GIR_CLAP "also a CLAP (clap-juce-extensions)" OFF)
option(GIR_TEST_HOST "the test host (tests/host)" ON)
set(GIR_WEB_ZIP "${CMAKE_BINARY_DIR}/web.zip" CACHE FILEPATH "the editor's web build, zipped (build-mac.sh)")
set(GIR_VERSION_LABEL "1.0.0-rc1" CACHE STRING "the version shown")

add_subdirectory(ext/JUCE)

juce_add_plugin(GirEditorPlugin
  PRODUCT_NAME "Gir Editor"
  COMPANY_NAME "Gir"
  BUNDLE_ID "io.github.valentingir21.gir-editor.plugin"
  PLUGIN_MANUFACTURER_CODE Girr
  PLUGIN_CODE GirE
  FORMATS AU VST3
  DESCRIPTION "Gir Editor for the FM-1, in the DAW"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx Tools
  COPY_PLUGIN_AFTER_BUILD FALSE
  HARDENED_RUNTIME_ENABLED FALSE)

juce_add_binary_data(GirWebData SOURCES "${GIR_WEB_ZIP}")
set_target_properties(GirWebData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(GirEditorPlugin PRIVATE
  src/PluginProcessor.cpp src/PluginEditor.cpp src/WebUI.cpp src/MidiLink.cpp src/PortLock.cpp src/WebKit.mm)
target_compile_definitions(GirEditorPlugin PUBLIC
  JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=0
  GIR_VERSION_LABEL="${GIR_VERSION_LABEL}")
target_link_libraries(GirEditorPlugin PRIVATE GirWebData
  juce::juce_audio_utils juce::juce_gui_extra juce::juce_audio_processors
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
target_link_libraries(GirEditorPlugin PRIVATE "-framework WebKit")

if(GIR_CLAP AND EXISTS "${CMAKE_SOURCE_DIR}/ext/clap-juce-extensions/CMakeLists.txt")
  add_subdirectory(ext/clap-juce-extensions EXCLUDE_FROM_ALL)
  clap_juce_extensions_plugin(TARGET GirEditorPlugin
    CLAP_ID "io.github.valentingir21.gir-editor"
    CLAP_FEATURES utility audio-effect)
endif()

if(GIR_TEST_HOST)
  add_subdirectory(tests/host)
endif()
