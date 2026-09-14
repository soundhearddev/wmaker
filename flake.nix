{
  description = "Custom wmaker (smart-tile fork) as a NixOS package";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      packages.${system} = {
        wmaker-tile = pkgs.stdenv.mkDerivation {
          pname = "wmaker-tile";
          version = "custom";

          # Nutzt cleanSource, um lokale Build-Artefakte zu filtern
          src = pkgs.lib.cleanSource ./.;

          nativeBuildInputs = with pkgs; [
            autoreconfHook
            pkg-config
            gettext
            libtool
            automake
            autoconf
          ];

          buildInputs = with pkgs; [
            libX11
            libXext
            libXinerama
            libXrandr
            libXmu
            libXpm
            libXft
            libSM
            libICE
            libXaw
            fontconfig
            freetype
            giflib
            libjpeg
            libpng
            libtiff
            libwebp
            libxml2
          ];

          configureFlags = [
            "--prefix=${placeholder "out"}"
            "--enable-shared"
            "--disable-static"
          ];

          # Baut erst wrlib explizit, um den Linker-Fehler zu vermeiden
          buildPhase = ''
            runHook preBuild
            make -C wrlib
            make
            runHook postBuild
          '';

          enableParallelBuilding = false;
        };

        default = self.packages.${system}.wmaker-tile;
      };
    };
}
