#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_database"
  include/multiplier/ui/EntityInformation.h
  include/multiplier/ui/IndexedTokenRangeData.h
  include/multiplier/ui/RPCErrorCode.h
  include/multiplier/ui/Result.h

  include/multiplier/ui/IDatabase.h
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
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "xxhash::xxhash"
    "multiplier::mx-api"
    "multiplier::mx-analysis"
    "mx_util"
)

target_include_directories("mx_database" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_database" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_database")
