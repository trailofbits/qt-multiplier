#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

option(MXQT_ENABLE_TESTS "Set to true to enable tests" true)
option(MXQT_ENABLE_MACDEPLOYQT "Set to true to build portable binaries" true)
option(MXQT_ENABLE_INSTALL "Set to true to enable the install directives" true)
option(MXQT_GENERATE_LIBRARY_MANIFEST "Set to true to generate the library manifest" true)
option(MXQT_EVAL_COPY "Set to true to enable an 'evaluation copy' label" false)

set(MXQT_MULTIPLIER_VERSION "" CACHE STRING "The release of the multiplier library")
if(NOT MXQT_MULTIPLIER_VERSION)
  message(FATAL_ERROR "qt-multiplier: The MXQT_MULTIPLIER_VERSION parameter is mandatory")

else()
  find_package(Git REQUIRED)

  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/libraries/vendored/multiplier"
    OUTPUT_VARIABLE "rev_parse_output"
    RESULT_VARIABLE "rev_parse_error"
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(NOT ${rev_parse_error} EQUAL 0)
    message(FATAL_ERROR "qt-multiplier: Failed to determine which multiplier is being vendored")
  endif()

  if(NOT "${rev_parse_output}" STREQUAL "${MXQT_MULTIPLIER_VERSION}")
    message(FATAL_ERROR "qt-multiplier: The vendored multiplier is not at the specified version: MXQT_MULTIPLIER_VERSION=${MXQT_MULTIPLIER_VERSION} != submodule=${rev_parse_output}. Please update: libraries/vendored/multiplier, the -DMXQT_MULTIPLIER_VERSION=... parameter and the version in .github/workflows/build.yml")
  endif()

  message(STATUS "qt-multiplier: Using multiplier ${rev_parse_output}")
endif()
