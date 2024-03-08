#include "git-compat-util.h"
#include "abspath.h"
#include "hex-ll.h"
#include "parse.h"
#include "strbuf.h"
#include "string-list.h"

/*
 * Calls all functions from git-std-lib
 * Some inline/trivial functions are skipped
 *
 * NEEDSWORK: The purpose of this file is to show that an executable can be
 * built with git-std-lib.a and git-stub-lib.a, and then executed. If there
 * is another executable that demonstrates this (for example, a unit test that
 * takes the form of an executable compiled with git-std-lib.a and git-stub-
 * lib.a), this file can be removed.
 */

static void abspath_funcs(void) {
	struct strbuf sb = STRBUF_INIT;

	fprintf(stderr, "calling abspath functions\n");
	is_directory("foo");
	strbuf_realpath(&sb, "foo", 0);
	strbuf_realpath_forgiving(&sb, "foo", 0);
	real_pathdup("foo", 0);
	absolute_path("foo");
	absolute_pathdup("foo");
	prefix_filename("foo/", "bar");
	prefix_filename_except_for_dash("foo/", "bar");
	is_absolute_path("foo");
	strbuf_add_absolute_path(&sb, "foo");
	strbuf_add_real_path(&sb, "foo");
}

static void hex_ll_funcs(void) {
	unsigned char c;

	fprintf(stderr, "calling hex-ll functions\n");

	hexval('c');
	hex2chr("A1");
	hex_to_bytes(&c, "A1", 1);
}

static void parse_funcs(void) {
	intmax_t foo;
	ssize_t foo1 = -1;
	unsigned long foo2;
	int foo3;
	int64_t foo4;

	fprintf(stderr, "calling parse functions\n");

	git_parse_signed("42", &foo, maximum_signed_value_of_type(int));
	git_parse_ssize_t("42", &foo1);
	git_parse_ulong("42", &foo2);
	git_parse_int("42", &foo3);
	git_parse_int64("42", &foo4);
	git_parse_maybe_bool("foo");
	git_parse_maybe_bool_text("foo");
	git_env_bool("foo", 1);
	git_env_ulong("foo", 1);
}

static int allow_unencoded_fn(char ch) {
	return 0;
}

