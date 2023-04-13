#!/usr/bin/env sh

set -euo pipefail

VCPKG_ROOT="vcpkg_install"
VCPKG_TARGET_TRIPLET="arm64-osx-rel"

vcpkg_toolchain=`realpath "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"`
export VCPKG_ROOT VCPKG_TARGET_TRIPLET

MACOSX_DEPLOYMENT_TARGET="12.0"
export MACOSX_DEPLOYMENT_TARGET

mkdir -p build

dep_flags() {
    echo -B "build/${dep}"
    if [[ "$1" = "qt-multiplier" ]]; then
        echo "-S" "."
    else
        echo "-S" "libraries/vendored/${dep}"
    fi

    case $1 in
        pasta)
            echo \
                -DPASTA_BOOTSTRAP_MACROS=OFF \
                -DPASTA_BOOTSTRAP_TYPES=OFF \
                -DPASTA_ENABLE_INSTALL=ON \
                -DPASTA_ENABLE_TESTING=OFF
            ;;
        gap)
            echo \
                -DVCPKG_MANIFEST_MODE=OFF \
                -DGAP_WARNINGS_AS_ERRORS=OFF \
                -DGAP_ENABLE_TESTING=OFF \
                -DGAP_ENABLE_EXAMPLES=OFF \
                -DGAP_ENABLE_COROUTINES=ON
            ;;
        multiplier)
            echo \
                -DMX_ENABLE_BOOTSTRAP=OFF \
                -DMX_ENABLE_WEGGLI=ON \
                -DMX_ENABLE_RE2=ON \
                -DMX_ENABLE_VAST=OFF \
                -DMX_ENABLE_INSTALL=ON
            ;;
        qt-multiplier)
            echo \
                -DMXQT_ENABLE_TESTS=ON
    esac
}

deps="weggli-native pasta gap multiplier"
[[ $# -gt 0 ]] && deps=$1

# linker_flags="-fuse-ld=`which ld64.lld`"
# -DCMAKE_EXE_LINKER_FLAGS="${linker_flags}" \
# -DCMAKE_MODULE_LINKER_FLAGS="${linker_flags}" \
# -DCMAKE_SHARED_LINKER_FLAGS="${linker_flags}" \

for dep in $deps; do
    echo $dep

    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_INSTALL_PREFIX=install \
        -DCMAKE_TOOLCHAIN_FILE="${vcpkg_toolchain}" \
        -DCMAKE_C_COMPILER=`which clang` \
        -DCMAKE_CXX_COMPILER=`which clang++` \
        -DVCPKG_ROOT="${VCPKG_ROOT}" \
        -DVCPKG_HOST_TRIPLET="${VCPKG_TARGET_TRIPLET}" \
        -DVCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}" \
        $(dep_flags "${dep}") \
        -G Ninja

    ninja -C "build/${dep}"

    ninja -C "build/${dep}" -t targets |
        grep -q '^install:' && \
            ninja -C "build/${dep}" install
done
