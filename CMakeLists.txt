cmake_minimum_required(VERSION 3.22)
project(GuitarAmpSim VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch JUCE ────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── LV2 SDK (lv2 headers only, no library needed) ────────────────────────────
# The lv2 headers are bundled with JUCE 7+ but you can also point to a system
# install.  Set LV2_PATH if your system headers are elsewhere.
# On Linux: sudo apt install lv2-dev
# On macOS:  brew install lv2
# On Windows: manually set -DLV2_PATH=<path>

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(GuitarAmpSim
    # Metadata ──────────────────────────────────────────────────────────────
    COMPANY_NAME            "QWare, Inc"
    JUCE_GENERATE_JUCEHEADER ON
    PLUGIN_MANUFACTURER_CODE OsSr
    PLUGIN_CODE             GASm
    FORMATS                 VST3 LV2 Standalone
    PRODUCT_NAME            "GuitarAmpIR"
    DESCRIPTION             "GuitarAmpIR with convolution IR loader"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # LV2 specific ───────────────────────────────────────────────────────────
    LV2URI                  "https://github.com/fajarjulyana/GuitarAmpIR"
    LV2_SHARED_LIBRARY_NAME GuitarAmpSim

    # VST3 specific ──────────────────────────────────────────────────────────
    VST3_CATEGORIES         "Fx" "Distortion" "Guitar"
)

# ── Source files ──────────────────────────────────────────────────────────────
target_sources(GuitarAmpSim PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ConvolutionEngine.cpp
    Source/IRLoader.cpp
    Source/AmpProcessor.cpp
)

target_include_directories(GuitarAmpSim PRIVATE Source)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(GuitarAmpSim PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

# ── Link JUCE modules ─────────────────────────────────────────────────────────
target_link_libraries(GuitarAmpSim PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ── Platform-specific flags ───────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    target_link_libraries(GuitarAmpSim PRIVATE pthread dl)
endif()

if(MSVC)
    target_compile_options(GuitarAmpSim PRIVATE /W4 /arch:AVX2)
else()
    target_compile_options(GuitarAmpSim PRIVATE -Wall -Wextra -O3 -march=native)
endif()
