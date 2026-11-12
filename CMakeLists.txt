cmake_minimum_required(VERSION 3.22)
project(FlameVault VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4          # any JUCE 8.x release works; WebBrowserComponent native integration needs JUCE 8
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# The dashboard UI (HTML + the two real image assets) is embedded in the plugin binary.
juce_add_binary_data(FlameVaultUI
  HEADER_NAME UIData.h
  NAMESPACE UIData
  SOURCES
    UI/index.html
    UI/Assets/02_main_audio_studio_banner_reference.png
    UI/Assets/06_sidebar_flame_wave_decoration_reference.png)
set_target_properties(FlameVaultUI PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

juce_add_plugin(FlameVault
  COMPANY_NAME "FlameVault"
  PRODUCT_NAME "FlameVault"
  PLUGIN_MANUFACTURER_CODE Flvt
  PLUGIN_CODE Fvlt
  FORMATS VST3 Standalone
  IS_SYNTH TRUE                 # loads as a generator in FL Studio so it can output preview audio
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  NEEDS_WEBVIEW2 TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FlameVault PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/SampleLibrary.cpp
  Source/FilenameParser.cpp)

target_compile_definitions(FlameVault PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
  JUCE_USE_CURL=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FlameVault
  PRIVATE
    FlameVaultUI
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
