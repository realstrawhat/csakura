{
  description = "A sakura tree with falling petals for your terminal (cmatrix-style)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: rec {
        csakura = pkgs.stdenv.mkDerivation {
          pname = "csakura";
          version = "2.0.0";

          src = self;

          nativeBuildInputs = [ pkgs.pkg-config ];
          buildInputs = [ pkgs.ncurses ];

          makeFlags = [ "PREFIX=${placeholder "out"}" "CC=cc" ];

          meta = with pkgs.lib; {
            description = "A sakura tree with falling petals for your terminal (cmatrix-style)";
            homepage = "https://github.com/realstrawhat/csakura";
            license = licenses.mit;
            mainProgram = "csakura";
            platforms = platforms.unix;
          };
        };

        default = csakura;
      });

      apps = forAllSystems (pkgs: rec {
        csakura = {
          type = "app";
          program = "${self.packages.${pkgs.system}.csakura}/bin/csakura";
        };
        default = csakura;
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          nativeBuildInputs = [ pkgs.pkg-config ];
          buildInputs = [ pkgs.ncurses ];
        };
      });
    };
}
