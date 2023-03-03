#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)
project("sample_database01")

set(CMAKE_EXPORT_COMPILE_COMMANDS true CACHE BOOL "Export the compile_commands.json file (forced)" FORCE)

add_executable("sample_database01"
  main.c
)

target_compile_options("sample_database01" PRIVATE
  -O0
  -g3
)
