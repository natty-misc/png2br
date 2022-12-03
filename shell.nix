let
  pkgs = import <nixpkgs> {};
  ignoringVulns = x: x // { meta = (x.meta // { knownVulnerabilities = []; }); };
  libavIgnoringVulns = pkgs.libav_12.overrideAttrs ignoringVulns;
in
  pkgs.mkShell {
    buildInputs = with pkgs; [
      ffmpeg
      libavIgnoringVulns
      libpng
      cmake
      clang
      llvmPackages.libcxxClang
      libcxxStdenv
    ];
    nativeBuildInputs = with pkgs; [
      pkg-config
    ];
  }
