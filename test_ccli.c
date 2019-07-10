#include "ccli.h"

void hello_callback(ccli *interface, ccli_table *options) {
  ccli_echo_color(interface, COLOR_GREEN, "Hello!");
  int number;
  bool boolean;
  char *string;
  if (ccli_table_get_int(options, "--number", &number)) {
    ccli_echo_color(interface, COLOR_YELLOW, "number: %d", number);
  }
  if (ccli_table_get_bool(options, "--bool", &boolean)) {
    ccli_echo_color(interface, COLOR_BLUE, "bool: %s", boolean ? "true" : "false");
  }
  if (ccli_table_exists(options, "--flag")) {
    ccli_echo_color(interface, COLOR_CYAN, "flag exists");
  }
  if (ccli_table_get_string(options, "--string", &string)) {
    ccli_echo(interface, "string: %s", string);
  }

  ccli_echo(interface, "test_arg: %s", ccli_get_bool_arg(interface, 0) ? "true" : "false");
}

void hello_command(ccli *interface) {
  ccli_command *hello = ccli_add_command(interface, "hello", hello_callback);
  ccli_command_set_description(hello, "Say hello, and use some random options!");

  ccli_option *number = ccli_command_add_option(hello, "--number", NULL, VAL_NUM);
  ccli_option_set_default_number(number, 3);
  ccli_command_add_option(hello, "--string", NULL, VAL_STRING);
  ccli_command_add_option(hello, "--bool", NULL, VAL_BOOL);
  ccli_command_add_option(hello, "--flag", NULL, VAL_NULL);

  ccli_command_add_bool_arg(hello, "test_arg");
}

int main(int argc, char **argv) {
  ccli *interface = ccli_init("test_ccli", argc, argv);
  ccli_set_description(interface, "Some description for a command line interface.");

  hello_command(interface);

  ccli_run(interface);

  ccli_free(interface);

  return 0;
}
