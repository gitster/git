/*
 * Diff process backend: communicates with a long-running external
 * tool via the pkt-line protocol to obtain custom line-matching
 * results.  The tool controls which lines are marked as changed
 * while the display shows the file content (after any textconv
 * transformation, if configured).
 *
 * Protocol: pkt-line over stdin/stdout, following the pattern of
 * the long-running filter process protocol (see convert.c).
 *
 * Handshake:
 *   git> git-diff-client / version=1 / flush
 *   tool< git-diff-server / version=1 / flush
 *   git> capability=hunks / flush
 *   tool< capability=hunks / flush
 *
 * Per-file:
 *   git> command=hunks / pathname=<path> / [old-oid=<hex>] / [new-oid=<hex>] / flush
 *   git> <old content packetized> / flush
 *   git> <new content packetized> / flush
 *   tool< hunk <old_start> <old_count> <new_start> <new_count>
 *   tool< ... / flush
 *   tool< status=success / flush
 *
 * When the tool returns no hunks with status=success, it considers
 * the files equivalent.  Git will skip the diff for that file.
 */

#include "git-compat-util.h"
#include "diff-process.h"
#include "diff.h"
#include "gettext.h"
#include "hex.h"
#include "repository.h"
#include "sigchain.h"
#include "userdiff.h"
#include "sub-process.h"
#include "pkt-line.h"
#include "strbuf.h"
#include "xdiff/xdiff.h"

#define CAP_HUNKS (1u << 0)

struct diff_subprocess {
	struct subprocess_entry subprocess;
	unsigned int supported_capabilities;
};

static int start_diff_process_fn(struct subprocess_entry *subprocess)
{
	static int versions[] = { 1, 0 };
	static struct subprocess_capability capabilities[] = {
		{ "hunks", CAP_HUNKS },
		{ NULL, 0 }
	};
	struct diff_subprocess *entry =
		container_of(subprocess, struct diff_subprocess, subprocess);

	return subprocess_handshake(subprocess, "git-diff",
				    versions, NULL,
				    capabilities,
				    &entry->supported_capabilities);
}

static struct diff_subprocess *get_or_launch_process(
		struct userdiff_driver *drv)
{
	struct diff_subprocess *entry;

	if (drv->diff_subprocess)
		return drv->diff_subprocess;

	entry = xcalloc(1, sizeof(*entry));
	if (subprocess_start_command(&entry->subprocess, drv->process,
				     start_diff_process_fn)) {
		free(entry);
		drv->diff_process_failed = 1;
		return NULL;
	}

	drv->diff_subprocess = entry;
	return entry;
}

static int send_file_content(int fd, const char *buf, long size)
{
	int ret = 0;

	if (size < 0)
		return -1;
	if (size > 0)
		ret = write_packetized_from_buf_no_flush(buf, size, fd);
	if (ret)
		return ret;
	return packet_flush_gently(fd);
}

/*
 * A hunk in the diff process's presentation coordinates: the line
 * numbering it reports over the protocol.  Kept distinct from struct
 * xdl_hunk (xdiff's coordinates) so that only translated hunks ever
 * reach the diff algorithm; diff_process_hunk_to_xdl() is the single
 * crossing point.
 */
struct diff_process_hunk {
	long old_start, old_count;
	long new_start, new_count;
};

/*
 * Parse one non-negative decimal field of a hunk line into *out and
 * advance *line past it.  Fields must be plain decimal with no leading
 * whitespace or sign (isdigit() takes an unsigned char to stay defined
 * for high-bit bytes).  The first three fields are followed by a single
 * space; the last (is_last) is followed by end-of-string or a space.
 * Trailing space-separated tokens after the last field are allowed and
 * ignored, so a future protocol version can append fields (e.g. a
 * "moved" marker) without older tools rejecting the line -- mirroring
 * the request-side rule that tools ignore unknown keys.
 */
static int parse_hunk_field(const char **line, long *out, int is_last)
{
	const char *p = *line;
	char *end;

	if (!isdigit((unsigned char)*p))
		return -1;
	errno = 0;
	*out = strtol(p, &end, 10);
	if (errno || end == p)
		return -1;
	if (is_last) {
		if (*end != '\0' && *end != ' ')
			return -1;
	} else {
		if (*end != ' ')
			return -1;
		end++;
	}
	*line = end;
	return 0;
}

