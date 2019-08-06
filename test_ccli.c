#include "ccli.h"

int main(int argc, char **argv) {
  ccli *interface = newCCLI(argc, argv);
  setDescriptionCCLI(interface, "Simple CLI showcasing CCLI");
  
  runCCLI(interface);
  
  freeCCLI(interface);
  return 0;
}
