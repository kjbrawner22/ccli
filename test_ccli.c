#include <string.h>

#include "ccli.h"

void hello(ccli *interface) {
  ccli_echo_color(interface, COLOR_GREEN, "Hello!");
}

int main(int argc, char **argv) {
  ccli *interface = ccli_init("test_ccli", argc, argv);
  ccli_set_description(interface, "Some description for a test ccli.");
  
  ccli_add_command(interface, "hello", hello);

  ccli_run(interface);

  ccli_free(interface);

  return 0;
}