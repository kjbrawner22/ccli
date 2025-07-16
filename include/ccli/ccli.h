#pragma once

#include <argp.h>
#include <stdbool.h>

/**************************************************

Structures and Types
These structures define the main components of the CCLI library, including commands, arguments, options, and values.

**************************************************/

/**
 * @brief Main CCLI interface structure
 *
 */
typedef struct Ccli Ccli;

/**
 * @brief Represents a command line argument
 *
 */
typedef struct CcliArg CcliArg;

/**
 * @brief Represents a command line option
 *
 */
typedef struct CcliOption CcliOption;

/**************************************************

Main CCLI Interface API

**************************************************/

/**
 * @brief Create, initialize, and return a new CCLI interface instance
 *
 * @param appName
 * @param argc
 * @param argv
 * @return Ccli*
 */
Ccli* ccli_new(const char* appName, int argc, char** argv);

/**
 * @brief Free the CCLI interface and all associated resources
 *
 * @param iface
 */
void ccli_free(Ccli* iface);

/**
 * @brief Run the CCLI interface, processing commands and options
 *
 * @param iface
 */
void ccli_run(Ccli* iface);

/**************************************************

Command Management API

**************************************************/

/**
 * @brief Represents a command in the CCLI
 *
 */
typedef struct CcliCommand CcliCommand;

/**
 * @brief Create a new command object
 *
 * @return CcliCommand*
 */
CcliCommand* ccli_new_command();

/**
 * @brief Set description for a command
 *
 * @param command
 * @param description
 */
void ccli_command_set_description(CcliCommand* command, const char* description);

/**
 * @brief
 *
 * @param command
 * @param optionName
 */
void ccli_command_new_bool_option(
  CcliCommand* command,
  const char* longOption,
  const char* shortOption
);

/**************************************************

Printing Utilities
These functions are used to print messages to the console, with optional color formatting.

**************************************************/

typedef enum {
  COLOR_DEFAULT,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_YELLOW,
  COLOR_BLUE,
  COLOR_MAGENTA,
  COLOR_CYAN,
} CcliColor;

void ccli_echo(Ccli* interface, const char* format, ...);

/**
 * @brief Echo a formatted string to the output stream in color (newline appended)
 *
 * @param interface
 * @param color
 * @param format
 * @param ...
 */
__attribute__((format(printf, 3, 4))) void ccli_echo_color(
  Ccli* interface,
  CcliColor color,
  const char* format,
  ...
);