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
      in
      {
        devShells.default = pkgs.mkShell {
      NIX_ENFORCE_NO_NATIVE = 0;
          packages = with pkgs;
            [
            python313
            python313Packages.matplotlib
            python313Packages.pygccxml
            castxml

	      # conda
	      # gcc15
	    # glibc
	    #   # glibc.dev
	    #   # glibc.static
	    #   stdenv.cc
	    #   clang-analyzer
	    #   scons
        # 
	    #   zig
	    #   llvm
        # 
	    #   linux-manual
        # 
        # 

            ];
        };
      }
    );
}