static int parse_hunk_line(const char *line,
			   struct diff_process_hunk *presented)
{
	/* Format: "hunk <old_start> <old_count> <new_start> <new_count>" */
	if (!skip_prefix(line, "hunk ", &line))
		return -1;
	if (parse_hunk_field(&line, &presented->old_start, 0) ||
	    parse_hunk_field(&line, &presented->old_count, 0) ||
	    parse_hunk_field(&line, &presented->new_start, 0) ||
	    parse_hunk_field(&line, &presented->new_count, 1))
		return -1;
	return 0;
}

/*
 * Translate a hunk from the diff process's presentation coordinates
 * into xdiff's.
 *
 * Protocol starts are already 1-based positions (the line a change
 * sits before), the same numbering xdiff uses, so the only adjustment
 * is for an empty file side: "git diff" addresses it with a start of 0
 * and a count of 0 (e.g. "0 0 1 5" adds five lines to an empty old
 * side), and since xdiff uses start-1 as an array index that 0 becomes
 * 1 here.  This is NOT the full inverse of xdl_emit_hunk_hdr()
 * (xdiff/xutils.c): that emitter shifts a count-0 range to start-1 for
 * the displayed "@@" header, but the protocol keeps the unshifted
 * 1-based position for a mid-file insert or delete.  This is the single
 * point where presentation coordinates become xdiff coordinates, so
 * xdl_populate_hunks_from_external() may assume 1-based starts.
 *
 * Returns -1 for a start of 0 paired with a nonzero count, which names
 * no line in either coordinate system.  (parse_hunk_line() already
 * guarantees non-negative starts and counts.)
 */
static int diff_process_hunk_to_xdl(const struct diff_process_hunk *presented,
				    struct xdl_hunk *xdl)
{
	long old_start = presented->old_start;
	long new_start = presented->new_start;

	if ((!old_start && presented->old_count) ||
	    (!new_start && presented->new_count))
		return -1;
	if (!old_start)
		old_start = 1;
	if (!new_start)
		new_start = 1;

	xdl->old_start = old_start;
	xdl->old_count = presented->old_count;
	xdl->new_start = new_start;
	xdl->new_count = presented->new_count;
	return 0;
}

