cmake_minimum_required(VERSION 3.25)
project(OneTwoFiveA_Pitchator VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PITCHATOR_BUILD_VST3 "Compile native Steinberg VST3" ON)
enable_testing()
add_executable(PitchEngineTests tests/PitchEngineTests.cpp)
add_executable(PitchSchedulerTests tests/PitchSchedulerTests.cpp)
add_executable(CurveMailboxTests tests/CurveMailboxTests.cpp)
add_executable(PitchatorUIContractTests tests/UIContractTests.cpp)
add_executable(PitchPointEditTests tests/PitchPointEditTests.cpp)
add_executable(PitchCurveMouseEventsTests tests/CurveMouseEventsTests.cpp src/PitchCurveView.cpp)
add_executable(PitchatorUILayoutTests tests/UILayoutTests.cpp)
add_executable(ScaleNoteNamesTests tests/ScaleNoteNamesTests.cpp)
add_executable(PitchatorDemoTimerTests tests/DemoTimerTests.cpp)
target_compile_definitions(PitchatorUILayoutTests PRIVATE
 PITCHATOR_UIDESC_PATH="${CMAKE_SOURCE_DIR}/src/resource/pitchator.uidesc"
 PITCHATOR_LOGO_SRC_PATH="${CMAKE_SOURCE_DIR}/src/BrandLogoView.cpp"
 PITCHATOR_SRC_PATH="${CMAKE_SOURCE_DIR}/src/PitchatorVST.cpp")
target_compile_definitions(PitchatorUIContractTests PRIVATE
 PITCHATOR_UIDESC_PATH="${CMAKE_SOURCE_DIR}/src/resource/pitchator.uidesc"
 PITCHATOR_SRC_PATH="${CMAKE_SOURCE_DIR}/src/PitchatorVST.cpp"
 PITCHATOR_GRAPH_PATH="${CMAKE_SOURCE_DIR}/src/PitchCurveView.cpp")
add_test(NAME PitchEngineTests COMMAND PitchEngineTests)
add_test(NAME PitchSchedulerTests COMMAND PitchSchedulerTests)
add_test(NAME CurveMailboxTests COMMAND CurveMailboxTests)
add_test(NAME PitchatorUIContractTests COMMAND PitchatorUIContractTests)
add_test(NAME PitchPointEditTests COMMAND PitchPointEditTests)
add_test(NAME PitchCurveMouseEventsTests COMMAND PitchCurveMouseEventsTests)
add_test(NAME PitchatorUILayoutTests COMMAND PitchatorUILayoutTests)
add_test(NAME ScaleNoteNamesTests COMMAND ScaleNoteNamesTests)
add_test(NAME PitchatorDemoTimerTests COMMAND PitchatorDemoTimerTests)
# VST3 processor/controller integration regression reproduces the
# Steinberg Validator's zero-buffer BYPASS persistence sequence.

if(PITCHATOR_BUILD_VST3)
 set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
 set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
 set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
 set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
 set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
 include(FetchContent)
 FetchContent_Declare(vst3sdk
  GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
  GIT_TAG v3.8.1_build_84)
 FetchContent_MakeAvailable(vst3sdk)
 smtg_add_vst3plugin(Pitchator SOURCES_LIST
  src/PitchatorVST.cpp src/PitchatorVST.h
  src/PitchCurveView.cpp src/PitchCurveView.h src/BrandLogoView.cpp src/BrandLogoView.h src/CurveMailbox.h
  src/PitchEngine.h src/PitchScheduler.h src/PitchPointEdit.h)
 # Steinberg SDK 3.8.1 fails its Windows resource-copy command when
 # PACKAGE_NAME includes spaces. Compile using the working SDK folder name,
 # then stage/rename the WHOLE bundle (folder + x64 DLL) for real validation.
 get_target_property(PITCHATOR_PACKAGE_NAME Pitchator SMTG_PLUGIN_PACKAGE_NAME)
 if(NOT PITCHATOR_PACKAGE_NAME STREQUAL "Pitchator.vst3")
  message(FATAL_ERROR "Unexpected internal SDK package: ${PITCHATOR_PACKAGE_NAME}")
 endif()
 target_link_libraries(Pitchator PRIVATE sdk vstgui_support)
 target_link_libraries(PitchCurveMouseEventsTests PRIVATE sdk vstgui_support)
 smtg_target_add_plugin_resources(Pitchator
  RESOURCES src/resource/pitchator.uidesc
            src/resource/125A_Logo_Master_FINAL.svg)
 target_compile_features(Pitchator PRIVATE cxx_std_17)
 target_compile_definitions(Pitchator PRIVATE PITCHATOR_DEMO_BUILD=1)
 if(WIN32)
  target_sources(Pitchator PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp)
 endif()
 smtg_target_configure_version_file(Pitchator)
 add_executable(PitchatorParameterFlushTests tests/ParameterFlushTests.cpp
  src/PitchatorVST.cpp src/PitchCurveView.cpp src/BrandLogoView.cpp
  ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp)
 target_compile_definitions(PitchatorParameterFlushTests PRIVATE PITCHATOR_NO_FACTORY=1)
 target_link_libraries(PitchatorParameterFlushTests PRIVATE sdk sdk_hosting vstgui_support)
 target_compile_features(PitchatorParameterFlushTests PRIVATE cxx_std_17)
 add_test(NAME PitchatorParameterFlushTests COMMAND PitchatorParameterFlushTests)

 add_executable(PitchatorDemoProcessorTests tests/DemoProcessorTests.cpp
  src/PitchatorVST.cpp src/PitchCurveView.cpp src/BrandLogoView.cpp
  ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp)
 target_compile_definitions(PitchatorDemoProcessorTests PRIVATE
  PITCHATOR_NO_FACTORY=1 PITCHATOR_DEMO_BUILD=1)
 target_link_libraries(PitchatorDemoProcessorTests PRIVATE sdk sdk_hosting vstgui_support)
 target_compile_features(PitchatorDemoProcessorTests PRIVATE cxx_std_17)
 add_test(NAME PitchatorDemoProcessorTests COMMAND PitchatorDemoProcessorTests)
endif()
