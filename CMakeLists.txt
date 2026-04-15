cmake_minimum_required(VERSION 3.23)

project(VoiceAVLearning VERSION 0.1.0 LANGUAGES C CXX)

set(VOICE_AV_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(VOICE_AV_EXAMPLES_DIR "${VOICE_AV_SOURCE_DIR}/voice_av")
set(VOICE_AV_CONAN_GENERATORS_DIR "${VOICE_AV_SOURCE_DIR}/build/generators")
set(VOICE_AV_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    VOICE_AV_USE_RELEASE_CONAN_DEPS
    "Use Release Conan packages when building non-Release multi-config CMake configs"
    ON
)

if(VOICE_AV_USE_RELEASE_CONAN_DEPS AND CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
endif()

list(PREPEND CMAKE_PREFIX_PATH
    "${CMAKE_BINARY_DIR}/generators"
    "${VOICE_AV_CONAN_GENERATORS_DIR}"
)

find_package(ffmpeg REQUIRED CONFIG)

function(voice_av_set_target_layout target_name target_folder)
    set_target_properties(${target_name} PROPERTIES
        FOLDER "${target_folder}"
        RUNTIME_OUTPUT_DIRECTORY "${VOICE_AV_RUNTIME_OUTPUT_DIR}"
    )
endfunction()

function(voice_av_configure_target target_name)
    if(MSVC)
        set_property(TARGET ${target_name} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
        )
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_link_options(${target_name} PRIVATE /IGNORE:4099)
    endif()

    if(TARGET ffmpeg::ffmpeg)
        target_link_libraries(${target_name} PRIVATE ffmpeg::ffmpeg)
    else()
        set(_ffmpeg_components
            ffmpeg::avdevice
            ffmpeg::avfilter
            ffmpeg::avformat
            ffmpeg::avcodec
            ffmpeg::swresample
            ffmpeg::swscale
            ffmpeg::avutil
            ffmpeg::postproc
        )

        foreach(_component IN LISTS _ffmpeg_components)
            if(TARGET ${_component})
                target_link_libraries(${target_name} PRIVATE ${_component})
            endif()
        endforeach()

        if(NOT TARGET ffmpeg::avformat)
            message(FATAL_ERROR "Conan generated ffmpeg package, but ffmpeg::avformat target was not found")
        endif()
    endif()
endfunction()

function(voice_av_add_executable target_name)
    add_executable(${target_name}
        ${ARGN}
    )
    voice_av_configure_target(${target_name})
    file(RELATIVE_PATH _target_folder "${VOICE_AV_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    string(REPLACE "\\" "/" _target_folder "${_target_folder}")
    voice_av_set_target_layout(${target_name} "${_target_folder}")
endfunction()

function(voice_av_example_group_folder output_var source_dir)
    file(RELATIVE_PATH _example_relative_path "${VOICE_AV_EXAMPLES_DIR}" "${source_dir}")
    string(REPLACE "\\" "/" _example_relative_path "${_example_relative_path}")

    if(NOT _example_relative_path MATCHES "^([^/]+)")
        message(FATAL_ERROR "Could not calculate example group folder for: ${source_dir}")
    endif()

    set(${output_var} "voice_av/${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(voice_av_add_example_executable target_name)
    add_executable(${target_name}
        ${ARGN}
    )
    voice_av_configure_target(${target_name})
    voice_av_example_group_folder(_target_folder "${CMAKE_CURRENT_SOURCE_DIR}")
    voice_av_set_target_layout(${target_name} "${_target_folder}")

    message(STATUS "Added voice_av target: ${target_name}")
endfunction()

function(voice_av_make_example_target_name output_var source_dir)
    file(RELATIVE_PATH _project_relative_path "${VOICE_AV_EXAMPLES_DIR}" "${source_dir}")
    string(REPLACE "\\" "/" _project_relative_path "${_project_relative_path}")

    if(NOT _project_relative_path MATCHES "^([0-9]+)-[^/]+/([^/]+)$")
        message(FATAL_ERROR "Could not calculate voice_av target name for: ${source_dir}")
    endif()

    set(_example_group_number "${CMAKE_MATCH_1}")
    set(_project_name "${CMAKE_MATCH_2}")
    string(TOLOWER "${_project_name}" _project_name)
    string(REGEX REPLACE "[^a-z0-9]+" "_" _project_name "${_project_name}")
    string(REGEX REPLACE "^_+|_+$" "" _project_name "${_project_name}")

    set(${output_var} "lesson_${_example_group_number}_${_project_name}" PARENT_SCOPE)
endfunction()

function(voice_av_collect_sources output_var)
    set(_source_patterns)
    foreach(_extension IN ITEMS c cpp cc cxx h hpp hh hxx)
        list(APPEND _source_patterns "${CMAKE_CURRENT_SOURCE_DIR}/*.${_extension}")
    endforeach()

    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
        ${_source_patterns}
    )
    set(${output_var} ${_sources} PARENT_SCOPE)
endfunction()

function(voice_av_add_current_example_executable)
    voice_av_make_example_target_name(_target_name "${CMAKE_CURRENT_SOURCE_DIR}")
    voice_av_collect_sources(_sources)
    voice_av_add_example_executable(${_target_name}
        ${_sources}
    )
endfunction()

function(voice_av_add_child_projects)
    file(GLOB _child_project_cmake_files CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt"
    )

    foreach(_child_project_cmake_file IN LISTS _child_project_cmake_files)
        get_filename_component(_child_project_dir "${_child_project_cmake_file}" DIRECTORY)
        add_subdirectory("${_child_project_dir}")
    endforeach()
endfunction()

file(GLOB voice_av_example_group_cmake_files CONFIGURE_DEPENDS
    RELATIVE "${VOICE_AV_EXAMPLES_DIR}"
    "${VOICE_AV_EXAMPLES_DIR}/*/CMakeLists.txt"
)

foreach(voice_av_example_group_cmake_file IN LISTS voice_av_example_group_cmake_files)
    get_filename_component(voice_av_example_group_dir "${voice_av_example_group_cmake_file}" DIRECTORY)
    add_subdirectory("${VOICE_AV_EXAMPLES_DIR}/${voice_av_example_group_dir}")
endforeach()
