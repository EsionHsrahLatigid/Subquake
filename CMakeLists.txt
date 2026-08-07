cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(Subquake VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(Subquake VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(SUBQUAKE_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(SUBQUAKE_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(SUBQUAKE_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(SUBQUAKE_CREATE_AU OFF)
if(APPLE)
    set(SUBQUAKE_CREATE_AU ON)
endif()
set(SUBQUAKE_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(subquake_engine STATIC
    source/SubquakeEngine.cpp)
target_include_directories(subquake_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(subquake_engine PUBLIC cxx_std_20)
set_target_properties(subquake_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(subquake_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SUBQUAKE_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(subquake_engine_tests
        tests/SubquakeEngineTests.cpp)
    target_link_libraries(subquake_engine_tests PRIVATE subquake_engine)
    target_compile_features(subquake_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(subquake_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(subquake_engine_tests)
    add_test(NAME subquake_engine_tests COMMAND subquake_engine_tests)
endif()

if(SUBQUAKE_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${SUBQUAKE_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${SUBQUAKE_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME subquake
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "Subquake"
        TARGET_APP_ID "audio.2bit.subquake"
        TARGET_APP_NAMESPACE "audio.2bit"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "audio.2bit.Subquake"
        PLUGIN_NAME "Subquake"
        PLUGIN_VENDOR "2bit"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "MIDI-triggered low-frequency pressure noise instrument"
        PLUGIN_URL "https://github.com/2bbb/Subquake"
        PLUGIN_IS_SYNTH ON
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${SUBQUAKE_CREATE_AU}
        PLUGIN_AU_SUBTYPE "SbQk"
        PLUGIN_AU_MANUFACTURER "2Bit"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(subquake_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/SubquakePlugin.cpp
        source/SubquakePlugin.h)
    target_link_libraries(subquake_shared INTERFACE subquake_engine)
    target_include_directories(subquake_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    set(subquake_release_bundle_targets
        subquake_standalone_plugin
        subquake_vst3_plugin)
    if(APPLE)
        list(APPEND subquake_release_bundle_targets subquake_au_plugin)
    endif()

    add_custom_target(subquake_release_bundles
        DEPENDS ${subquake_release_bundle_targets})

    if(APPLE)
        set_target_properties(subquake_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "audio.2bit.subquake"
            MACOSX_BUNDLE_BUNDLE_NAME "Subquake"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "audio.2bit.subquake")
    endif()

    if(APPLE AND SUBQUAKE_ADHOC_SIGN)
        find_program(SUBQUAKE_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET subquake_vst3_plugin POST_BUILD
            COMMAND "${SUBQUAKE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:subquake_vst3_plugin>"
            COMMENT "Ad-hoc signing Subquake VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET subquake_standalone_plugin POST_BUILD
            COMMAND "${SUBQUAKE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:subquake_standalone_plugin>"
            COMMENT "Ad-hoc signing Subquake standalone app for local launch"
            VERBATIM)
    endif()
endif()
