#include <ccli/ccli.h>

int main(int argc, char** argv)
{
  ccli* interface = ccli_init("test_ccli", argc, argv);
  // ccli_set_description(interface, "Some description for a command line interface.");

  // hello_command(interface);
  // goodbye_command(interface);

  ccli_run(interface);

  ccli_free(interface);

  return 0;
}
