# Graphical user interface for Multiplier

## Documentation

 * [Code documentation](https://upgraded-potato-g33zzjm.pages.github.io/)

## Build Instructions

For the time being, refer to the [GitHub Actions workflow](https://github.com/trailofbits/qt-multiplier/blob/next/.github/workflows/posix.yml) which goes through the installation of each dependency from scratch.

## Pre-built binaries

### CI builds

The CI workflow automatically uploads build artifacts. To download a pre-built binary:

 * Open the repository home page
 * Click on the green checkmark of the last commit (or use the commit history)
 * Open the `details` link of the macOS build entry
 * Select `Summary` in the top left corner of the page
 * Scroll down and download the relevant package for your system.

### Releases

Tagged releases can be found at the following link: [qt-multiplier releases](https://github.com/trailofbits/qt-multiplier/releases)

### deb-specific instructions

1. Install the package with the following command: `sudo dpkg -i package-name.deb`
2. Install the missing dependencies: `sudo apt install -f`

## Running qt-multiplier

**NOTE: A sample database created by indexing the uBPF library from IO Visor can be found in the build artifacts (see the `Pre-build binaries` section).**

1. Generate a new database using `mx-index` from the `multiplier` repository: `mx-index --target compile_commands.json --db database.mx`
2. Open the `Multiplier.app` application and select the database file

## Open source licenses

| Library name               | License      | Homepage                                                       |
|----------------------------|--------------|----------------------------------------------------------------|
| Qt 6                       | LGPL         | https://github.com/qt/qt5                                      |
| Phantom Style              | LGPL         | https://github.com/randrew/phantomstyle                        |
| Qt-Advanced-Docking-System | LGPL         | https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System |
| doctest                    | MIT          | https://github.com/doctest/doctest                             |
| xxHash                     | BSD 2-Clause | https://github.com/Cyan4973/xxHash                             |
