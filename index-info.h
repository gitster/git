#ifndef INDEX_INFO_H
#define INDEX_INFO_H

#include "hash.h"
#include "object.h"

typedef int (*each_index_info_fn)(unsigned int, struct object_id *, enum object_type, int, const char *, void *);

#define INDEX_INFO_EMPTY_LINE 1

/* Iterate over parsed index info from stdin */
int read_index_info(int nul_term_line, each_index_info_fn fn, void *cbdata);

#endif /* INDEX_INFO_H */
