#!/usr/bin/env bash
#
# bundle_for_offline.sh
#
# Creates a self-contained bundle for building qt-multiplier on a Linux
# machine without internet access.
#
# What gets bundled:
#   1. qt-multiplier source (with all submodules baked in, .git dirs stripped)
#   2. Qt 6 source (only the submodules needed: qtbase, qt5compat, and deps)
#   3. A build script that runs on the target machine
#
# Usage:
#   ./scripts/bundle_for_offline.sh [output_dir]
#
# Then transfer the output tarball to the Linux machine and run:
#   tar xf qt-multiplier-offline-bundle.tar.gz
#   cd qt-multiplier-offline-bundle
#   ./build.sh /path/to/multiplier-install /path/to/install-prefix
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="${1:-$(pwd)}"
BUNDLE_NAME="qt-multiplier-offline-bundle"
BUNDLE_DIR="$OUTPUT_DIR/$BUNDLE_NAME"

# Where the Qt source was cloned during the macOS build.
# Adjust if your build dir is different.
QT_SOURCE_DIR="${QT_SOURCE_DIR:-$HOME/Build/multiplier/Release/qt-multiplier/qt-src}"

echo "=== qt-multiplier offline bundle builder ==="
echo ""
echo "Source dir:    $SOURCE_DIR"
echo "Qt source dir: $QT_SOURCE_DIR"
echo "Output dir:    $OUTPUT_DIR"
echo ""

# --------------------------------------------------------------------------
# Validate inputs
# --------------------------------------------------------------------------

if [ ! -f "$SOURCE_DIR/CMakeLists.txt" ]; then
    echo "ERROR: Cannot find qt-multiplier CMakeLists.txt at $SOURCE_DIR"
    exit 1
fi

if [ ! -d "$QT_SOURCE_DIR/qtbase" ]; then
    echo "ERROR: Cannot find Qt source tree at $QT_SOURCE_DIR"
    echo "Set QT_SOURCE_DIR to the qt-src directory from a previous build."
    exit 1
fi

# --------------------------------------------------------------------------
# Create bundle directory
# --------------------------------------------------------------------------

rm -rf "$BUNDLE_DIR"
mkdir -p "$BUNDLE_DIR"

# --------------------------------------------------------------------------
# 1. Bundle qt-multiplier source
# --------------------------------------------------------------------------

echo ">>> Bundling qt-multiplier source..."

# Use rsync to copy source, excluding .git dirs and the vendored multiplier
# submodule (which is private and will be provided as a pre-built package).
rsync -a \
    --exclude='.git' \
    --exclude='libraries/vendored/multiplier/' \
    --exclude='build/' \
    --exclude='*.o' \
    --exclude='*.a' \
    --exclude='*.dylib' \
    --exclude='*.so' \
    "$SOURCE_DIR/" "$BUNDLE_DIR/qt-multiplier/"

echo "    qt-multiplier source: $(du -sh "$BUNDLE_DIR/qt-multiplier" | cut -f1)"

# --------------------------------------------------------------------------
# 2. Bundle Qt source (only needed submodules)
# --------------------------------------------------------------------------

echo ">>> Bundling Qt source (qtbase, qt5compat, and deps)..."

QT_BUNDLE="$BUNDLE_DIR/qt-source"
mkdir -p "$QT_BUNDLE"

