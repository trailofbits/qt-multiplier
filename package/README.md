# Creating redistributable packages

## Linux

### Building the project

Build the Qt SDK. This will create a folder named `qt5-install` which from now on will be referred to as `qt_redist_path` 

```bash
./qt-multiplier/scripts/build_qtsdk.sh --redist
```

Build Multiplier and install it in a folder which from now on will be referred to as `install_path`.

Build and install qt-multiplier:
 * Pass the Multiplier install path with `-DCMAKE_INSTALL_PREFIX=install_path`
 * Pass the Qt SDK redist path with `-DQT_REDIST_PATH=qt_redist_path`

### Creating the DEB package

```bash
cmake \
  -S "./qt-multiplier/package" \
  -B "package-build" \
  -DQT_MULTIPLIER_DATA_PATH=install_path \
  -DQT_REDIST_PATH=qt_redist_path \
  -DCPACK_GENERATOR=DEB

cmake \
  --build package_build \
  --target package
```

### Additional notes

Ensure that the dependency list specified in `qt-multiplier/package/generators/DEB.cmake` matches the version of the packages used in the build environment.

It is best to build on Ubuntu 22.04, as that will allow packages to work on later versions as well.