static enum diff_process_result get_hunks(
		struct userdiff_driver *drv,
		const char *path,
		const char *old_buf, long old_size,
		const char *new_buf, long new_size,
		const struct object_id *oid_a,
		const struct object_id *oid_b,
		struct xdl_hunk **hunks_out,
		size_t *nr_hunks_out)
{
	struct diff_subprocess *backend;
	struct child_process *process;
	int fd_in, fd_out;
	struct strbuf status = STRBUF_INIT;
	struct xdl_hunk *hunks = NULL;
	struct diff_process_hunk presented;
	struct xdl_hunk hunk;
	size_t nr_hunks = 0, alloc_hunks = 0;
	size_t max_hunks;
	int len;
	char *line;

	backend = get_or_launch_process(drv);
	if (!backend)
		return DIFF_PROCESS_ERROR;

	if (!(backend->supported_capabilities & CAP_HUNKS))
		return DIFF_PROCESS_SKIP;

	process = subprocess_get_child_process(&backend->subprocess);
	fd_in = process->in;
	fd_out = process->out;

	sigchain_push(SIGPIPE, SIG_IGN);

	/* Send request */
	if (packet_write_fmt_gently(fd_in, "command=hunks\n") ||
	    packet_write_fmt_gently(fd_in, "pathname=%s\n", path))
		goto comm_error;
	/*
	 * old-oid/new-oid let the tool key a cache on the blob pair.  A
	 * side is sent only when its content is the raw blob (the caller
	 * passes NULL otherwise, e.g. for textconv'd content), so an oid
	 * that is present always names the bytes the tool receives.
	 */
	if (oid_a &&
	    packet_write_fmt_gently(fd_in, "old-oid=%s\n", oid_to_hex(oid_a)))
		goto comm_error;
	if (oid_b &&
	    packet_write_fmt_gently(fd_in, "new-oid=%s\n", oid_to_hex(oid_b)))
		goto comm_error;
	if (packet_flush_gently(fd_in))
		goto comm_error;

	/* Send old file content */
	if (send_file_content(fd_in, old_buf, old_size))
		goto comm_error;

	/* Send new file content */
	if (send_file_content(fd_in, new_buf, new_size))
		goto comm_error;

	/*
	 * Hunks are non-overlapping and each useful hunk covers at least
	 * one line, so a valid response cannot contain more hunks than the
	 * two files have lines, which is bounded by their byte sizes.  Cap
	 * the accumulation accordingly so a misbehaving tool that floods
	 * hunk lines cannot drive unbounded memory growth before validation.
	 */
	max_hunks = (size_t)old_size + (size_t)new_size + 1;

	/* Read hunks until flush packet */
	while ((len = packet_read_line_gently(fd_out, NULL, &line)) >= 0 &&
	       line) {
		if (parse_hunk_line(line, &presented) < 0)
			goto comm_error;
		if (diff_process_hunk_to_xdl(&presented, &hunk) < 0)
			goto comm_error;
		if (nr_hunks >= max_hunks) {
			warning(_("diff process '%s' sent too many hunks"
				  " for '%s'"), drv->process, path);
			goto comm_error;
		}
		ALLOC_GROW(hunks, nr_hunks + 1, alloc_hunks);
		hunks[nr_hunks++] = hunk;
	}
	if (len < 0)
		goto comm_error;

	/* Read status */
	if (subprocess_read_status(fd_out, &status))
		goto comm_error;

	if (!strcmp(status.buf, "success")) {
		*hunks_out = hunks;
		*nr_hunks_out = nr_hunks;
		strbuf_release(&status);
		sigchain_pop(SIGPIPE);
		return DIFF_PROCESS_OK;
	}

	if (!strcmp(status.buf, "abort")) {
		/*
		 * The tool voluntarily withdrew: stop sending requests
		 * but do not warn (this is not a failure).
		 */
		backend->supported_capabilities &= ~CAP_HUNKS;
		free(hunks);
		strbuf_release(&status);
		sigchain_pop(SIGPIPE);
		return DIFF_PROCESS_SKIP;
	}

	/* status=error or unknown status */
	free(hunks);
	strbuf_release(&status);
	sigchain_pop(SIGPIPE);
	return DIFF_PROCESS_ERROR;

comm_error:
	/*
	 * Communication failure (broken pipe, malformed response).
	 * Tear down the process and mark as failed so we do not
	 * retry on every subsequent file.
	 */
	drv->diff_process_failed = 1;
	drv->diff_subprocess = NULL;
	subprocess_stop_command(&backend->subprocess);
	free(backend);
	free(hunks);
	strbuf_release(&status);
	sigchain_pop(SIGPIPE);
	return DIFF_PROCESS_ERROR;
}

/*
 * Whether exactly one of the two blobs ends in a newline.  A change
 * that only adds or removes the trailing newline is not expressible as
 * line hunks, so a tool comparing lines reports the files as equal.
 */
static int eof_newline_differs(const mmfile_t *a, const mmfile_t *b)
{
	int a_nl = a->size > 0 && a->ptr[a->size - 1] == '\n';
	int b_nl = b->size > 0 && b->ptr[b->size - 1] == '\n';
	return a_nl != b_nl;
}

/*
 * Number of lines in a blob, matching xdiff's record count: one per
 * newline, plus one more if the last line has no trailing newline.
 */
static long count_lines(const char *buf, long size)
{
	long lines = 0, i;

	for (i = 0; i < size; i++)
		if (buf[i] == '\n')
			lines++;
	if (size > 0 && buf[size - 1] != '\n')
		lines++;
	return lines;
}

/*
 * Validate the tool's hunks (already in xdiff coordinates) against the
 * two blobs before they bypass the diff algorithm.  Each hunk must fit
 * within its file, the hunks must be ordered and non-overlapping, and
 * the unchanged run before each hunk (and after the last) must be the
 * same length on both sides -- xdl_build_script() walks the two files
 * in lockstep over unchanged lines, so a mismatched gap desynchronizes
 * it and yields a corrupt diff even when the totals balance.  This is
 * the git layer's job so xdiff stays diagnostic-free; on a bad response
 * we warn and the caller falls back to the builtin diff.  Returns 0 if
 * valid, -1 (after warning) otherwise.
 */
