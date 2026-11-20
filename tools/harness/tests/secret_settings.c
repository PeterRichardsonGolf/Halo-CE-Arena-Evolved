#include "harness.h"
#include <ctype.h>
/* the server's two views of a secret setting agree: server_commands.c's server_commands_setting_secret (the settings'
answers show "(set)") and server_console.c's server_console_line_is_secret (the console log hides the command line
that sets it). Prints, for each setting name given: the name, whether the settings side calls it secret, whether
`sv_set <name> <value>` is hidden in the log */

#include "under_test.inc"

int main(int argc, char **argv)
{
	int index;
	char line[256];

	for (index = 1; index < argc; index++)
	{
		snprintf(line, sizeof(line), "sv_set %s https://example.invalid/hook?token=abc", argv[index]);
		printf("%s %d %d\n", argv[index], server_commands_setting_secret(argv[index]) ? 1 : 0,
			server_console_line_is_secret(line) ? 1 : 0);
	}
	return 0;
}
