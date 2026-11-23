function(voice_av_import_android_ffmpeg output_libraries)
    if(NOT ANDROID_ABI)
        message(FATAL_ERROR
            "Configure this module with the Android NDK toolchain and set ANDROID_ABI.")
    endif()

    get_filename_component(ffmpeg_lib_dir
        "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}" ABSOLUTE)
    set(ffmpeg_libraries avcodec avdevice avfilter avformat avutil postproc swresample swscale)
    set(missing_libraries "")
    foreach(component IN LISTS ffmpeg_libraries)
        set(library "${ffmpeg_lib_dir}/lib${component}.so")
        if(NOT EXISTS "${library}" OR IS_DIRECTORY "${library}")
            list(APPEND missing_libraries "lib${component}.so")
        endif()
    endforeach()

    if(missing_libraries)
        string(REPLACE ";" ", " missing_names "${missing_libraries}")
        message(FATAL_ERROR
            "Missing FFmpeg shared libraries for Android ABI ${ANDROID_ABI}.\n"
            "Expected directory: ${ffmpeg_lib_dir}\n"
            "Missing files: ${missing_names}\n"
            "See voice_av/37-android-ndk/README.md for setup.")
    endif()

    foreach(component IN LISTS ffmpeg_libraries)
        add_library(${component} SHARED IMPORTED)
        set_target_properties(${component} PROPERTIES
            IMPORTED_LOCATION "${ffmpeg_lib_dir}/lib${component}.so")
    endforeach()
    set(${output_libraries} ${ffmpeg_libraries} PARENT_SCOPE)
endfunction()
