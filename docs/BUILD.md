# Building Multiplier GUI

This document provides comprehensive instructions for building the Multiplier GUI from source, based on the automated build process used in our CI system.

## Table of Contents

- [Prerequisites](#prerequisites)
- [System Dependencies](#system-dependencies)
- [Build Environment Setup](#build-environment-setup)
- [Development Build](#development-build)
- [Production Build](#production-build)
- [Package Creation](#package-creation)
- [Build Configuration Reference](#build-configuration-reference)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Tools

- **CMake**: Minimum version 3.19
- **Clang/LLVM**: Version 17.0 (recommended)
- **Python**: Version 3.11+ with development headers
- **Git**: For repository management and submodules
- **ccache**: For build acceleration (optional but recommended)

### Supported Platforms

- **Linux**: Ubuntu 20.04+ (tested on Ubuntu 23.10)
- **macOS**: 10.15+ with Xcode command line tools

## System Dependencies

### Ubuntu/Debian

Install the core dependencies:

```bash
sudo apt update
sudo apt install -y \
    ccache \
    cmake \
    flex \
    bison \
    qt6-base-dev \
    libqt6core5compat6-dev \
    libgl1-mesa-dev \
    libglvnd-dev \
    cbindgen \
    gh \
    python3.11-dev
```

Install additional Qt and graphics dependencies:

```bash
sudo apt install -y \
    '^libxcb.*-dev' \
    libx11-xcb-dev \
    libglu1-mesa-dev \
    libxrender-dev \
    libxi-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-dev \
    libinput-dev \
    xserver-xorg-input-libinput
```

### macOS

Install dependencies via Homebrew:

```bash
brew install cmake ccache llvm@17 python@3.11
```

Install Xcode command line tools:

```bash
xcode-select --install
```

## Build Environment Setup

### 1. Clone Repository

```bash
git clone https://github.com/trailofbits/qt-multiplier.git
cd qt-multiplier
```

### 2. Initialize Submodules

```bash
git submodule update --init --recursive libraries/doctest/src
git submodule update --init --recursive libraries/phantomstyle/src
git submodule update --init --recursive libraries/xxhash/src
git submodule update --init --recursive libraries/qt-advanced-docking-system/src
git submodule update --init libraries/vendored/multiplier
```

### 3. Set Environment Variables

```bash
# Set build paths
export CCACHE_DIR="$HOME/.ccache"
export LLVM_PATH="/path/to/llvm-18"  # Adjust path as needed

# For production builds
export MULTIPLIER_RELEASE="44e350e"  # Current release version
export GITHUB_TOKEN="your_github_token"  # For downloading releases
```

## Development Build

Development builds are optimized for debugging and rapid iteration.

### 1. Build Qt SDK (Development)

```bash
# Build Qt SDK with debug symbols
./scripts/build_qtsdk.sh --debug
```

This will create a Qt installation in `qt5-build/` directory.

### 2. Configure CMake (Development)

```bash
cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DMXQT_ENABLE_TESTS=true \
    -DQt6_DIR="$(pwd)/qt5-build/qtbase/lib/cmake/Qt6" \
    -DQt6Gui_DIR="$(pwd)/qt5-build/qtbase/lib/cmake/Qt6Gui" \
    -S . \
    -B build_debug
```

### 3. Build

```bash
cmake --build build_debug --parallel $(nproc)
```

### 4. Run

```bash
./build_debug/application/Multiplier
```

## Production Build

Production builds are optimized for distribution and include all dependencies.

### 1. Build Qt SDK (Production)

```bash
# Build Qt SDK for redistribution
./scripts/build_qtsdk.sh --redist
```

This creates a distributable Qt installation in `qt5-install/` directory.

### 2. Download Multiplier Framework

```bash
# Download the required Multiplier release
folder_name="multiplier-${MULTIPLIER_RELEASE}"
archive_name="${folder_name}.tar.xz"

gh release download ${MULTIPLIER_RELEASE} \
    --repo trailofbits/multiplier \
    --pattern "${archive_name}"

mkdir "${folder_name}"
tar -xf "${archive_name}" --directory "${folder_name}"
export MULTIPLIER_INSTALL_DIRECTORY="$(pwd)/${folder_name}"
```

### 3. Configure CMake (Production)

```bash
cmake \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${LLVM_PATH}/bin/clang" \
    -DCMAKE_CXX_COMPILER="${LLVM_PATH}/bin/clang++" \
    -DMXQT_ENABLE_TESTS=true \
    -DMXQT_MULTIPLIER_VERSION="${MULTIPLIER_RELEASE}" \
    -Dmultiplier_DIR="${MULTIPLIER_INSTALL_DIRECTORY}/lib/cmake/multiplier" \
    -Dgap_DIR="${MULTIPLIER_INSTALL_DIRECTORY}/lib/cmake/gap" \
    -DQt6_DIR="$(pwd)/qt5-install/usr/local/Qt-6.7.0/lib/cmake/Qt6" \
    -DQt6Gui_DIR="$(pwd)/qt5-install/usr/local/Qt-6.7.0/lib/cmake/Qt6Gui" \
    -S . \
    -B build_release
```

### 4. Build and Install

```bash
# Build the application
cmake --build build_release --parallel $(nproc)

# Install to staging directory
export QT_MULTIPLIER_INSTALL_DIRECTORY="$(pwd)/qt_multiplier_install"
export DESTDIR="${QT_MULTIPLIER_INSTALL_DIRECTORY}"
cmake --build build_release --target install
```

## Package Creation

### DEB Package (Linux)

Create a Debian package for distribution:

```bash
cmake \
    -S package \
    -B package-build \
    -DQT_MULTIPLIER_DATA_PATH="${QT_MULTIPLIER_INSTALL_DIRECTORY}/usr/local" \
    -DMULTIPLIER_DATA_PATH="${MULTIPLIER_INSTALL_DIRECTORY}" \
    -DQT_REDIST_PATH="$(pwd)/qt5-install" \
    -DCPACK_GENERATOR=DEB

cmake --build package-build --target package
```

The resulting `.deb` file will be in the `package-build/` directory.

### Installation

Install the package:

```bash
sudo dpkg -i package-build/multiplier-*.deb
sudo apt install -f  # Install any missing dependencies
```

## Build Configuration Reference

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `MXQT_ENABLE_TESTS` | `true` | Enable test suite |
| `MXQT_ENABLE_MACDEPLOYQT` | `true` | Build portable binaries |
| `MXQT_ENABLE_INSTALL` | `true` | Enable install directives |
| `MXQT_GENERATE_LIBRARY_MANIFEST` | `true` | Generate library manifest |
| `MXQT_EVAL_COPY` | `false` | Add evaluation copy label |
| `MXQT_MULTIPLIER_VERSION` | - | Multiplier framework version |

### Compiler Flags

#### Debug Build
```
-fno-omit-frame-pointer -fno-optimize-sibling-calls -O0 -g3
```

#### Release Build
```
-fno-omit-frame-pointer -fno-optimize-sibling-calls -gline-tables-only
```

#### Common Flags
```
-Wall -pedantic -Wconversion -Wunused -Wshadow -fvisibility=hidden 
-Werror -Wno-c99-designator
```

### Qt Configuration

The build system compiles Qt 6.7.0 with the following configuration:

#### Linux
```bash
CC=clang CXX=clang++ \
configure \
    -release \
    -opengl desktop \
    -xcb-xlib -xcb -bundled-xcb-xinput \
    -libinput -evdev -libudev \
    -qpa xcb \
    -opensource \
    -nomake examples \
    -nomake tests \
    -qt-zlib -qt-freetype -qt-pcre \
    -qt-harfbuzz -qt-libjpeg -qt-libpng \
    -no-gtk
```

#### macOS
```bash
CC=clang CXX=clang++ \
configure \
    -release \
    -opensource \
    -nomake examples \
    -nomake tests \
    -qt-zlib -qt-freetype -qt-pcre \
    -qt-harfbuzz -qt-libjpeg -qt-libpng
```

## Troubleshooting

### Common Issues

#### Qt Not Found
```
CMake Error: Could not find Qt6
```
**Solution**: Ensure Qt6_DIR points to the correct CMake files:
```bash
-DQt6_DIR="$(pwd)/qt5-build/qtbase/lib/cmake/Qt6"
```

#### Multiplier Framework Not Found
```
Could not find package configuration file: multiplier-config.cmake
```
**Solution**: Verify MULTIPLIER_INSTALL_DIRECTORY and multiplier_DIR:
```bash
export MULTIPLIER_INSTALL_DIRECTORY="$(pwd)/multiplier-44e350e"
-Dmultiplier_DIR="${MULTIPLIER_INSTALL_DIRECTORY}/lib/cmake/multiplier"
```

#### Missing LLVM
```
Could not find clang compiler
```
**Solution**: Install LLVM 17 and set the path:
```bash
export LLVM_PATH="/usr/lib/llvm-17"  # Ubuntu
export LLVM_PATH="/opt/homebrew/opt/llvm@17"  # macOS Homebrew
```

#### Submodule Issues
```
fatal: No url found for submodule path 'libraries/...'
```
**Solution**: Re-initialize submodules:
```bash
git submodule deinit --all
git submodule update --init --recursive
```

### Build Performance

- **Use ccache**: Significantly speeds up rebuilds
- **Parallel builds**: Use `-j$(nproc)` or `--parallel $(nproc)`
- **SSD storage**: Improves I/O performance during builds
- **RAM**: 8GB+ recommended for parallel builds

### Debugging Build Issues

1. **Check dependencies**: Ensure all system packages are installed
2. **Verify paths**: Double-check all path variables
3. **Clean build**: Remove build directories and start fresh
4. **Verbose output**: Add `-v` to cmake build commands
5. **Check logs**: Review CMake configuration output for warnings

### Running Tests

```bash
# After building
cmake --build build_debug --target test

# Or run ctest directly
cd build_debug
ctest --output-on-failure
```

### Development Tips

- Use debug builds for development and debugging
- Use ccache to speed up incremental builds
- Keep separate build directories for debug and release
- Use your IDE's CMake integration for easier development
- Consider using Qt Creator for GUI development

For additional help, refer to the [GitHub Actions workflow](.github/workflows/posix.yml) which contains the exact commands used in our CI system.