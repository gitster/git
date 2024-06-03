#include "git-compat-util.h"
#include "missing.h"
#include "object-file.h"

int parse_missing_action_value(const char *value)
{
	if (!strcmp(value, "error"))
		return MA_ERROR;

	if (!strcmp(value, "allow-any"))
		return MA_ALLOW_ANY;

	if (!strcmp(value, "print"))
		return MA_PRINT;

	if (!strcmp(value, "allow-promisor"))
		return MA_ALLOW_PROMISOR;

	return -1;
}

int parse_missing_action_value_for_packing(const char *value)
{
	int res = parse_missing_action_value(value);

	if (res < 0)
		return -1;

	switch (res) {
	case MA_ERROR:
	case MA_ALLOW_ANY:
	case MA_ALLOW_PROMISOR:
		return res;
	default:
		return -2 - res;
	}
}

const char *missing_action_to_string(enum missing_action action)
{
	switch (action) {
	case MA_ERROR:
		return "error";
	case MA_ALLOW_ANY:
		return "allow-any";
	case MA_PRINT:
		return "print";
	case MA_ALLOW_PROMISOR:
		return "allow-promisor";
	default:
		BUG("invalid missing action %d", action);
	}
}
