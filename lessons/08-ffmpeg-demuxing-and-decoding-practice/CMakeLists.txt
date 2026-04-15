file(GLOB lesson_08_sources CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*/main.c"
    "*/main.cpp"
    "*/main.cc"
    "*/main.cxx"
)

set(lesson_08_test_files_dir "${CMAKE_CURRENT_SOURCE_DIR}/testFiles")
set(lesson_08_05_decode_audio_DEBUGGER_ARGS
    "believe.aac"
    "believe.pcm"
)
set(lesson_08_06_decode_video_DEBUGGER_ARGS
    "source.200kbps.768x320_10s.h264"
    "source.yuv"
)

set(lesson_08_example_dirs)

foreach(lesson_08_source IN LISTS lesson_08_sources)
    get_filename_component(lesson_08_example_dir "${lesson_08_source}" DIRECTORY)

    if(lesson_08_example_dir IN_LIST lesson_08_example_dirs)
        message(WARNING
            "Multiple main files found in lesson 08 example '${lesson_08_example_dir}'. "
            "Using the first one found."
        )
        continue()
    endif()

    list(APPEND lesson_08_example_dirs "${lesson_08_example_dir}")
    string(TOLOWER "${lesson_08_example_dir}" lesson_08_example_id)
    string(REGEX REPLACE "[^a-z0-9]+" "_" lesson_08_example_id "${lesson_08_example_id}")
    string(REGEX REPLACE "^_+" "" lesson_08_example_id "${lesson_08_example_id}")
    string(REGEX REPLACE "_+$" "" lesson_08_example_id "${lesson_08_example_id}")
    set(lesson_08_target "lesson_08_${lesson_08_example_id}")

    add_executable(${lesson_08_target}
        "${lesson_08_source}"
    )
    configure_voice_av_target(${lesson_08_target})
    set_target_properties(${lesson_08_target} PROPERTIES
        FOLDER "lessons/08-ffmpeg-demuxing-and-decoding-practice"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${lesson_08_test_files_dir}"
    )

    if(DEFINED ${lesson_08_target}_DEBUGGER_ARGS)
        string(REPLACE ";" " " lesson_08_debugger_args "${${lesson_08_target}_DEBUGGER_ARGS}")
        set_target_properties(${lesson_08_target} PROPERTIES
            VS_DEBUGGER_COMMAND_ARGUMENTS "${lesson_08_debugger_args}"
        )
    endif()

    message(STATUS "Added lesson 08 target: ${lesson_08_target}")
endforeach()
