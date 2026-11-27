#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The plugin's dependencies, at pinned versions, into plugin/ext/ (not committed): JUCE 8 (AGPLv3), the VST3 SDK's
# validator (its own licence: GPLv3 or Steinberg's; the plugin itself uses the VST3 SDK JUCE bundles), clap-juce-extensions.
set -e
cd "$(dirname "$0")"
mkdir -p ext
JUCE_TAG=8.0.15
VST3_TAG=v3.8.0_build_66
CLAP_TAG=0.26.0
[ -d ext/JUCE ] || git clone -q --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git ext/JUCE
if [ "${WITH_VST3SDK:-1}" = 1 ] && [ ! -d ext/vst3sdk ]; then
  git clone -q --depth 1 --branch "$VST3_TAG" https://github.com/steinbergmedia/vst3sdk.git ext/vst3sdk
  (cd ext/vst3sdk && git submodule update -q --init --depth 1 base pluginterfaces public.sdk cmake)
fi
if [ "${WITH_CLAP:-1}" = 1 ] && [ ! -d ext/clap-juce-extensions ]; then
  git clone -q --depth 1 --branch "$CLAP_TAG" https://github.com/free-audio/clap-juce-extensions.git ext/clap-juce-extensions
  (cd ext/clap-juce-extensions && git submodule update -q --init --recursive --depth 1)
fi
echo "deps: JUCE $JUCE_TAG, VST3 SDK $VST3_TAG, clap-juce-extensions $CLAP_TAG"
