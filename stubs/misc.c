#include <assert.h>
#include <stdlib.h>

#ifndef NO_GETTEXT
/*
 * NEEDSWORK: This is enough to link our unit tests against
 * git-std-lib.a built with gettext support. We don't really support
 * programs other than git using git-std-lib.a with gettext support
 * yet. To do that we need to start using dgettext() rather than
 * gettext() in our code.
 */
#include "gettext.h"
int git_gettext_enabled = 0;
#endif

int common_exit(const char *file, int line, int code);

int common_exit(const char *file, int line, int code)
{
	exit(code);
}

#if !defined(__MINGW32__) && !defined(_MSC_VER)
int lstat_cache_aware_rmdir(const char *path);

int lstat_cache_aware_rmdir(const char *path)
{
	/*
	 * This function should not be called by programs linked
	 * against git-stub-lib.a
	 */
	assert(0);
}
#endif
