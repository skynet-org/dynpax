function(fixup_target_bundle TARGET_NAME)
  set(TARGET ${TARGET_NAME})
  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/FixBundle.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-FixBundle.cmake @ONLY)

  include(${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-FixBundle.cmake)
endfunction()
