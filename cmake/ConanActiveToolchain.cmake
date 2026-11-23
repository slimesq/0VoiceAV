# The dependency scripts publish this file at build/<platform>/generators/conan_toolchain.cmake.
set(_voice_av_flavor_file "${CMAKE_CURRENT_LIST_DIR}/active-flavor.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_voice_av_flavor_file}")
file(STRINGS "${_voice_av_flavor_file}" _voice_av_active_flavor LIMIT_COUNT 1)
if(NOT _voice_av_active_flavor MATCHES "^(light|full)$")
    message(FATAL_ERROR "Invalid Conan dependency selection. Run the project dependency setup script again.")
endif()
set(VOICE_AV_ACTIVE_CONAN_DIR "${CMAKE_CURRENT_LIST_DIR}/${_voice_av_active_flavor}")
include("${VOICE_AV_ACTIVE_CONAN_DIR}/conan_toolchain.cmake")
