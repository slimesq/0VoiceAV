cmake_minimum_required(VERSION 3.22)

project(VoiceAVLearning VERSION 0.1.0 LANGUAGES C CXX)

option(VOICE_AV_WINDOWS_ALL "Build the Windows desktop course examples alongside the main examples" OFF)
option(VOICE_AV_COURSE_ONLY "Build only the selected course targets" OFF)

include(cmake/VoiceAVExamples.cmake)

if(NOT VOICE_AV_COURSE_ONLY)
    add_subdirectory(voice_av/00-ffmpeg-version)
    file(GLOB _example_cmake_files CONFIGURE_DEPENDS
        RELATIVE "${VOICE_AV_EXAMPLES_DIR}"
        "${VOICE_AV_EXAMPLES_DIR}/08-ffmpeg-demuxing-and-decoding/*/CMakeLists.txt"
        "${VOICE_AV_EXAMPLES_DIR}/09-ffmpeg-encoding-and-muxing/*/CMakeLists.txt"
        "${VOICE_AV_EXAMPLES_DIR}/10-ffmpeg-filtering/*/CMakeLists.txt"
    )
    foreach(_example_cmake_file IN LISTS _example_cmake_files)
        get_filename_component(_example_dir "${_example_cmake_file}" DIRECTORY)
        add_subdirectory("${VOICE_AV_EXAMPLES_DIR}/${_example_dir}")
    endforeach()
endif()

if(VOICE_AV_WINDOWS_ALL)
    if(NOT WIN32 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Use the Windows x64 MSVC preset for all desktop examples.")
    endif()
    if(NOT VOICE_AV_COURSE_TARGETS)
        set(VOICE_AV_COURSE_TARGETS
            voice_av_02_* voice_av_06_* voice_av_07_* voice_av_08_camera_dshow
            voice_av_11_* voice_av_12_* voice_av_14_* voice_av_15_* voice_av_19_*
            voice_av_37_gdb_* voice_av_37_make_*
        )
    endif()
endif()
if(VOICE_AV_WINDOWS_ALL OR VOICE_AV_COURSE_ONLY OR VOICE_AV_COURSE_TARGETS)
    add_subdirectory(cmake/course)
endif()

if(VOICE_AV_WINDOWS_ALL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    get_property(_examples GLOBAL PROPERTY VOICE_AV_EXAMPLE_EXECUTABLES)
    set(_chapter_entries)
    foreach(_target IN LISTS _examples)
        get_target_property(_folder ${_target} FOLDER)
        list(APPEND _chapter_entries "${_folder}/${_target}")
    endforeach()
    list(SORT _chapter_entries COMPARE NATURAL)
    list(LENGTH _chapter_entries _example_count)
    set(_report "Windows desktop examples in chapter order (${_example_count})\n")
    foreach(_entry IN LISTS _chapter_entries)
        get_filename_component(_target "${_entry}" NAME)
        string(APPEND _report "${_target}\n")
    endforeach()
    set(_report_path "${CMAKE_BINARY_DIR}/windows-targets.txt")
    file(CONFIGURE OUTPUT "${_report_path}" CONTENT "${_report}" @ONLY)

    # Print the chapter list after the independent targets finish in parallel.
    add_custom_target(voice_av_build_summary ALL
        COMMAND "${CMAKE_COMMAND}" -E cat "${_report_path}"
        VERBATIM)
    add_dependencies(voice_av_build_summary ${_examples})
endif()
