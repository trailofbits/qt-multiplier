# Creating redistributable packages

## Linux

### Build

Build the Qt SDK: `./qt-multiplier/scripts/build_qtsdk.sh --redist`. This creates the `qt_redist_path` folder

Build and install `llvm-project` into its own folder: `llvm_path`

Build and install `multiplier` into the final install folder: `install_path`. You will have to pass `-DCMAKE_PREFIX_PATH=llvm_path`

Build and install `qt-multiplier` into the final install folder: `install_path`. You will have to pass `-DCMAKE_PREFIX_PATH=llvm_path;qt_redist_path`

### Packaging

```
cmake \
  -S "qt-multiplier/package" \
  -B "package-build" \
  -DQT_MULTIPLIER_DATA_PATH=install_path \
  -DQT_REDIST_PATH=qt_redist_path \
  -DCPACK_GENERATOR=DEB

cmake \
  --build package_build \
  --target package
```
