{
  description = "";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = nixpkgs.lib;

	elfio-for-naxriscv = pkgs.stdenv.mkDerivation {
          name = "elfio";

          src = pkgs.fetchFromGitHub {
            owner = "serge1";
            repo = "ELFIO";
            rev = "d251da09a07dff40af0b63b8f6c8ae71d2d1938d";
            sha256 = "sha256-0uqWs+W6ePBgol3IKLr5GMx7b6Fm3SsV3KuRMTZCjaA=";
          };

          nativeBuildInputs = [ pkgs.cmake ];

          buildPhase = ''
            cmake . -DCMAKE_INSTALL_PREFIX=$out
            make
          '';

          installPhase = ''
            make install
          '';
        };

      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs;
            [
	      conda
	      gcc
	      # glibc
	      # glibc.dev
	      # glibc.static
	      stdenv.cc
	      clang-analyzer
	      scons

	      zig
	      llvm

	      linux-manual

	      ## Stuff for cpus
	      jdk11
              (sbt.override { jre = pkgs.jdk11; })
	      verilator
	      SDL2

	      ## OpenODC Risc-v
	      libtool
	      automake
	      autoconf
	      libusb1
	      texinfo
	      libyaml
	      pkg-config

	      ## NaxRiscv
	      elfio-for-naxriscv
	      dtc
	      boost

	      ## Briey
		gcc
		xorg.libX11
		xorg.libXrandr
		xorg.libXinerama
		xorg.libXcursor
		libudev-zero
		mesa
		libGL
		libGLU
		alsa-lib
		libpulseaudio
		openal
		libogg
		libvorbis
		audiofile
		libpng
		freetype
		libusb1
		dbus
		zlib
		directfb
		SDL2
            ];
        };
      }
    );
}
