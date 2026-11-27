# SPDX-License-Identifier: GPL-3.0-only
# girhost: the plugin's test host (no DAW): loads the AU or the VST3, opens its window, drives its parameters, and
# offers a virtual FM-1 on a CoreMIDI port of its own ("Gir Test FM-1"): the Gir firmware with its SysEx editor,
# compiled natively from web/gir-editor/emu/felucca_link.c (the editor's virtual FM-1, here not in WebAssembly).
set(GIR_ROOT "${CMAKE_SOURCE_DIR}/..")
file(STRINGS "${GIR_ROOT}/firmware/src/felucca.c" _v REGEX "^#define FELUCCA_VERSION \"")
string(REGEX REPLACE ".*\"([^\"]*)\".*" "\\1" FELUCCA_VERSION "${_v}")

add_library(girlink STATIC "${GIR_ROOT}/web/gir-editor/emu/felucca_link.c")
target_include_directories(girlink PRIVATE "${GIR_ROOT}/build/gen" "${GIR_ROOT}/firmware/src")
# (the firmware's linker sections mean nothing here: "section(x)" becomes "used"; firmware/ is not changed)
target_compile_definitions(girlink PRIVATE "FELUCCA_VERSION=\"${FELUCCA_VERSION}\"")
target_compile_options(girlink PRIVATE "-Dsection(x)=used" -O2 -ffp-contract=off -std=gnu99 -w)

juce_add_gui_app(girhost PRODUCT_NAME "girhost" BUNDLE_ID "io.github.valentingir21.girhost")
target_sources(girhost PRIVATE Main.cpp)
target_compile_definitions(girhost PRIVATE JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=1 JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0 JUCE_APPLICATION_NAME_STRING="girhost" JUCE_APPLICATION_VERSION_STRING="1.0")
target_link_libraries(girhost PRIVATE girlink juce::juce_audio_utils juce::juce_audio_processors
  PUBLIC juce::juce_recommended_config_flags)
