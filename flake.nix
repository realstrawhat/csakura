{
  description = "A sakura tree with falling petals for your terminal";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";

  outputs =
    { self, nixpkgs }:
    let
      supportedSystems = [
        "aarch64-linux"
        "x86_64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          csakura = pkgs.stdenv.mkDerivation {
            pname = "csakura";
            version = "2.0.0";
            src = self;

            nativeBuildInputs = [ pkgs.pkg-config ];
            buildInputs = [ pkgs.ncurses ];

            makeFlags = [ "PREFIX=$(out)" ];

            meta = {
              description = "Sakura tree with falling petals for your terminal";
              homepage = "https://github.com/realstrawhat/csakura";
              license = pkgs.lib.licenses.mit;
              mainProgram = "csakura";
              platforms = supportedSystems;
            };
          };

          default = self.packages.${system}.csakura;
        }
      );

      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-tree);
    };
}
