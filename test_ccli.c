#include <string.h>

#include "ccli.h"

void hello(ccli *interface, ccli_table *options) {
  ccli_echo_color(interface, COLOR_GREEN, "Hello!");
  int number;
  if (ccli_table_get_int(options, "--number", &number)) {
    ccli_echo_color(interface, COLOR_YELLOW, "number: %d", number);
  }
}

void hello_setup(ccli *interface) {
  ccli_command *_hello = ccli_add_command(interface, "hello", hello);
  ccli_option *_number = ccli_command_add_option(_hello, "--number", NULL, VAL_NUM);
}

int main(int argc, char **argv) {
  ccli *interface = ccli_init("test_ccli", argc, argv);
  ccli_set_description(interface, "Some description for a test ccli.");

  hello_setup(interface);

  ccli_run(interface);

  ccli_free(interface);

  return 0;
}