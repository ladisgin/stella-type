{
  description = "Stella typechecker";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      packageFor = system:
        let pkgs = nixpkgs.legacyPackages.${system};
        in pkgs.stdenv.mkDerivation {
          pname = "stella-typechecker";
          version = "0.1.0";

          src = pkgs.lib.fileset.toSource {
            root = ./.;
            fileset = pkgs.lib.fileset.unions [
              ./CMakeLists.txt
              ./Stella.cf
              ./src/main.cpp
              ./lib
              ./tests
            ];
          };

          nativeBuildInputs = with pkgs; [ cmake haskellPackages.BNFC bison flex ];
          nativeCheckInputs = [ pkgs.python3 ];
          doCheck = true;
          checkPhase = ''
            runHook preCheck
            python3 ../tests/check.py ./typechecker
            runHook postCheck
          '';

          installPhase = ''
            runHook preInstall
            install -Dm755 typechecker "$out/bin/typechecker"
            runHook postInstall
          '';

          meta.mainProgram = "typechecker";
        };
    in {
      packages = forAllSystems (system: {
        default = packageFor system;
      });

      devShells = forAllSystems (system:
        let pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
            packages = [ pkgs.clang-tools pkgs.python3 ];
          };
        });
    };
}
