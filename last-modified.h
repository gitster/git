#ifndef LAST_MODIFIED_H
#define LAST_MODIFIED_H

#include "commit.h"
#include "hashmap.h"
#include "revision.h"

struct last_modified {
	struct hashmap paths;
	struct rev_info rev;
};

/*
 * Initialize the last-modified machinery using command line arguments.
 */
int last_modified_init(struct last_modified *lm,
		     struct repository *r,
		     const char *prefix,
		     int argc, const char **argv);

void last_modified_release(struct last_modified *);

typedef void (*last_modified_callback)(const char *path,
				    const struct commit *commit,
				    void *data);

/*
 * Run the last-modified traversal. For each path found the callback is called
 * passing the path, the commit, and the cbdata.
 */
int last_modified_run(struct last_modified *lm,
		   last_modified_callback cb,
		   void *cbdata);

#endif /* LAST_MODIFIED_H */
