#include "git-compat-util.h"
#include "index-info.h"
#include "hash.h"
#include "hex.h"
#include "strbuf.h"
#include "quote.h"

int read_index_info(int nul_term_line, each_index_info_fn fn, void *cbdata)
{
	const int hexsz = the_hash_algo->hexsz;
	struct strbuf buf = STRBUF_INIT;
	struct strbuf uq = STRBUF_INIT;
	strbuf_getline_fn getline_fn;
	int ret = 0;

	getline_fn = nul_term_line ? strbuf_getline_nul : strbuf_getline_lf;
	while (getline_fn(&buf, stdin) != EOF) {
		char *ptr, *tab;
		char *path_name;
		struct object_id oid;
		enum object_type obj_type = OBJ_NONE;
		unsigned int mode;
		unsigned long ul;
		int stage;

		if (!buf.len) {
			ret = INDEX_INFO_EMPTY_LINE;
			break;
		}

		/* This reads lines formatted in one of three formats:
		 *
		 * (1) mode         SP sha1          TAB path
		 * The first format is what "git apply --index-info"
		 * reports, and used to reconstruct a partial tree
		 * that is used for phony merge base tree when falling
		 * back on 3-way merge.
		 *
		 * (2) mode SP type SP sha1          TAB path
		 * The second format is to stuff "git ls-tree" output
		 * into the index file.
		 *
		 * (3) mode         SP sha1 SP stage TAB path
		 * This format is to put higher order stages into the
		 * index file and matches "git ls-files --stage" output.
		 */
		errno = 0;
		ul = strtoul(buf.buf, &ptr, 8);
		if (ptr == buf.buf || *ptr != ' '
		    || errno || (unsigned int) ul != ul)
			goto bad_line;
		mode = ul;

		tab = strchr(ptr, '\t');
		if (!tab || tab - ptr < hexsz + 1)
			goto bad_line;

		if (tab[-2] == ' ' && '0' <= tab[-1] && tab[-1] <= '3') {
			stage = tab[-1] - '0';
			path_name = tab + 1; /* point at the head of path */
			tab = tab - 2; /* point at tail of sha1 */
		} else {
			stage = 0;
			path_name = tab + 1; /* point at the head of path */
		}

		if (get_oid_hex(tab - hexsz, &oid) ||
			tab[-(hexsz + 1)] != ' ')
			goto bad_line;

		if (!nul_term_line && path_name[0] == '"') {
			strbuf_reset(&uq);
			if (unquote_c_style(&uq, path_name, NULL)) {
				ret = error("bad quoting of path name");
				break;
			}
			path_name = uq.buf;
		}

		/* Get the type, if provided */
		if (tab - hexsz - 1 > ptr + 1) {
			if (*(tab - hexsz - 1) != ' ')
				goto bad_line;
			*(tab - hexsz - 1) = '\0';
			obj_type = type_from_string(ptr + 1);
		}

		ret = fn(mode, &oid, obj_type, stage, path_name, cbdata);
		if (ret) {
			ret = -1;
			break;
		}

		continue;

	bad_line:
		ret = error("malformed input line '%s'", buf.buf);
		break;
	}
	strbuf_release(&buf);
	strbuf_release(&uq);

	return ret;
}
