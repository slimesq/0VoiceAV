include_guard(GLOBAL)

option(
    VOICE_AV_USE_RELEASE_CONAN_DEPS
    "Use Release Conan packages for non-Release builds when Debug packages are unavailable"
    ON
)

function(voice_av_map_release_imports)
    foreach(_config IN ITEMS DEBUG RELWITHDEBINFO MINSIZEREL)
        set(CMAKE_MAP_IMPORTED_CONFIG_${_config} Release PARENT_SCOPE)
    endforeach()
endfunction()

function(voice_av_find_ffmpeg repo_root release_dependencies)
    string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" _host)
    set(_managed_generators "${repo_root}/build/${_host}/generators")

    if(VOICE_AV_CONAN_GENERATORS_DIR)
        set(_generators "${VOICE_AV_CONAN_GENERATORS_DIR}")
    elseif(VOICE_AV_ACTIVE_CONAN_DIR)
        set(_generators "${VOICE_AV_ACTIVE_CONAN_DIR}")
    elseif(CMAKE_TOOLCHAIN_FILE)
        get_filename_component(_generators "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    elseif(EXISTS "${_managed_generators}/active-flavor.txt")
        set(_active_flavor "${_managed_generators}/active-flavor.txt")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_active_flavor}")
        file(STRINGS "${_active_flavor}" _flavor LIMIT_COUNT 1)
        if(NOT _flavor MATCHES "^(light|full)$")
            message(FATAL_ERROR "Invalid Conan dependency selection. Run the dependency setup script again.")
        endif()
        set(_generators "${_managed_generators}/${_flavor}")
    elseif(EXISTS "${_managed_generators}/ffmpeg-config.cmake")
        set(_generators "${_managed_generators}")
    else()
        set(_generators "${repo_root}/build/generators")
    endif()

    cmake_path(IS_PREFIX _managed_generators "${_generators}" NORMALIZE _uses_managed_generators)
    if(_uses_managed_generators)
        # Cached configs from another flavor can override the new search path.
        get_cmake_property(_cache_entries CACHE_VARIABLES)
        foreach(_entry IN LISTS _cache_entries)
            if(_entry MATCHES "_DIR$")
                get_property(_path CACHE "${_entry}" PROPERTY VALUE)
                cmake_path(IS_PREFIX _managed_generators "${_path}" NORMALIZE _is_managed)
                cmake_path(IS_PREFIX _generators "${_path}" NORMALIZE _is_selected)
                if(_is_managed AND NOT _is_selected)
                    unset(${_entry} CACHE)
                endif()
            endif()
        endforeach()
        foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
            cmake_path(IS_PREFIX _managed_generators "${_prefix}" NORMALIZE _is_managed)
            cmake_path(IS_PREFIX _generators "${_prefix}" NORMALIZE _is_selected)
            if(_is_managed AND NOT _is_selected)
                list(REMOVE_ITEM CMAKE_PREFIX_PATH "${_prefix}")
            endif()
        endforeach()
        list(PREPEND CMAKE_PREFIX_PATH "${_generators}")
    else()
        list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/generators" "${_generators}")
    endif()

    set(_release_only OFF)
    if(VOICE_AV_USE_RELEASE_CONAN_DEPS
        AND EXISTS "${_generators}/ffmpeg-Target-release.cmake"
        AND NOT EXISTS "${_generators}/ffmpeg-Target-debug.cmake"
    )
        set(_release_only ON)
        voice_av_map_release_imports()
    endif()

    find_package(ffmpeg 7.1.3 EXACT REQUIRED CONFIG)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    set(${release_dependencies} "${_release_only}" PARENT_SCOPE)
endfunction()
