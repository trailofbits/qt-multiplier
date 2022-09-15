# Graphical user interface for Multiplier

## Build Instructions 

### macOS 

Going forward, we assume the environment variable `WORKSPACE_DIR` dir represents
the directory where everything goes.

#### Prerequisites 

Install [Multiplier](https://github.com/trailofbits/multiplier) and it's dependencies to `WORKSPACE_DIR`.

Install [SQlite Multiplier](https://github.com/trailofbits/sqlite-multiplier) and it's dependencies to `WORKSPACE_DIR`.

Install [Python bindings for Multiplier](https://github.com/trailofbits/py-multiplier) and it's dependencies to `WORKSPACE_DIR`.

Install QT5

```shell
brew install qt@5
```

#### Build QT Multiplier


```shell
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/usr/local/opt;${WORKSPACE_DIR}/install"
  -DCMAKE_INSTALL_PREFIX="${WORKSPACE_DIR}/install/qt-multiplier" \
  -DVCPKG_ROOT="${VCPKG_ROOT}" \
  -DVCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}" \
  -DCMAKE_CXX_COMPILER=`which clang++` \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/usr/local/opt;${WORKSPACE_DIR}/install"
  -S . -B ~/build/qt-multiplier
  
cmake --build ${WORKSPACE_DIR}/build/qt-multiplier

```

## Run QT Multiplier

Within `${WORKSPACE_DIR}/build/qt-multiplier` there should be a file called `Multipier.app`. Open this file to launch the GUI.

On macOS this can be done via `open ${WORKSPACE_DIR}/build/qt-multiplier/Multipier.app`

## Example Workflow

- Select "Launch new indexer" from the "File" menu

- Create os select a directory (e.g. /tmp/ws)

- Use the default options for host and port. Wait from approximately 3 seconds

- Select "Import Build" from the "File" menu and choose compile_commands.json of interest. Wait again. Larger files will require longer wait time.