static void strbuf_funcs(void) {
	struct strbuf *sb = xmalloc(sizeof(*sb));
	struct strbuf *sb2 = xmalloc(sizeof(*sb2));
	struct strbuf sb3 = STRBUF_INIT;
	struct string_list list = STRING_LIST_INIT_NODUP;
	int fd = open("/dev/null", O_RDONLY);

	fprintf(stderr, "calling strbuf functions\n");

	fprintf(stderr, "at line %d\n", __LINE__);
	starts_with("foo", "bar");
	fprintf(stderr, "at line %d\n", __LINE__);
	istarts_with("foo", "bar");
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_init(sb, 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_init(sb2, 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_release(sb);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_attach(sb, strbuf_detach(sb, NULL), 0, 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_swap(sb, sb2);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_setlen(sb, 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_trim(sb);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_trim_trailing_dir_sep(sb);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_trim_trailing_newline(sb);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_reencode(sb, "foo", "bar");
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_tolower(sb);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_add_separated_string_list(sb, " ", &list);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_list_free(strbuf_split_buf("foo bar", 8, ' ', -1));
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_cmp(sb, sb2);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_addch(sb, 1);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_splice(sb, 0, 1, "foo", 3);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_insert(sb, 0, "foo", 3);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_insertf(sb, 0, "%s", "foo");
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_remove(sb, 0, 1);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_add(sb, "foo", 3);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_addbuf(sb, sb2);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_join_argv(sb, 0, NULL, ' ');
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_addchars(sb, 1, 1);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_addstr(sb, "foo");
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_add_commented_lines(sb, "foo", 3, "#");
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_commented_addf(sb, "#", "%s", "foo");
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_addbuf_percentquote(sb, &sb3);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_add_percentencode(sb, "foo", STRBUF_ENCODE_SLASH);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_fread(sb, 0, stdin);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_read(sb, fd, 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_read_once(sb, fd, 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_write(sb, stderr);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_readlink(sb, "/dev/null", 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_getcwd(sb);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_getwholeline(sb, stderr, '\n');
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_appendwholeline(sb, stderr, '\n');
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_getline(sb, stderr);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_getline_lf(sb, stderr);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_getline_nul(sb, stderr);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_getwholeline_fd(sb, fd, '\n');
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_read_file(sb, "/dev/null", 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_add_lines(sb, "foo", "bar", 0);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_addstr_xml_quoted(sb, "foo");
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_addstr_urlencode(sb, "foo", allow_unencoded_fn);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_humanise_bytes(sb, 42);
	fprintf(stderr, "at line %d\n", __LINE__);
	strbuf_humanise_rate(sb, 42);
	fprintf(stderr, "at line %d\n", __LINE__);
	printf_ln("%s", sb->buf);
	fprintf(stderr, "at line %d\n", __LINE__);
	fprintf_ln(stderr, "%s", sb->buf);
	fprintf(stderr, "at line %d\n", __LINE__);
	xstrdup_tolower("foo");
	fprintf(stderr, "at line %d\n", __LINE__);
	xstrdup_toupper("foo");
	fprintf(stderr, "at line %d\n", __LINE__);
	xstrfmt("%s", "foo");
	fprintf(stderr, "at line %d\n", __LINE__);
}

static void error_builtin(const char *err, va_list params) {}
static void warn_builtin(const char *err, va_list params) {}

static void usage_funcs(void) {
	fprintf(stderr, "calling usage functions\n");
	error("foo");
	error_errno("foo");
	die_message("foo");
	die_message_errno("foo");
	warning("foo");
	warning_errno("foo");

	get_die_message_routine();
	set_error_routine(error_builtin);
	get_error_routine();
	set_warn_routine(warn_builtin);
	get_warn_routine();
}

static void wrapper_funcs(void) {
	int tmp;
	void *ptr = xmalloc(1);
	int fd = open("/dev/null", O_RDONLY);
	struct strbuf sb = STRBUF_INIT;
	int mode = 0444;
	char host[PATH_MAX], path[PATH_MAX], path1[PATH_MAX];
	xsnprintf(path, sizeof(path), "out-XXXXXX");
	xsnprintf(path1, sizeof(path1), "out-XXXXXX");

	fprintf(stderr, "calling wrapper functions\n");

	xstrdup("foo");
	xmalloc(1);
	xmallocz(1);
	xmallocz_gently(1);
	xmemdupz("foo", 3);
	xstrndup("foo", 3);
	xrealloc(ptr, 2);
	xcalloc(1, 1);
	xsetenv("foo", "bar", 0);
	xopen("/dev/null", O_RDONLY);
	xread(fd, &sb, 1);
	xwrite(fd, &sb, 1);
	xpread(fd, &sb, 1, 0);
	xdup(fd);
	xfopen("/dev/null", "r");
	xfdopen(fd, "r");
	tmp = xmkstemp(path);
	close(tmp);
	unlink(path);
	tmp = xmkstemp_mode(path1, mode);
	close(tmp);
	unlink(path1);
	xgetcwd();
	fopen_for_writing(path);
	fopen_or_warn(path, "r");
	xstrncmpz("foo", "bar", 3);
	xgethostname(host, 3);
	tmp = git_mkstemps_mode(path, 1, mode);
	close(tmp);
	unlink(path);
	tmp = git_mkstemp_mode(path, mode);
	close(tmp);
	unlink(path);
	read_in_full(fd, &sb, 1);
	write_in_full(fd, &sb, 1);
	pread_in_full(fd, &sb, 1, 0);
}

int main(int argc, const char **argv) {
	abspath_funcs();
	hex_ll_funcs();
	parse_funcs();
	strbuf_funcs();
	usage_funcs();
	wrapper_funcs();
	fprintf(stderr, "all git-std-lib functions finished calling\n");
	return 0;
}
