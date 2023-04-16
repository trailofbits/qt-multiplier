{
  nixpkgs,
  pkgs,
}: let
  moldPatched = pkgs.mold.overrideAttrs (o: {
    patchPhase =
      (o.patchPhase or "")
      + ''
        sed -iE 's/return.*list element expected.*/break;/' macho/yaml.cc
      '';
  });

  bintoolsFixupAdapter = let
    mkBintools = stdenv:
      stdenv.cc.bintools.overrideAttrs (old: {
        postFixup =
          old.postFixup
          + ''
            # rm $out/nix-support/post-link-hook
            # the no_uuid flag causes the linker to generate invalid binaries.
            sed -i "/-no_uuid/d" $out/nix-support/libc-ldflags-before
            wrap ld64.mold ${nixpkgs}/pkgs/build-support/bintools-wrapper/ld-wrapper.sh ${moldPatched}/bin/ld64.mold
          '';
      });
  in
    stdenv:
      stdenv.override (oldStdenv: {
        cc = oldStdenv.cc.override {
          bintools = mkBintools oldStdenv;
        };
      });

  stdenvAdapters = import "${nixpkgs}/pkgs/stdenv/adapters.nix" {
    inherit pkgs;
    lib = pkgs.lib;
    config = pkgs.config;
  };
in rec {
  stdenv = stdenvAdapters.useMoldLinker (bintoolsFixupAdapter pkgs.llvmPackages_15.stdenv);
  mold = pkgs.writeShellScriptBin "ld64.mold" ''
    extendedFlags=("-syslibroot" "/")
    NIX_LD_USE_RESPONSE_FILE=0 exec ${stdenv.cc.bintools}/bin/ld64.mold $@ ''${extendedFlags[@]}
  '';
  inoculateDrv = drv: attrs:
    (drv.override
      {
        inherit stdenv;
      }
      attrs)
    .overrideAttrs (o: {
      # mold is buggy and its LC_FUNCTION_STARTS and LC_CODE_SIGNATURE don't pass install_name_tool.
      # We simply disable LC_FUNCTION_STARTS generation and leave codesigning to the codesign tool.
      NIX_CFLAGS_LINK = "-fuse-ld=${mold}/bin/ld64.mold -Wl,-no_function_starts,-no_adhoc_codesign";
    });
}
