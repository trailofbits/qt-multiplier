#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)

include("cmake/ccache.cmake")
include("cmake/vcpkg_helper.cmake")

project(qt-multiplier CXX)

include("cmake/system.cmake")
include("cmake/utils.cmake")

if(PLATFORM_MACOS)
  include("cmake/macdeployqt.cmake")
endif()

set(MX_QT_VERSION "5" CACHE STRING "Which version to use for the Qt SDK")

set(MX_QT "Qt${MX_QT_VERSION}")
set(MX_GUI_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(MX_QT_COMPONENTS Core Widgets)
if(MX_QT_VERSION GREATER "5")
  list(APPEND MX_QT_COMPONENTS Core5Compat)
endif()
find_package(${MX_QT} COMPONENTS ${MX_QT_COMPONENTS} REQUIRED)

find_package(multiplier CONFIG REQUIRED)

find_package(sqlite-multiplier CONFIG REQUIRED)
find_package(Python3 3.7 MODULE REQUIRED COMPONENTS Development)
find_package(py-multiplier CONFIG REQUIRED)

if(PLATFORM_MACOS)
  set(CMAKE_INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(PLATFORM_LINUX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

set(exe_name "mx-gui")
add_executable("${exe_name}"

  Resources/resources.qrc
  
  Code.h
  
  CodeTheme.cpp
  CodeTheme.h
  
  CodeBrowserView.cpp
  CodeBrowserView.h
  
  CodeSearchResults.cpp
  CodeSearchResults.h
  
  CodeView.cpp
  CodeView.h
  
  Configuration.cpp
  Configuration.h
  
  Event.cpp
  Event.h

  FileBrowserView.cpp
  FileBrowserView.h

  FileView.cpp
  FileView.h
  
  HistoryBrowserView.cpp
  HistoryBrowserView.h
  
  IndexMonitorThread.cpp
  IndexMonitorThread.h

  Main.cpp
  
  Multiplier.cpp
  Multiplier.h
  
  OmniBoxView.cpp
  OmniBoxView.h

  OpenConnectionDialog.cpp
  OpenConnectionDialog.h

  PythonCompletionModel.cpp
  PythonCompletionModel.h

  PythonOutputAdapter.cpp
  PythonOutputAdapter.h

  PythonPromptView.cpp
  PythonPromptView.h

  ReferenceBrowserView.cpp
  ReferenceBrowserView.h
  
  TitleNamePrompt.cpp
  TitleNamePrompt.h
  
  Util.cpp
  Util.h
)

target_link_libraries("${exe_name}" PRIVATE
  multiplier::mx-api
  ${MX_QT}::Core
  ${MX_QT}::Widgets
  sqlite-multiplier::mxsqlitebridge
  Python3::Python
  py-multiplier::py-multiplier
)

if(MX_QT_VERSION GREATER "5")
  target_link_libraries("${exe_name}" PRIVATE
    ${MX_QT}::Core5Compat
  )
endif()

set_target_properties("${exe_name}"
  PROPERTIES
    AUTOMOC TRUE
    AUTOUIC TRUE
    AUTORCC TRUE
)

target_compile_definitions("${exe_name}" PRIVATE
  PY_SSIZE_T_CLEAN
)

# Find libLTO.so or libLTO.dylib. LLVM relies on these, and LLVM is used in
# PASTA, which is used in mx-index and mx-import.
find_library(lib_lto_path LTO)
if("${lib_lto_path}" STREQUAL "lib_lto_path-NOTFOUND")
  message(FATAL_ERROR "Could not find libLTO")
else()
  message(STATUS "Found LTO library: ${lib_lto_path}")
endif()
get_filename_component(lib_lto_name "${lib_lto_path}" NAME)

# Find libRemarks.so or libRemarks.dylib. LLVM relies on these, and LLVM is
# used in PASTA, which is used in mx-index and mx-import.
find_library(lib_remarks_path Remarks)
if("${lib_remarks_path}" STREQUAL "lib_remarks_path-NOTFOUND")
  message(FATAL_ERROR "Could not find libRemarks")
else()
  message(STATUS "Found Remarks library: ${lib_remarks_path}")
endif()
get_filename_component(lib_remarks_name "${lib_remarks_path}" NAME)

if(PLATFORM_MACOS)
  find_program(install_name_tool install_name_tool)
  if("${install_name_tool}" STREQUAL "install_name_tool-NOTFOUND")
    message(FATAL_ERROR "Could not find install_name_tool")  
  endif()

  set(icon_path "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/Logo.ico")
  set(plist_path "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")

  target_link_libraries("${exe_name}"
    PRIVATE
      "-framework AppKit"
  )
  
  target_sources("${exe_name}" PRIVATE
    "${icon_path}"
    "${plist_path}"
    "MacOSUtils.h"
    "MacOSUtils.mm"
  )
  
  set_source_files_properties("${icon_path}"
    PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources"
  )
  
  set_source_files_properties("${plist_path}"
    PROPERTIES
      MACOSX_PACKAGE_LOCATION "Contents"
  )
  set(output_bundle_path "${CMAKE_CURRENT_BINARY_DIR}/Multiplier.app")

  get_target_property(multiplier_include_dir "multiplier::mx-common" INTERFACE_INCLUDE_DIRECTORIES)
  set(multiplier_library_path ${multiplier_DIR})
  
  add_custom_command(
    OUTPUT
      "${output_bundle_path}"
    DEPENDS 
      "${install_name_tool}"
      "$<TARGET_FILE:mx-gui>"
      "$<TARGET_FILE:multiplier::mx-index>"
      "$<TARGET_FILE:multiplier::mx-import>"
      "$<TARGET_FILE:sqlite-multiplier::mxsqlitebridge>"
      "${lib_lto_path}"
      "${lib_remarks_path}"
      "${plist_path}"
      "${icon_path}"
    
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents/Frameworks"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents/MacOS"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents/Resources"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents/bin"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents/lib"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents/PlugIns"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_bundle_path}/Contents/include"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:mx-gui>" "${output_bundle_path}/Contents/MacOS/Multiplier"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:multiplier::mx-index>" "${output_bundle_path}/Contents/bin/mx-index"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:multiplier::mx-import>" "${output_bundle_path}/Contents/bin/mx-import"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:sqlite-multiplier::mxsqlitebridge>" "${output_bundle_path}/Contents/lib/mxsqlitebridge.a"
    COMMAND "${CMAKE_COMMAND}" -E copy "${lib_lto_path}" "${output_bundle_path}/Contents/lib/${lib_lto_name}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${lib_remarks_path}" "${output_bundle_path}/Contents/lib/${lib_remarks_name}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${multiplier_include_dir}/multiplier" "${output_bundle_path}/Contents/include/multiplier"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${multiplier_library_path}" "${output_bundle_path}/Contents/lib/cmake/multiplier"
    COMMAND "${install_name_tool}" -change "@rpath/${lib_lto_name}" "@loader_path/../lib/${lib_lto_name}" "${output_bundle_path}/Contents/bin/mx-index"
    COMMAND "${install_name_tool}" -change "@rpath/${lib_lto_name}" "@loader_path/../lib/${lib_lto_name}" "${output_bundle_path}/Contents/bin/mx-import"
    COMMAND "${install_name_tool}" -change "@rpath/${lib_remarks_name}" "@loader_path/../lib/${lib_remarks_name}" "${output_bundle_path}/Contents/bin/mx-index"
    COMMAND "${install_name_tool}" -change "@rpath/${lib_remarks_name}" "@loader_path/../lib/${lib_remarks_name}" "${output_bundle_path}/Contents/bin/mx-import"
    COMMAND "${CMAKE_COMMAND}" -E copy "${plist_path}" "${output_bundle_path}/Contents/Info.plist"
    COMMAND "${CMAKE_COMMAND}" -E copy "${icon_path}" "${output_bundle_path}/Contents/Resources/Logo.ico"
  )
  
  add_custom_target("mx-gui-bundle" ALL
    DEPENDS "${output_bundle_path}"
  )
  
  qt_deploy_target("mx-gui-bundle" "${output_bundle_path}")

  mx_deploy_targets("mx-gui-bundle" "${output_bundle_path}")
endif(PLATFORM_MACOS)

if(MX_ENABLE_INSTALL)
  install(
    TARGETS
      "${exe_name}"
    DESTINATION
      "${CMAKE_INSTALL_BINDIR}")


  if(PLATFORM_MACOS)
    install(
      DIRECTORY
        "${output_bundle_path}"
      DESTINATION
        "."
      USE_SOURCE_PERMISSIONS)
  endif(PLATFORM_MACOS)
endif(MX_ENABLE_INSTALL)
