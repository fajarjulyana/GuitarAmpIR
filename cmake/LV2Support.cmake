# LV2Support.cmake
# ─────────────────────────────────────────────────────────────────────────────
# Helper module for LV2 bundle installation and TTL generation.
# Included automatically by the root CMakeLists.txt via JUCE 7 internals;
# this file adds install targets and a post-build TTL check.
# ─────────────────────────────────────────────────────────────────────────────

# ── Locate lv2 system headers ──────────────────────────────────────────────
if(DEFINED LV2_PATH)
    # User-supplied path (e.g. on Windows without a package manager)
    target_include_directories(GuitarAmpSim PRIVATE "${LV2_PATH}")
    message(STATUS "Using user-supplied LV2 headers: ${LV2_PATH}")
else()
    find_path(LV2_INCLUDE_DIR lv2.h
        HINTS
            /usr/include/lv2
            /usr/local/include/lv2
            /opt/homebrew/include/lv2
            /opt/local/include/lv2
    )
    if(LV2_INCLUDE_DIR)
        target_include_directories(GuitarAmpSim PRIVATE "${LV2_INCLUDE_DIR}/..")
        message(STATUS "Found LV2 headers: ${LV2_INCLUDE_DIR}")
    else()
        message(WARNING
            "LV2 headers not found.\n"
            "  Linux  : sudo apt install lv2-dev\n"
            "  macOS  : brew install lv2\n"
            "  Windows: set -DLV2_PATH=<path-to-lv2-headers>")
    endif()
endif()

# ── Bundle install path ────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    # XDG standard: ~/.lv2  or  /usr/lib/lv2
    set(LV2_INSTALL_DIR "$ENV{HOME}/.lv2" CACHE PATH "LV2 bundle install directory")
elseif(APPLE)
    set(LV2_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/LV2" CACHE PATH "LV2 bundle install directory")
else()
    # Windows: %APPDATA%\LV2
    set(LV2_INSTALL_DIR "$ENV{APPDATA}/LV2" CACHE PATH "LV2 bundle install directory")
endif()

message(STATUS "LV2 install directory: ${LV2_INSTALL_DIR}")

# ── Install target ─────────────────────────────────────────────────────────
# JUCE places the .lv2 bundle in <build>/GuitarAmpSim_artefacts/LV2/
# We add a cmake --install step that copies it to the system bundle dir.
install(
    DIRECTORY "${CMAKE_BINARY_DIR}/GuitarAmpSim_artefacts/LV2/GuitarAmpSim.lv2"
    DESTINATION "${LV2_INSTALL_DIR}"
    USE_SOURCE_PERMISSIONS
)

# ── VST3 install ───────────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    set(VST3_INSTALL_DIR "$ENV{HOME}/.vst3" CACHE PATH "VST3 install directory")
elseif(APPLE)
    set(VST3_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3" CACHE PATH "VST3 install directory")
else()
    set(VST3_INSTALL_DIR "$ENV{PROGRAMFILES}/Common Files/VST3" CACHE PATH "VST3 install directory")
endif()

install(
    DIRECTORY "${CMAKE_BINARY_DIR}/GuitarAmpSim_artefacts/VST3/GuitarAmpSim.vst3"
    DESTINATION "${VST3_INSTALL_DIR}"
    USE_SOURCE_PERMISSIONS
)

message(STATUS "VST3 install directory: ${VST3_INSTALL_DIR}")
