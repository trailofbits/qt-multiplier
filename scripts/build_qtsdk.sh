#!/usr/bin/env bash

QTSDK_REPOSITORY="https://code.qt.io/qt/qt5.git"
QTSDK_VERSION="v6.4.2"

main() {
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
  ( cd "qt5-build" && ../qt5/configure -developer-build -opensource -nomake examples -nomake tests ) || panic "The configuration step has failed"
  cmake -S "qt5" -B "qt5-build" -DWARNINGS_ARE_ERRORS=OFF
}

clone_or_update_qtsdk() {
  if [[ ! -d "qt5" ]] ; then
    git clone "${QTSDK_REPOSITORY}" || panic "Failed to clone the Qt SDK repository"
  fi

  ( cd "qt5" && git fetch --tags )
  ( cd "qt5" && git reset --hard && git clean -ffdx )
  ( cd "qt5" && git reset "${QTSDK_VERSION}" ) || panic "Failed to update the Qt SDK repository"
  ( cd "qt5" && perl init-repository -f --module-subset=default,-qtwebengine ) || panic "Failed to initialize the git submodules"
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

main $@
exit $?
