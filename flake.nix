{
  description = "Phylogenetic Trees";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          clang
          gnumake
          gdb
          valgrind
          perf
          time
        ];

        buildInputs = with pkgs; [

          clang
          gnumake
          gdb
          valgrind
          perf
          time

        ];

        RUST_SRC_PATH = "${pkgs.rustPlatform.rustLibSrc}";
      };
    };
}