# Copy top-level Qt files (configure script, CMakeLists.txt, etc.)
for f in "$QT_SOURCE_DIR"/*; do
    name="$(basename "$f")"
    if [ -f "$f" ]; then
        cp "$f" "$QT_BUNDLE/"
    fi
done
# Copy top-level cmake dir
if [ -d "$QT_SOURCE_DIR/cmake" ]; then
    rsync -a --exclude='.git' "$QT_SOURCE_DIR/cmake/" "$QT_BUNDLE/cmake/"
fi
# Copy LICENSES
if [ -d "$QT_SOURCE_DIR/LICENSES" ]; then
    rsync -a "$QT_SOURCE_DIR/LICENSES/" "$QT_BUNDLE/LICENSES/"
fi
# Copy coin (needed by configure)
if [ -d "$QT_SOURCE_DIR/coin" ]; then
    rsync -a --exclude='.git' "$QT_SOURCE_DIR/coin/" "$QT_BUNDLE/coin/"
fi

# Copy the submodules that were initialized.
# qt-multiplier needs: qtbase + qt5compat
# Qt's configure with -submodules also pulled in: qtdeclarative, qtshadertools,
# qtsvg, qtimageformats, qtlanguageserver, qtrepotools as transitive deps.
QT_SUBMODULES=(
    qtbase
    qt5compat
    qtdeclarative
    qtshadertools
    qtsvg
    qtimageformats
    qtlanguageserver
    qtrepotools
)

for mod in "${QT_SUBMODULES[@]}"; do
    if [ -d "$QT_SOURCE_DIR/$mod" ] && [ "$(ls "$QT_SOURCE_DIR/$mod" | wc -l)" -gt 2 ]; then
        echo "    Copying $mod..."
        rsync -a --exclude='.git' "$QT_SOURCE_DIR/$mod/" "$QT_BUNDLE/$mod/"
    else
        echo "    Skipping $mod (not initialized)"
    fi
done

# Create empty .gitmodules so configure doesn't complain
touch "$QT_BUNDLE/.gitmodules"

echo "    Qt source: $(du -sh "$QT_BUNDLE" | cut -f1)"

# --------------------------------------------------------------------------
# 3. Create the build script for the target machine
# --------------------------------------------------------------------------

echo ">>> Creating build script..."

cat > "$BUNDLE_DIR/build.sh" << 'BUILDSCRIPT'
#!/usr/bin/env bash
#
# Offline build script for qt-multiplier on Linux.
#
# Usage:
#   ./build.sh <multiplier-install-prefix> <install-prefix> [build-dir]
#
# Example:
#   ./build.sh /opt/multiplier /opt/multiplier ~/build/qt-multiplier
#
# Requirements:
#   - CMake 3.19+
#   - Ninja
#   - Python 3
#   - C/C++ compiler (clang or gcc)
#   - Development headers: libgl1-mesa-dev, libxkbcommon-dev, etc.
#
# For Ubuntu/Debian, install build dependencies with:
#   apt install build-essential cmake ninja-build python3 git \
#       libgl1-mesa-dev libxkbcommon-dev libxkbcommon-x11-dev \
#       libxcb-xinerama0-dev libxcb-cursor-dev libxcb-keysyms1-dev \
#       libxcb-shape0-dev libxcb-xfixes0-dev libxcb-icccm4-dev \
#       libxcb-image0-dev libxcb-render-util0-dev libxcb-randr0-dev \
#       libx11-xcb-dev libfontconfig1-dev libfreetype6-dev \
#       libinput-dev libudev-dev

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <multiplier-install-prefix> <install-prefix> [build-dir]"
    echo ""
    echo "  multiplier-install-prefix: Where multiplier is installed (has lib/cmake/multiplier/)"
    echo "  install-prefix:            Where to install qt-multiplier"
    echo "  build-dir:                 Build directory (default: ./build)"
    exit 1
fi

MULTIPLIER_PREFIX="$(cd "$1" && pwd)"
INSTALL_PREFIX="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
BUILD_DIR="${3:-$(pwd)/build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

QT_SOURCE="$SCRIPT_DIR/qt-source"
MXQT_SOURCE="$SCRIPT_DIR/qt-multiplier"

# Auto-detect compiler
if command -v clang++ >/dev/null 2>&1; then
    CC="${CC:-clang}"
    CXX="${CXX:-clang++}"
else
    CC="${CC:-gcc}"
    CXX="${CXX:-g++}"
fi

PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc 2>/dev/null || echo 4)}"

echo "=== qt-multiplier offline build ==="
echo ""
echo "Multiplier:  $MULTIPLIER_PREFIX"
echo "Install:     $INSTALL_PREFIX"
echo "Build dir:   $BUILD_DIR"
echo "Qt source:   $QT_SOURCE"
echo "Compiler:    $CXX"
echo "Parallel:    $PARALLEL_JOBS jobs"
echo ""

# -----------------------------------------------------------------------
# Step 1: Build Qt from the bundled source
# -----------------------------------------------------------------------

QT_BUILD_DIR="$BUILD_DIR/qt-build"
QT_INSTALL_DIR="$BUILD_DIR/qt-install"

if [ -f "$QT_INSTALL_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo ">>> Qt already built at $QT_INSTALL_DIR, skipping."
else
    echo ">>> Building Qt from bundled source..."
    mkdir -p "$QT_BUILD_DIR"

    cd "$QT_BUILD_DIR"
    "$QT_SOURCE/configure" \
        -prefix "$QT_INSTALL_DIR" \
        -opensource \
        -confirm-license \
        -nomake examples \
        -nomake tests \
        -release \
        -submodules qtbase,qt5compat \
        -xcb \
        -bundled-xcb-xinput \
        -- \
        -GNinja \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX"

    cmake --build . --parallel "$PARALLEL_JOBS"
    cmake --install .

    echo ">>> Qt installed to $QT_INSTALL_DIR"
fi

# -----------------------------------------------------------------------
# Step 2: Build qt-multiplier
# -----------------------------------------------------------------------

MXQT_BUILD_DIR="$BUILD_DIR/qt-multiplier"

echo ">>> Configuring qt-multiplier..."
mkdir -p "$MXQT_BUILD_DIR"

cmake \
    -G Ninja \
    -S "$MXQT_SOURCE" \
    -B "$MXQT_BUILD_DIR" \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_PREFIX_PATH="$MULTIPLIER_PREFIX;$QT_INSTALL_DIR" \
    -DMXQT_USE_SYSTEM_QT=ON \
    -DMXQT_ENABLE_MACDEPLOYQT=OFF \
    -DMXQT_GENERATE_LIBRARY_MANIFEST=OFF

echo ">>> Building qt-multiplier..."
cmake --build "$MXQT_BUILD_DIR" --parallel "$PARALLEL_JOBS"

echo ">>> Installing qt-multiplier..."
cmake --install "$MXQT_BUILD_DIR"

echo ""
echo "=== Build complete ==="
echo "Installed to: $INSTALL_PREFIX"
echo ""
echo "To run:"
echo "  export LD_LIBRARY_PATH=$QT_INSTALL_DIR/lib:$MULTIPLIER_PREFIX/lib:\$LD_LIBRARY_PATH"
echo "  $INSTALL_PREFIX/bin/multiplier --database /path/to/your.db"
BUILDSCRIPT

chmod +x "$BUNDLE_DIR/build.sh"

# --------------------------------------------------------------------------
# 4. Create the tarball
# --------------------------------------------------------------------------

echo ">>> Creating tarball..."

cd "$OUTPUT_DIR"
tar czf "${BUNDLE_NAME}.tar.gz" "$BUNDLE_NAME"

TARBALL_SIZE=$(du -sh "${BUNDLE_NAME}.tar.gz" | cut -f1)

echo ""
echo "=== Done ==="
echo "Bundle: $OUTPUT_DIR/${BUNDLE_NAME}.tar.gz ($TARBALL_SIZE)"
echo ""
echo "Transfer to Linux and run:"
echo "  tar xf ${BUNDLE_NAME}.tar.gz"
echo "  cd ${BUNDLE_NAME}"
echo "  ./build.sh /path/to/multiplier-install /path/to/install-prefix"
echo ""

# Clean up the uncompressed bundle dir
rm -rf "$BUNDLE_DIR"
