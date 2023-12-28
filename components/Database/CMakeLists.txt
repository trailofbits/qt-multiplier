#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_database"
  include/multiplier/GUI/EntityInformation.h
  include/multiplier/GUI/IndexedTokenRangeData.h
  include/multiplier/GUI/RPCErrorCode.h
  include/multiplier/GUI/Result.h

  include/multiplier/GUI/IDatabase.h
  src/IDatabase.cpp

  src/Database.h
  src/Database.cpp

  src/requests/GetIndexedTokenRangeData.h
  src/requests/GetIndexedTokenRangeData.cpp

  src/requests/GetEntityName.h
  src/requests/GetEntityName.cpp

  src/requests/GetEntityList.h
  src/requests/GetEntityList.cpp

  src/requests/GetEntityInformation.h
  src/requests/GetEntityInformation.cpp

  src/requests/GetRelatedEntities.h
  src/requests/GetRelatedEntities.cpp
)

target_link_libraries("mx_database"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_multiplier_library"
    "xxhash::xxhash"
    "mx_util_component"
)

target_include_directories("mx_database" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_database" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_database")
