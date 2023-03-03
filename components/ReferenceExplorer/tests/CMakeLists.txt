#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

# The multiplier library can't open multiple databases in the same
# process. As a workaround, generate one binary for each test

set(test_file_name_list
  "NodeImporter"
  "ReferenceExplorerModel"
)

add_library("mx_referenceexplorer_tests_utils" STATIC
  src/Utils.h
  src/Utils.cpp
)

target_link_libraries("mx_referenceexplorer_tests_utils" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
  "multiplier::mx-api"
  
)

enable_qt_properties("mx_referenceexplorer_tests_utils")
enable_test_definitions("mx_referenceexplorer_tests_utils")

foreach(test_file_name ${test_file_name_list})
  set(target_name "mx_referenceexplorer_tests_${test_file_name}")

  add_executable("${target_name}"
    src/main.cpp
    "src/${test_file_name}.cpp"
  )

  target_include_directories("${target_name}" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
  )


  target_link_libraries("${target_name}" PRIVATE
    "qtmx_cxx_flags"
    "thirdparty_qt"
    "thirdparty_doctest"
    "multiplier::mx-api"
    "mx_referenceexplorer"
    "mx_referenceexplorer_tests_utils"
  )

  enable_qt_properties("${target_name}")
  enable_test_definitions("${target_name}")

  add_test(
    NAME "ReferenceExplorer_${test_file_name}"
    COMMAND "${target_name}"
  )
endforeach()
