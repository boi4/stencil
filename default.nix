with import <nixpkgs>  {};

stdenv.mkDerivation {
  name = "stencil-serial";
  buildInputs = [ glibc gcc9 ];
}
