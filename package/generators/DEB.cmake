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

# Auto-detect runtime dependencies by scanning ELF headers of all
# bundled binaries and shared libraries. dpkg-shlibdeps maps each
# needed .so to the correct Debian package automatically.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# The bundled libraries (Qt, ICU, multiplier) live in /usr/local/lib
# inside the package. Tell dpkg-shlibdeps to look there so it doesn't
# flag them as unresolved, while still detecting true system deps.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${BUNDLE_DIR}/lib")

# =============================================================================
# Install the entire bundle into /usr/local
# =============================================================================

# Binaries — goes to /usr/local/bin (already on PATH)
install(
  DIRECTORY "${BUNDLE_DIR}/bin/"
  DESTINATION "/usr/local/bin"
  USE_SOURCE_PERMISSIONS
  PATTERN "qt.conf" EXCLUDE
)

# qt.conf must be next to the executable
install(
  FILES "${BUNDLE_DIR}/bin/qt.conf"
  DESTINATION "/usr/local/bin"
)

# Shared libraries — goes to /usr/local/lib (standard linker search path)
install(
  DIRECTORY "${BUNDLE_DIR}/lib/"
  DESTINATION "/usr/local/lib"
  USE_SOURCE_PERMISSIONS
)

# Qt plugins
if(EXISTS "${BUNDLE_DIR}/plugins")
  install(
    DIRECTORY "${BUNDLE_DIR}/plugins/"
    DESTINATION "/usr/local/plugins"
    USE_SOURCE_PERMISSIONS
  )
endif()

# Headers (for development against multiplier)
if(EXISTS "${BUNDLE_DIR}/include")
  install(
    DIRECTORY "${BUNDLE_DIR}/include/"
    DESTINATION "/usr/local/include"
  )
endif()

# Shared resources (icons, license, etc.)
if(EXISTS "${BUNDLE_DIR}/share")
  install(
    DIRECTORY "${BUNDLE_DIR}/share/"
    DESTINATION "/usr/local/share"
  )
endif()

# =============================================================================
# System integration
# =============================================================================

# ldconfig: ensure /usr/local/lib is picked up by the dynamic linker
install(CODE "
  file(WRITE \"\$ENV{DESTDIR}/etc/ld.so.conf.d/multiplier.conf\"
       \"/usr/local/lib\\n\")
")

# .desktop file for application menus
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/linux/multiplier.desktop"
  DESTINATION "/usr/share/applications"
)

# Application icons in the XDG icon hierarchy at multiple sizes.
# The CI generates these from the source logo.png via ImageMagick.
foreach(icon_size 16 24 32 48 64 128 256)
  set(icon_file "${BUNDLE_DIR}/share/icons/multiplier-${icon_size}.png")
  if(EXISTS "${icon_file}")
    install(
      FILES "${icon_file}"
      DESTINATION "/usr/share/icons/hicolor/${icon_size}x${icon_size}/apps"
      RENAME "multiplier.png"
    )
  endif()
endforeach()

# Fallback: if sized icons weren't generated, install the original
if(NOT EXISTS "${BUNDLE_DIR}/share/icons/multiplier-256.png")
  if(EXISTS "${BUNDLE_DIR}/share/icons/logo.png")
    install(
      FILES "${BUNDLE_DIR}/share/icons/logo.png"
      DESTINATION "/usr/share/icons/hicolor/256x256/apps"
      RENAME "multiplier.png"
    )
  endif()
endif()
