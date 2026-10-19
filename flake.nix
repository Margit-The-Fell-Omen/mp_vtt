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
      in
      {
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
          ];

          shellHook = ''
            echo "Adventure Board dev shell ready."
          '';
        };
      }
    );
}