#include "art/Framework/Core/artapp.h"

#include <iostream>

int main( int argc, char* argv[] ) {
  int result = artapp(argc,argv);
  std::cout
    << "Art has completed and will exit with status "
    << result
    << ".\n";
  return result;
}