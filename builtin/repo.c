#include "builtin.h"
#include "parse-options.h"
#include "quote.h"
#include "refs.h"
#include "strbuf.h"

static const char *const repo_usage[] = {
	"git repo info [<key>...]",
	NULL
};

typedef int get_value_fn(struct repository *repo, struct strbuf *buf);

struct field {
	const char *key;
	get_value_fn *get_value;
};

static int get_references_format(struct repository *repo, struct strbuf *buf)
{
	strbuf_addstr(buf,
		      ref_storage_format_to_name(repo->ref_storage_format));
	return 0;
}

/* repo_info_fields keys should be in lexicographical order */
static const struct field repo_info_fields[] = {
	{ "references.format", get_references_format },
};

static int repo_info_fields_cmp(const void *va, const void *vb)
{
	const struct field *a = va;
	const struct field *b = vb;

	return strcmp(a->key, b->key);
}

static get_value_fn *get_value_fn_for_key(const char *key)
{
	const struct field search_key = { key, NULL };
	const struct field *found = bsearch(&search_key, repo_info_fields,
					    ARRAY_SIZE(repo_info_fields),
					    sizeof(*found),
					    repo_info_fields_cmp);
	return found ? found->get_value : NULL;
}

static int qsort_strcmp(const void *va, const void *vb)
{
	const char *a = *(const char **)va;
	const char *b = *(const char **)vb;

	return strcmp(a, b);
}

static int print_fields(int argc, const char **argv, struct repository *repo)
{
	int ret = 0;
	const char *last = "";
	struct strbuf sb = STRBUF_INIT;

	QSORT(argv, argc, qsort_strcmp);

	for (int i = 0; i < argc; i++) {
		get_value_fn *get_value;
		const char *key = argv[i];
		char *value;

		if (!strcmp(key, last))
			continue;

		get_value = get_value_fn_for_key(key);

		if (!get_value) {
			ret = error(_("key '%s' not found"), key);
			continue;
		}

		strbuf_reset(&sb);
		get_value(repo, &sb);

		value = strbuf_detach(&sb, NULL);
		quote_c_style(value, &sb, NULL, 0);
		free(value);

		printf("%s=%s\n", key, sb.buf);
		last = key;
	}

	strbuf_release(&sb);
	return ret;
}

static int repo_info(int argc, const char **argv, const char *prefix UNUSED,
		     struct repository *repo)
{
	return print_fields(argc - 1, argv + 1, repo);
}

int cmd_repo(int argc, const char **argv, const char *prefix,
	     struct repository *repo)
{
	parse_opt_subcommand_fn *fn = NULL;
	struct option options[] = {
		OPT_SUBCOMMAND("info", &fn, repo_info),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, repo_usage, 0);

	return fn(argc, argv, prefix, repo);
}
