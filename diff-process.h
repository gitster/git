#ifndef DIFF_PROCESS_H
#define DIFF_PROCESS_H

#include "xdiff/xdiff.h"

struct diff_options;
struct object_id;

enum diff_process_result {
	DIFF_PROCESS_ERROR = -1, /* failed; caller falls back to builtin */
	DIFF_PROCESS_OK = 0,     /* hunks populated in xpp */
	DIFF_PROCESS_SKIP,       /* process did not apply: use builtin */
	DIFF_PROCESS_EQUIVALENT, /* tool says files are equivalent */
};

/*
 * Consult the diff process configured for 'path' and populate
 * xpp->external_hunks with the returned hunks.
 *
 * Handles driver lookup, flag checks (--no-ext-diff,
 * --diff-algorithm), subprocess management, and error reporting.
 *
 * Returns DIFF_PROCESS_OK when hunks are populated in xpp.
 * The caller owns xpp->external_hunks and must free() it.
 *
 * Returns DIFF_PROCESS_EQUIVALENT when the tool returns no hunks and
 * the blobs are not a trailing-newline-only change (files are
 * considered identical); caller should skip diff/blame.
 * Returns DIFF_PROCESS_SKIP when no process applies; caller
 * should use the builtin diff algorithm.
 * Returns DIFF_PROCESS_ERROR on tool failure (already warned);
 * caller should fall back to the builtin diff algorithm.
 *
 * oid_a/oid_b, when non-NULL, are sent to the tool as old-oid/new-oid
 * so it can key a cache on the blob pair.  Pass NULL for a side whose
 * content is not the raw blob (e.g. textconv'd) or whose object name is
 * unknown, so any oid that is sent always names the bytes the tool
 * receives.
 */
enum diff_process_result diff_process_fill_hunks(
		struct diff_options *diffopt,
		const char *path,
		const mmfile_t *file_a,
		const mmfile_t *file_b,
		const struct object_id *oid_a,
		const struct object_id *oid_b,
		xpparam_t *xpp);

/*
 * Process-aware xdi_diff(): consult the diff process for 'path', then
 * run xdiff either constrained to the tool's hunks or computing the
 * diff itself when the process does not apply or fails.  Frees any
 * hunks it obtained before returning.
 *
 * Returns DIFF_PROCESS_EQUIVALENT (without running xdiff) when the tool
 * reports the blobs equal, so the caller can drop or skip the change;
 * DIFF_PROCESS_OK when xdiff ran (on tool hunks or builtin); and
 * DIFF_PROCESS_ERROR if xdiff itself errored.
 *
 * The caller fills xpp (flags, ignore_regex, anchors) and xecfg/ecb as
 * for a direct xdi_diff() call.  oid_a/oid_b are forwarded to
 * diff_process_fill_hunks() (see there).
 */
enum diff_process_result xdi_diff_process(
		struct diff_options *diffopt,
		const char *path,
		mmfile_t *file_a,
		mmfile_t *file_b,
		const struct object_id *oid_a,
		const struct object_id *oid_b,
		xpparam_t *xpp,
		xdemitconf_t *xecfg,
		xdemitcb_t *ecb);

#endif /* DIFF_PROCESS_H */
