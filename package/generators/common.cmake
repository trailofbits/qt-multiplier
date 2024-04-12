#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(standard_executable_permissions
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_WRITE GROUP_EXECUTE
  WORLD_READ             WORLD_EXECUTE
)

set(standard_resource_permissions
  OWNER_READ OWNER_WRITE
  GROUP_READ GROUP_WRITE
  WORLD_READ
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(dyn_lib_ext "so")
else()
    set(dyn_lib_ext "dylib")
endif()
