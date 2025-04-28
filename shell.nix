# Run with `nix-shell cuda-shell.nix`
{ pkgs ? import <nixpkgs> { } }:
pkgs.mkShell {
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
    soapysdr
    yaml-cpp
    pkg-config
    pothos
    gnuradio
    gqrx
    sdrangel
    sigdigger
    sdrpp

    (python3.withPackages (ps: with ps; [numpy scipy matplotlib soapysdr ipython ]))
  ];
  shellHook = ''
    export CUDA_PATH=${pkgs.cudatoolkit}
    export LD_LIBRARY_PATH=${pkgs.linuxPackages.nvidia_x11}/lib:${pkgs.ncurses5}/lib:../cuddc:../sdaa_data/target/release
    export EXTRA_LDFLAGS="-L/lib -L${pkgs.linuxPackages.nvidia_x11}/lib"
    export EXTRA_CCFLAGS="-I/usr/include"
    export SOAPY_SDR_PLUGIN_PATH=$PWD
  '';
}
