{
  description = "Adventure Board - TTRPG note-taking, projection & initiative app";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        androidPkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };
        androidNdk = androidPkgs.androidenv.androidPkgs.ndk-bundle;
      in
      {
        devShells.android = androidPkgs.mkShell {
          packages = with androidPkgs; [
            cmake
            ninja
            jdk17
            gradle
            androidenv.androidPkgs.ndk-bundle
            androidenv.androidPkgs.platform-tools
          ];

          shellHook = ''
            export ANDROID_NDK_HOME="${androidNdk}/libexec/android-sdk/ndk/29.0.14206865"
            export ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"
            echo "Android NDK ready at $ANDROID_NDK_HOME"
            echo "Set ANDROID_HOME to an SDK with platform 36/build-tools 36.1."
          '';
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            clang
            pkg-config
            libx11
            libxcursor
            libxinerama
            libxrandr
            libxi
            libGL
            wayland
            wayland-scanner
            libxkbcommon
            fontconfig
            dejavu_fonts
            python3
          ];

          # GLFW dlopen()s these shared libraries at runtime rather than
          # linking against them, so they must be discoverable via the
          # loader path (Nix keeps them out of the default linker paths).
          LD_LIBRARY_PATH = with pkgs; pkgs.lib.makeLibraryPath [
            wayland
            libxkbcommon
            libx11
            libGL
            libxcursor
            libxinerama
            libxrandr
            libxi
            fontconfig
          ];

          shellHook = ''
            ./scripts/gen_compile_commands.sh 2>/dev/null || true
            echo "Adventure Board dev shell ready."
          '';
        };
      }
    );
}