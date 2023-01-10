#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

if(PLATFORM_MACOS)
  set(CMAKE_INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(PLATFORM_LINUX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()
