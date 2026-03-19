#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")

# Minimal runtime dependencies — Qt and multiplier libs are bundled.
# We only need the low-level system libraries that can't be bundled.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.35), libglx0, libxcb1, libxcb-xinput0, libx11-xcb1, libxkbcommon0, libxkbcommon-x11-0"
)

# =============================================================================
# Install the entire bundle into /opt/multiplier
# =============================================================================

# Binaries
install(
  DIRECTORY "${BUNDLE_DIR}/bin/"
  DESTINATION "/opt/multiplier/bin"
  USE_SOURCE_PERMISSIONS
  PATTERN "qt.conf" EXCLUDE
)

# qt.conf must be next to the executable
install(
  FILES "${BUNDLE_DIR}/bin/qt.conf"
  DESTINATION "/opt/multiplier/bin"
)

# Shared libraries (Qt, multiplier, ICU, etc.)
install(
  DIRECTORY "${BUNDLE_DIR}/lib/"
  DESTINATION "/opt/multiplier/lib"
  USE_SOURCE_PERMISSIONS
)

# Qt plugins (platforms, imageformats, etc.)
if(EXISTS "${BUNDLE_DIR}/plugins")
  install(
    DIRECTORY "${BUNDLE_DIR}/plugins/"
    DESTINATION "/opt/multiplier/plugins"
    USE_SOURCE_PERMISSIONS
  )
endif()

# Icons and other shared resources
if(EXISTS "${BUNDLE_DIR}/share")
  install(
    DIRECTORY "${BUNDLE_DIR}/share/"
    DESTINATION "/opt/multiplier/share"
  )
endif()

# =============================================================================
# System integration
# =============================================================================

# Symlink in /usr/local/bin so `multiplier` is on PATH
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E create_symlink
    "/opt/multiplier/bin/multiplier"
    "${CMAKE_CURRENT_BINARY_DIR}/multiplier"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/multiplier"
  DESTINATION "/usr/local/bin"
)

# .desktop file for application menus
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/linux/multiplier.desktop"
  DESTINATION "/usr/share/applications"
)

# Application icon in the XDG icon hierarchy so desktop environments find it
if(EXISTS "${BUNDLE_DIR}/share/icons/logo.png")
  # Install at multiple sizes for best compatibility.
  # The PNG is high-res so the 256x256 hicolor slot works well.
  install(
    FILES "${BUNDLE_DIR}/share/icons/logo.png"
    DESTINATION "/usr/share/icons/hicolor/256x256/apps"
    RENAME "multiplier.png"
  )

  # Also keep a copy in the bundle for the .desktop Icon= path
  install(
    FILES "${BUNDLE_DIR}/share/icons/logo.png"
    DESTINATION "/opt/multiplier/share/icons"
  )
endif()
