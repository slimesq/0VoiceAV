include_guard(GLOBAL)

option(VOICE_AV_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(voice_av_pad_target_numbers output_var target_name)
    string(REPLACE "_" ";" _components "${target_name}")
    list(TRANSFORM _components REPLACE "^([0-9])$" "0\\1")
    list(JOIN _components "_" _name)
    set(${output_var} "${_name}" PARENT_SCOPE)
endfunction()

function(voice_av_configure_warnings target_name)
    if(VOICE_AV_WARNINGS_AS_ERRORS)
        target_compile_options(${target_name} PRIVATE
            $<$<COMPILE_LANG_AND_ID:C,MSVC>:/WX>
            $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX>
            $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Werror>
            $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Werror>
        )
    endif()
endfunction()

function(voice_av_configure_target target_name)
    target_compile_options(${target_name} PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
        $<$<COMPILE_LANG_AND_ID:C,GNU>:-finput-charset=UTF-8>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-finput-charset=UTF-8>
        $<$<COMPILE_LANG_AND_ID:C,GNU>:-fexec-charset=UTF-8>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-fexec-charset=UTF-8>
        $<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:-finput-charset=UTF-8>
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-finput-charset=UTF-8>
    )

    voice_av_configure_warnings(${target_name})

    if(MSVC)
        if(VOICE_AV_USING_RELEASE_CONAN_DEPS)
            set_property(TARGET ${target_name} PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
            )
        endif()
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

function(voice_av_add_example_executable target_name)
    add_executable(${target_name} ${ARGN})
    voice_av_configure_target(${target_name})

    file(RELATIVE_PATH _example_relative_path "${VOICE_AV_EXAMPLES_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    string(REPLACE "\\" "/" _example_relative_path "${_example_relative_path}")
    if(NOT _example_relative_path MATCHES "^([^/]+)")
        message(FATAL_ERROR "Could not calculate example group folder for: ${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set_target_properties(${target_name} PROPERTIES
        FOLDER "voice_av/${CMAKE_MATCH_1}"
        RUNTIME_OUTPUT_DIRECTORY "${VOICE_AV_RUNTIME_OUTPUT_DIR}"
    )
    set_property(GLOBAL APPEND PROPERTY VOICE_AV_EXAMPLE_EXECUTABLES "${target_name}")
    message(STATUS "Added voice_av target: ${target_name}")
endfunction()

function(voice_av_add_current_example_executable)
    file(RELATIVE_PATH _project_relative_path "${VOICE_AV_EXAMPLES_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    string(REPLACE "\\" "/" _project_relative_path "${_project_relative_path}")

    if(NOT _project_relative_path MATCHES "^([0-9]+)-[^/]+/([^/]+)$")
        message(FATAL_ERROR "Could not calculate voice_av target name for: ${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set(_example_group_number "${CMAKE_MATCH_1}")
    set(_project_name "${CMAKE_MATCH_2}")
    string(TOLOWER "${_project_name}" _project_name)
    string(REGEX REPLACE "[^a-z0-9]+" "_" _project_name "${_project_name}")
    string(REGEX REPLACE "^_+|_+$" "" _project_name "${_project_name}")

    voice_av_pad_target_numbers(_target_name "voice_av_${_example_group_number}_${_project_name}")

    set(_source_patterns)
    foreach(_extension IN ITEMS c cpp cc cxx h hpp hh hxx)
        list(APPEND _source_patterns
            "${CMAKE_CURRENT_SOURCE_DIR}/*.${_extension}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.${_extension}"
            "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.${_extension}"
        )
    endforeach()

    # CMake may generate compiler probe sources in example-local build trees.
    file(GLOB _sources LIST_DIRECTORIES false CONFIGURE_DEPENDS
        ${_source_patterns}
    )
    voice_av_add_example_executable(${_target_name} ${_sources})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/inc")
        target_include_directories(${_target_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
    endif()
endfunction()
