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
        packages = with pkgs; [
          clang-tools # wrapped clangd (knows Nix include paths), keep before clang
          clang
          bear # `bear -- make` generates compile_commands.json for clangd
          gnumake
          gdb
          valgrind
          perf
          time
        ];
      };
    };
}
