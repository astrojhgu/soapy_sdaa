{
  description = "CUDA + SDR 开发环境，带有 unfree 包";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };

        gnuradio-custom = pkgs.gnuradio.override {
          extraPackages = with pkgs.gnuradioPackages; [
            osmosdr
          ];
          extraPythonPackages = with pkgs.gnuradio.python.pkgs; [
            numpy
            qtpy
          ];
        };

      in {
        devShells.default = pkgs.mkShell {
          name = "cuda-env-shell";

          buildInputs = with pkgs; [
            git
            gitRepo
            gnupg
            autoconf
            curl
            procps
            gnumake
            util-linux
            m4
            gperf
            unzip
            cudatoolkit
            linuxPackages.nvidia_x11
            libGLU
            libGL
            nvtopPackages.nvidia
            cudaPackages.cuda_cudart.all
            cudaPackages.libcufft.all
            xorg.libXi
            xorg.libXmu
            freeglut
            xorg.libXext
            xorg.libX11
            xorg.libXv
            xorg.libXrandr
            zlib
            ncurses5
            stdenv.cc
            binutils
            gdb
            boost.all
            soapysdr-with-plugins
            yaml-cpp
            pkg-config
            pothos
            gnuradio-custom
            gqrx
            sdrangel
            sigdigger
            sdrpp
            cubicsdr
            libsForQt5.qt5.qtwayland

            (python3.withPackages (ps: with ps; [
              numpy
              scipy
              matplotlib
              soapysdr
              ipython
              qtpy
            ]))
          ];

          shellHook = ''
            export CUDA_PATH=${pkgs.cudatoolkit}
            export LD_LIBRARY_PATH=${pkgs.linuxPackages.nvidia_x11}/lib:${pkgs.ncurses5}/lib:$PWD/../cuddc:$PWD/../sdaa_data/target/release
            export EXTRA_LDFLAGS="-L/lib -L${pkgs.linuxPackages.nvidia_x11}/lib"
            export EXTRA_CCFLAGS="-I/usr/include"
            export SOAPY_SDR_PLUGIN_PATH=$PWD
          '';
        };
      });
}
