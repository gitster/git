#include "builtin.h"
#include "parse-options.h"
#include "strbuf.h"
#include "refs.h"

typedef void add_field_fn(struct strbuf *buf, struct repository *repo);

struct field {
	const char *key;
	add_field_fn *add_field_callback;
};

static void add_string(struct strbuf *buf,
		       const char *key, const char *value)
{
	strbuf_addf(buf, "%s\n%s%c", key, value, '\0');
}

static void add_references_format(struct strbuf *buf,
				  struct repository *repo)
{
	add_string(buf, "references.format",
		   ref_storage_format_to_name(repo->ref_storage_format));
}

// repo_info_fields keys should be in lexicographical order
static const struct field repo_info_fields[] = {
	{"references.format", add_references_format},
};

static int repo_info_fields_cmp(const void *va, const void *vb)
{
	const struct field *a = va;
	const struct field *b = vb;

	return strcmp(a->key, b->key);
}

static add_field_fn *get_append_callback(const char *key) {
	const struct field search_key = {key, NULL};
	const struct field *found = bsearch(&search_key, repo_info_fields,
					    ARRAY_SIZE(repo_info_fields),
					    sizeof(struct field),
					    repo_info_fields_cmp);
	return found ? found->add_field_callback : NULL;
}

static int qsort_strcmp(const void *va, const void *vb)
{
	const char *a = *(const char **)va;
	const char *b = *(const char **)vb;

	return strcmp(a, b);
}

static void print_fields(int argc, const char **argv, struct repository *repo) {
	const char *last = "";
	struct strbuf buf;
	strbuf_init(&buf, 256);

	QSORT(argv, argc, qsort_strcmp);

	for (int i = 0; i < argc; i++) {
		add_field_fn *callback;
		const char *key = argv[i];

		if (!strcmp(key, last))
			continue;

		callback = get_append_callback(key);

		if (!callback) {
			error("key %s not found", key);
			strbuf_release(&buf);
			exit(1);
		}

		callback(&buf, repo);
		last = key;
	}

	fwrite(buf.buf, 1, buf.len, stdout);
	strbuf_release(&buf);
}

static int repo_info(int argc,
		     const char **argv,
		     const char *prefix UNUSED,
		     struct repository *repo)
{

	print_fields(argc - 1 , argv + 1, repo);
	return 0;
}

int cmd_repo(int argc,
	     const char **argv,
	     const char *prefix,
	     struct repository *repo)
{
	parse_opt_subcommand_fn *fn = NULL;
	const char *const repo_usage[] = {
		"git repo info [<key>...]",
		NULL
	};
	struct option options[] = {
		OPT_SUBCOMMAND("info", &fn, repo_info),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, repo_usage, 0);

	if (fn) {
		return fn(argc, argv, prefix, repo);
	} else {
		if (argc) {
			error(_("unknown subcommand: `%s'"), argv[0]);
			usage_with_options(repo_usage, options);
		}
		return 1;
	}
}
