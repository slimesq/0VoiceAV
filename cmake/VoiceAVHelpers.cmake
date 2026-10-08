function(voice_av_set_target_layout target_name target_folder)
    set_target_properties(${target_name} PROPERTIES
        FOLDER "${target_folder}"
        RUNTIME_OUTPUT_DIRECTORY "${VOICE_AV_RUNTIME_OUTPUT_DIR}"
    )
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

    if(VOICE_AV_WARNINGS_AS_ERRORS)
        target_compile_options(${target_name} PRIVATE
            $<$<COMPILE_LANG_AND_ID:C,MSVC>:/WX>
            $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX>
            $<$<COMPILE_LANG_AND_ID:C,GNU>:-Werror>
            $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Werror>
            $<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:-Werror>
            $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Werror>
        )
    endif()

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

    set(${output_var} "voice_av_${_example_group_number}_${_project_name}" PARENT_SCOPE)
endfunction()

# Get the current example's target name before or after creating the target.
function(voice_av_get_current_example_target_name output_var)
    voice_av_make_example_target_name(_target_name "${CMAKE_CURRENT_SOURCE_DIR}")
    set(${output_var} "${_target_name}" PARENT_SCOPE)
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
    voice_av_get_current_example_target_name(_target_name)
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

function(voice_av_add_example_groups)
    file(GLOB _example_group_cmake_files CONFIGURE_DEPENDS
        RELATIVE "${VOICE_AV_EXAMPLES_DIR}"
        "${VOICE_AV_EXAMPLES_DIR}/*/CMakeLists.txt"
    )

    foreach(_example_group_cmake_file IN LISTS _example_group_cmake_files)
        get_filename_component(_example_group_dir "${_example_group_cmake_file}" DIRECTORY)
        add_subdirectory("${VOICE_AV_EXAMPLES_DIR}/${_example_group_dir}")
    endforeach()
endfunction()
