#include "unit-test.h"

int cmd_main(int argc, const char **argv)
{
	const char **argv_copy;
	int ret;

	/* Append the "-t" flag such that the tests generate TAP output. */
	ALLOC_ARRAY(argv_copy, argc + 1);
	COPY_ARRAY(argv_copy, argv, argc);
	argv_copy[argc++] = "-t";

	ret = clar_test(argc, (char **) argv_copy);

	free(argv_copy);
	return ret;
}
