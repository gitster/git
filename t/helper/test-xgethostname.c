#include "test-tool.h"

int cmd__xgethostname(int argc, const char **argv)
{
	char hostname[HOST_NAME_MAX + 1];

	if (xgethostname(hostname, sizeof(hostname)))
		die("unable to get the host name");

	puts(hostname);
	return 0;
}
