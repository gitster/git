#include "builtin.h"
#include "json-writer.h"
#include "parse-options.h"

enum output_format {
	FORMAT_JSON,
	FORMAT_PLAINTEXT
};

struct repo_info {
	struct repository *repo;
	enum output_format format;
};

static void repo_info_init(struct repo_info *repo_info,
			   struct repository *repo,
			   char *format)
{
	repo_info->repo = repo;

	if (format == NULL || !strcmp(format, "json"))
		repo_info->format = FORMAT_JSON;
	else if (!strcmp(format, "plaintext"))
		repo_info->format = FORMAT_PLAINTEXT;
	else
		die("invalid format %s", format);
}

static void repo_info_print_plaintext(struct repo_info *repo_info UNUSED)
{
}

static void repo_info_print_json(struct repo_info *repo_info UNUSED)
{
	struct json_writer jw;

	jw_init(&jw);

	jw_object_begin(&jw, 1);
	jw_end(&jw);

	puts(jw.json.buf);
	jw_release(&jw);
}

static void repo_info_print(struct repo_info *repo_info)
{
	enum output_format format = repo_info->format;

	switch (format) {
	case FORMAT_JSON:
		repo_info_print_json(repo_info);
		break;
	case FORMAT_PLAINTEXT:
		repo_info_print_plaintext(repo_info);
		break;
	}
}

int cmd_repo_info(int argc,
		  const char **argv,
		  const char *prefix,
		  struct repository *repo)
{
	const char *const repo_info_usage[] = {
		"git repo-info",
		NULL
	};
	struct repo_info repo_info;
	char *format = NULL;
	int allow_empty = 0;
	struct option options[] = {
		OPT_STRING(0, "format", &format, N_("format"),
			   N_("output format")),
		OPT_BOOL(0, "allow-empty", &allow_empty,
			 "when set, it will use an empty set of fields if no field is requested"),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, repo_info_usage,
			     PARSE_OPT_KEEP_UNKNOWN_OPT);
	repo_info_init(&repo_info, repo, format);
	repo_info_print(&repo_info);

	return 0;
}
