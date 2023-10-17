#!/usr/bin/env bash

QTSDK_REPOSITORY="https://code.qt.io/qt/qt5.git"
QTSDK_VERSION="v6.4.2"
BUILD_TYPE=Release
RELEASE_FLAGS="-fno-omit-frame-pointer -fno-optimize-sibling-calls -gline-tables-only"
DEBUG_FLAGS="-fno-omit-frame-pointer -fno-optimize-sibling-calls -O0 -g3"
FLAGS="${RELEASE_FLAGS}"
CONFIG_EXTRA=-release

main() {
  while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
    -h | --help)
      Help
      exit 0
      ;;
    --debug)
      BUILD_TYPE=Debug
      FLAGS="${DEBUG_FLAGS}"
      CONFIG_EXTRA=
      ;;
    --release)
      BUILD_TYPE=Release
      FLAGS="${RELEASE_FLAGS}"
      CONFIG_EXTRA=-release
      ;;
    *)
      ADD_ARGS+=("$1")
      ;;
    esac
    shift
  done


  clone_or_update_qtsdk
  configure_build
  build_project

  echo "Append the following path to the CMAKE_PREFIX_PATH: $(realpath qt5-build/qtbase/lib/cmake)"
  echo "If you already have a path, use ; as separator"

  return 0
}

build_project() {
  ( cd "qt5-build" && cmake --build . --parallel ) || panic "The build has failed"
}

configure_build() {
  mkdir -p "qt5-build"
  ( cd "qt5-build" && CC=`which clang` CXX=`which clang++` \
                      ../qt5/configure \
                      ${CONFIG_EXTRA} \
                      -developer-build \
                      -opensource \
                      -nomake examples \
                      -nomake tests \
                      -qt-zlib \
                      -qt-freetype \
                      -qt-pcre \
                      -qt-harfbuzz \
                      -qt-libjpeg \
                      -qt-libpng \
                      -no-xcb \
                      -no-gtk ) || panic "The configuration step has failed"
  
  CXXFLAGS="${FLAGS}" \
  CCFLAGS="${FLAGS}" \
  cmake \
      "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}" 
      -DCMAKE_C_COMPILER=`which clang` \
      -DCMAKE_CXX_COMPILER=`which clang++` \
      -S "qt5" -B "qt5-build" -DWARNINGS_ARE_ERRORS=OFF
}

clone_or_update_qtsdk() {
  if [[ ! -d "qt5" ]] ; then
    git clone "${QTSDK_REPOSITORY}" || panic "Failed to clone the Qt SDK repository"
  fi

  ( cd "qt5" && git fetch --tags )
  ( cd "qt5" && git reset --hard && git clean -ffdx )
  ( cd "qt5" && git reset "${QTSDK_VERSION}" ) || panic "Failed to update the Qt SDK repository"
  ( cd "qt5" && perl init-repository -f --module-subset=qtbase,qt5compat,-qtwebengine ) || panic "Failed to initialize the git submodules"
}

panic() {
  if [[ $# -ne 1 ]] ; then
    echo "No error description provided to panic()"
  else
    local error_message="$1"
    echo "${error_message}"
  fi

  exit 1
}

main "$@"
exit $?
