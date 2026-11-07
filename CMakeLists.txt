cmake_minimum_required(VERSION 3.22)

project(ChenevieresComp VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

# JUCE : copie locale si JUCE_DIR est donné, sinon téléchargement
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

option(CHNV_AAX "Build the AAX plugin (Pro Tools)" ON)
if(CHNV_AAX AND (APPLE OR WIN32))
    list(APPEND PLUGIN_FORMATS AAX)
endif()

option(CHNV_STANDALONE "Build the standalone app too" OFF)
if(CHNV_STANDALONE)
    list(APPEND PLUGIN_FORMATS Standalone)
endif()

juce_add_plugin(ChenevieresComp
    COMPANY_NAME "Studio des Chenevieres"
    BUNDLE_ID "fr.chenevieres.comp"
    PLUGIN_MANUFACTURER_CODE Chnv
    PLUGIN_CODE Chcp
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Chenevieres Comp"
    DESCRIPTION "Compresseur multi-modeles"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(ChenevieresComp)

file(GLOB PANEL_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/panels/*)
file(GLOB THUMB_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/thumbs/*)

juce_add_binary_data(ChenevieresCompData SOURCES
    Resources/wordmark_comp.png
    Resources/logo_beagle.png
    Resources/fonts/RobotoCondensed-500.ttf
    Resources/fonts/RobotoCondensed-600.ttf
    Resources/fonts/RobotoCondensed-700.ttf
    Resources/fonts/Inter-Medium.otf
    Resources/fonts/Inter-SemiBold.otf
    ${PANEL_FILES}
    ${THUMB_FILES})

target_sources(ChenevieresComp PRIVATE
    Source/Interpret.cpp
    Source/CompEngine.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PanelView.cpp
    Source/TopBar.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ChenevieresComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ChenevieresComp
    PRIVATE
        ChenevieresCompData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(NOT MSVC)
    target_compile_options(ChenevieresComp PRIVATE -Wno-float-equal)
endif()

# Tests du moteur (sans JUCE) : ctest
enable_testing()
add_executable(EngineTest tests/EngineTest.cpp Source/CompEngine.cpp Source/Interpret.cpp)
add_test(NAME EngineTest COMMAND EngineTest)

# Rendu hors ligne pour les mesures (tools/mesure)
add_executable(CompRender tests/Render.cpp Source/CompEngine.cpp Source/Interpret.cpp)

# Capture de l'interface (docs/apercu.png) : -DCHNV_TESTS=ON
option(CHNV_TESTS "Build the snapshot tool" OFF)
if(CHNV_TESTS)
    add_executable(CompSnapshot tests/Snapshot.cpp)
    target_link_libraries(CompSnapshot PRIVATE ChenevieresComp)
    target_include_directories(CompSnapshot PRIVATE $<TARGET_PROPERTY:ChenevieresComp,INCLUDE_DIRECTORIES>)
    target_compile_definitions(CompSnapshot PRIVATE $<TARGET_PROPERTY:ChenevieresComp,COMPILE_DEFINITIONS>)
endif()
