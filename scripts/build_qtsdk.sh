#!/usr/bin/env bash

QTSDK_REPOSITORY="https://code.qt.io/qt/qt5.git"
QTSDK_VERSION="v6.7.0"
BUILD_TYPE=Release
RELEASE_FLAGS="-fno-omit-frame-pointer -fno-optimize-sibling-calls -gline-tables-only"
DEBUG_FLAGS="-fno-omit-frame-pointer -fno-optimize-sibling-calls -O0 -g3"
REDIST_FLAGS="${RELEASE_FLAGS}"
FLAGS="${RELEASE_FLAGS}"
OS_FLAGS=
CONFIG_EXTRA=-release
OS_ASAN_FLAGS=
EXTRA_CMAKE_FLAGS=

export CCC_OVERRIDE_OPTIONS="x-Werror"

main() {
  local is_redist_build=0

  # Get Linux dependencies.
  if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    sudo apt --purge autoremove -y \
      libicu-dev \
      python3.10-dev

    sudo apt install -y \
      '^libxcb.*-dev' \
      libx11-xcb-dev \
      libglu1-mesa-dev \
      libxrender-dev \
      libxi-dev \
      libxkbcommon-dev \
      libxkbcommon-x11-dev \
      libinput-dev \
      xserver-xorg-input-libinput \
      python3.11-dev

    REDIST_FLAGS="${REDIST_FLAGS} --disable_new_dtags -no-prefix -Wl,-rpath=\$ORIGIN/../lib"
    OS_ASAN_FLAGS="-fsanitize=address -ffunction-sections -fdata-sections -Wno-unused-command-line-argument"
  fi

  if [[ "$OSTYPE" == "darwin"* ]]; then
    OS_ASAN_FLAGS="-fsanitize=address -ffunction-sections -fdata-sections -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -Wno-unused-command-line-argument"
  fi

  while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
    -h | --help)
      Help
      exit 0
      ;;
    --asan)
      BUILD_TYPE=Debug
      FLAGS="${DEBUG_FLAGS} ${OS_ASAN_FLAGS}"
      CONFIG_EXTRA=
      EXTRA_CMAKE_FLAGS="-DCMAKE_EXE_LINKER_FLAGS=\"-fsanitize=address\""
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
    --redist)
      BUILD_TYPE=Release
      FLAGS="${REDIST_FLAGS}"
      CONFIG_EXTRA=-release
      is_redist_build=1
      ;;
    *)
      ADD_ARGS+=("$1")
      ;;
    esac
    shift
  done

  clone_or_update_qtsdk
  configure_build ${is_redist_build}
  build_project

  if [[ $is_redist_build != 0 ]] ; then
    install_project

    echo "Append the following path to the CMAKE_PREFIX_PATH: $(pwd)/qt5-install/usr/local/Qt-6.7.0)"
    echo "If you already have a path, use ; as separator"

  else
    echo "Append the following path to the CMAKE_PREFIX_PATH: $(pwd)/qt5-build/qtbase/lib/cmake)"
    echo "If you already have a path, use ; as separator"
  fi

  return 0
}

install_project() {
  mkdir "qt5-install"
  export DESTDIR=$(realpath "qt5-install")

  ( cd "qt5-build" &&  cmake --build . --parallel --target install ) || panic "The install target has failed"
}

build_project() {
  ( cd "qt5-build" && cmake --build . --parallel ) || panic "The build has failed"
}

configure_build() {
  local is_redist_build="$1"
  if [[ $is_redist_build == 0 ]] ; then
    local optional_developer_build_flag="-developer-build"
  fi

  if [[ "$OSTYPE" == "darwin"* ]]; then
    local opengl_flag="-no-opengl"
    local xcb_flag="-no-xcb"
  else
    local opengl_flag="-opengl desktop"
    local xcb_flag="-xcb-xlib -xcb -bundled-xcb-xinput"
    local input_flags="-libinput -evdev -libudev"
    local default_qpa="-qpa xcb"
  fi

  mkdir -p "qt5-build"
  ( cd "qt5-build" && CC=`which clang` CXX=`which clang++` \
                      ../qt5/configure \
                      ${CONFIG_EXTRA} \
                      ${opengl_flag} \
                      ${optional_developer_build_flag} \
                      ${default_qpa} \
                      ${xcb_flag} \
                      ${input_flags} \
                      -opensource \
                      -nomake examples \
                      -nomake tests \
                      -qt-zlib \
                      -qt-freetype \
                      -qt-pcre \
                      -qt-harfbuzz \
                      -qt-libjpeg \
                      -qt-libpng \
                      -no-gtk ) || panic "The configuration step has failed"
  
  CXXFLAGS="${FLAGS} ${OS_CXXFLAGS}" \
  CCFLAGS="${FLAGS} ${OS_CCFLAGS}" \
  cmake \
      "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}" \
      -DCMAKE_C_COMPILER=`which clang` \
      -DCMAKE_CXX_COMPILER=`which clang++` \
      ${EXTRA_CMAKE_FLAGS} \
      -S "qt5" \
      -B "qt5-build" \
      -DWARNINGS_ARE_ERRORS=OFF
}

clone_or_update_qtsdk() {
  if [[ ! -d "qt5" ]] ; then
    git clone "${QTSDK_REPOSITORY}" || panic "Failed to clone the Qt SDK repository"
  fi

  ( cd "qt5" && git fetch --tags )

  # Ensure we won't get conflicts when switching version
  ( cd "qt5" && git reset --hard && git clean -ffdx )
  ( cd "qt5" && git reset "${QTSDK_VERSION}" ) || panic "Failed to update the Qt SDK repository"

  # As we are now tracking a different tree, clean it up again
  ( cd "qt5" && git reset --hard && git clean -ffdx )
  ( cd "qt5" && perl init-repository -f --module-subset=qtbase,qt5compat,qtsvg,-qtwebengine ) || panic "Failed to initialize the git submodules"
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
