# Creating redistributable packages

## Linux

### Building the project

Build the Qt SDK. This will create a folder named `qt5-install` which from now on will be referred to as `qt_redist_path`.

```bash
./qt-multiplier/scripts/build_qtsdk.sh --redist
```

Build multiplier and install it in a folder which from now on will be referred to as `multiplier_install_path`.
Build qt-multiplier and install it in a folder which from now on will be referred to as `qt_multiplier_install_path`.

Configure the packaging project passing the following arguments:
 * -DMULTIPLIER_DATA_PATH=multiplier_install_path
 * -DQT_MULTIPLIER_DATA_PATH=qt_multiplier_install_path
 * -DQT_REDIST_PATH=qt_redist_path
 * -DQTMULTIPLIER_VERSION=1.1.2 (try to follow what's in `<qt-multiplier-root>/cmake/version.cmake`)

### Creating the DEB package

```bash
cmake \
  -DCPACK_GENERATOR=DEB \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja \
  -S qt-multiplier/package \
  -B build/package \
  -DQT_MULTIPLIER_DATA_PATH="$(realpath install/qt-multiplier)" \
  -DQT_REDIST_PATH="$(realpath qt-multiplier/scripts/qt5-install)" \
  -DQTMULTIPLIER_VERSION=1.1.2 \
  -DMULTIPLIER_DATA_PATH="$(realpath install)"

cmake \
  --build build/package \
  --target package
```

### Additional notes

Ensure that the dependency list specified in `qt-multiplier/package/generators/DEB.cmake` matches the version of the packages used in the build environment.

It is best to build on Ubuntu 22.04, as that will allow packages to work on later versions as well.
