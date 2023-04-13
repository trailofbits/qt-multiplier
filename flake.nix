{
  inputs.nixpkgs.url = "nixpkgs/nixos-unstable-small";
  inputs.nixpkgs2211.url = "nixpkgs/nixos-22.11-small";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.rust-overlay.url = "github:oxalica/rust-overlay";

  outputs = {
    self,
    nixpkgs,
    nixpkgs2211,
    rust-overlay,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        overlays = [(import rust-overlay)];
        pkgsFor = input: {extraOverlays ? []}:
          import input {
            inherit system;
            overlays = overlays ++ extraOverlays;
          };
        pkgs = pkgsFor nixpkgs {};
        pkgs2211 = pkgsFor nixpkgs2211 {};

        mold-linker = import ./nix/mold-linker.nix {
          pkgs = pkgs2211;
          nixpkgs = nixpkgs;
        };
        useMold = false;
        stdenv = pkgs.llvmPackages_15.stdenv;
        cclCXXWrapper = pkgs.writeShellScriptBin "cc" ''
          extraFlags=()
          [[ "$@" == *.rcgu.o* ]] && extraFlags+=("-lc++abi")
          exec ${stdenv.cc}/bin/cc $@ ''${extraFlags[@]}
        '';
      in {
        devShell =
          if useMold
          then mold-linker.inoculateDrv pkgs.mkShell
          else
            pkgs.mkShell.override {
              # override the stdenv with our llvm15 version to swap our compiler.
              inherit stdenv;
            }
            {
              nativeBuildInputs = [
                # XX: put a wrapped `clangd` early in the path
                pkgs.clang-tools_15

                # HACK: we use this to add the clang-15 libcxx before
                # the incorrect clang-11 one in ldflags.
                cclCXXWrapper
                stdenv.cc

                # wrapped `lld`
                pkgs.llvmPackages_15.bintools
              ];

              buildInputs = with pkgs; [
                # nix formatter
                alejandra

                # build tools
                cmake
                ninja

                # deps(qt-multiplier): qt
                qt6.qtbase
                qt6.qttools
                qt6.qt5compat

                # deps(weggli-native): rust
                rust-bin.nightly.latest.default
                rust-cbindgen
              ];
            };
      }
    );
}
