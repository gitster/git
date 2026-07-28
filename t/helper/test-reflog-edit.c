#define USE_THE_REPOSITORY_VARIABLE

#include "test-tool.h"
#include "hex.h"
#include "object-name.h"
#include "refs.h"
#include "repository.h"
#include "setup.h"
#include "strbuf.h"

int cmd__reflog_edit(int argc, const char **argv)
{
	struct reflog_edit *edits = NULL;
	size_t edits_nr = 0, edits_alloc = 0;
	struct strbuf sb = STRBUF_INIT;
	const char *refname;
	int ret;

	if (argc != 2)
		die("usage: test-tool reflog-edit <refname>");
	refname = argv[1];

	setup_git_directory(the_repository);

	while (strbuf_getline(&sb, stdin) != EOF) {
		struct reflog_edit edit;
		char *p = sb.buf;
		char *op_str, *idx_str;

		if (!*p || *p == '#')
			continue;

		memset(&edit, 0, sizeof(edit));

		op_str = p;
		p = strchr(p, ' ');
		if (p)
			*p++ = '\0';

		if (!strcmp(op_str, "replace"))
			edit.op = REFLOG_EDIT_REPLACE;
		else if (!strcmp(op_str, "delete"))
			edit.op = REFLOG_EDIT_DELETE;
		else if (!strcmp(op_str, "insert"))
			edit.op = REFLOG_EDIT_INSERT;
		else
			die("unknown operation '%s'", op_str);

		if (!p)
			die("missing index for operation '%s'", op_str);

		idx_str = p;
		p = strchr(p, ' ');
		if (p)
			*p++ = '\0';

		edit.idx = strtoul(idx_str, NULL, 10);

		while (p && *p) {
			char *key, *val;
			const char *arg;

			if (skip_prefix(p, "msg=", &arg)) {
				struct strbuf msg_sb = STRBUF_INIT;
				const char *m = arg;
				while (*m) {
					if (m[0] == '\\' && m[1] == 'n') {
						strbuf_addch(&msg_sb, '\n');
						m += 2;
					} else {
						strbuf_addch(&msg_sb, *m++);
					}
				}
				edit.data.msg = strbuf_detach(&msg_sb, NULL);
				break;
			}
			if (skip_prefix(p, "committer=", &arg)) {
				char *gt = strchr(arg, '>');
				if (!gt)
					die("missing '>' in committer '%s'", arg);
				edit.data.committer = xstrndup(arg, gt + 1 - arg);
				p = gt + 1;
				if (*p == ' ')
					p++;
				continue;
			}

			key = p;
			p = strchr(p, ' ');
			if (p)
				*p++ = '\0';

			val = strchr(key, '=');
			if (!val)
				die("invalid key-value pair '%s'", key);
			*val++ = '\0';

			if (!strcmp(key, "timestamp")) {
				edit.data.timestamp = parse_timestamp(val, NULL, 10);
			} else if (!strcmp(key, "tz")) {
				edit.data.tz = atoi(val);
			} else if (!strcmp(key, "old_oid")) {
				struct object_id oid;
				if (repo_get_oid(the_repository, val, &oid))
					die("cannot resolve old_oid '%s'", val);
				edit.data.old_oid = oid;
			} else if (!strcmp(key, "new_oid")) {
				struct object_id oid;
				if (repo_get_oid(the_repository, val, &oid))
					die("cannot resolve new_oid '%s'", val);
				edit.data.new_oid = oid;
			} else {
				die("unknown key '%s'", key);
			}
		}

		ALLOC_GROW(edits, edits_nr + 1, edits_alloc);
		edits[edits_nr++] = edit;
	}

	ret = refs_reflog_edit_in_bulk(get_main_ref_store(the_repository),
				       refname, edits_nr, edits);

	strbuf_release(&sb);
	for (size_t i = 0; i < edits_nr; i++) {
		free((char *)edits[i].data.msg);
		free((char *)edits[i].data.committer);
	}
	free(edits);

	return ret ? 1 : 0;
}
