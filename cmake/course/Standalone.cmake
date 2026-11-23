# Standalone examples use their project name to select the shared course target.
set(VOICE_AV_COURSE_TARGETS "${PROJECT_NAME}")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/course")