static int validate_external_hunks(const struct xdl_hunk *hunks, size_t nr,
				   long old_lines, long new_lines,
				   const char *process, const char *path)
{
	size_t i;
	long prev_old_end = 0, prev_new_end = 0;

	for (i = 0; i < nr; i++) {
		const struct xdl_hunk *h = &hunks[i];

		if (h->old_count > old_lines - h->old_start + 1 ||
		    h->new_count > new_lines - h->new_start + 1) {
			warning(_("diff process '%s' returned a hunk past the "
				  "end of '%s'; using the builtin diff"),
				process, path);
			return -1;
		}
		if (h->old_start < prev_old_end || h->new_start < prev_new_end) {
			warning(_("diff process '%s' returned overlapping hunks "
				  "for '%s'; using the builtin diff"),
				process, path);
			return -1;
		}
		if (h->old_start - prev_old_end != h->new_start - prev_new_end) {
			warning(_("diff process '%s' returned hunks that leave "
				  "'%s' misaligned; using the builtin diff"),
				process, path);
			return -1;
		}
		prev_old_end = h->old_start + h->old_count;
		prev_new_end = h->new_start + h->new_count;
	}
	if (old_lines - prev_old_end != new_lines - prev_new_end) {
		warning(_("diff process '%s' returned hunks that leave '%s' "
			  "misaligned; using the builtin diff"),
			process, path);
		return -1;
	}
	return 0;
}

enum diff_process_result diff_process_fill_hunks(
		struct diff_options *diffopt,
		const char *path,
		const mmfile_t *file_a,
		const mmfile_t *file_b,
		const struct object_id *oid_a,
		const struct object_id *oid_b,
		xpparam_t *xpp)
{
	struct userdiff_driver *drv;
	struct xdl_hunk *ext_hunks = NULL;
	size_t nr = 0;
	enum diff_process_result res;

	if (!diffopt || !path)
		return DIFF_PROCESS_SKIP;
	if (diffopt->flags.no_diff_process || diffopt->ignore_driver_algorithm)
		return DIFF_PROCESS_SKIP;
	/*
	 * Whitespace-ignoring, regex-ignore (-I) and anchored options
	 * change which lines count as different, but the tool is never
	 * told about them, so its hunks could not honor them.  Rather
	 * than silently override the user's request, fall back to the
	 * builtin diff, which does honor these flags.  Key this off xpp
	 * (the parameters this diff actually runs with) rather than
	 * diffopt, so a caller like blame that keeps its flags outside
	 * diffopt is covered without a separate guard of its own.
	 */
	if ((xpp->flags & (XDF_WHITESPACE_FLAGS | XDF_IGNORE_BLANK_LINES)) ||
	    xpp->ignore_regex_nr || xpp->anchors_nr)
		return DIFF_PROCESS_SKIP;

	drv = userdiff_find_by_path(diffopt->repo->index, path);
	if (!drv || !drv->process)
		return DIFF_PROCESS_SKIP;
	if (drv->diff_process_failed)
		return DIFF_PROCESS_SKIP;

	res = get_hunks(drv, path,
			file_a->ptr, file_a->size,
			file_b->ptr, file_b->size,
			oid_a, oid_b,
			&ext_hunks, &nr);
	if (res == DIFF_PROCESS_OK) {
		if (!nr) {
			free(ext_hunks);
			/*
			 * Zero hunks means the tool considers the line
			 * content identical, but it cannot express a
			 * trailing-newline-only change.  When that is the
			 * actual difference, fall back to the builtin diff
			 * so the "\ No newline at end of file" marker is
			 * preserved instead of reporting the files equal.
			 */
			if (eof_newline_differs(file_a, file_b))
				return DIFF_PROCESS_SKIP;
			return DIFF_PROCESS_EQUIVALENT;
		}
		if (validate_external_hunks(ext_hunks, nr,
					    count_lines(file_a->ptr, file_a->size),
					    count_lines(file_b->ptr, file_b->size),
					    drv->process, path) < 0) {
			free(ext_hunks);
			return DIFF_PROCESS_SKIP;
		}
		xpp->external_hunks = ext_hunks;
		xpp->external_hunks_nr = nr;
		return DIFF_PROCESS_OK;
	}
	if (res == DIFF_PROCESS_ERROR) {
		warning(_("diff process '%s' failed for '%s',"
			  " falling back to builtin diff"),
			drv->process, path);
		return DIFF_PROCESS_ERROR;
	}
	return DIFF_PROCESS_SKIP;
}
