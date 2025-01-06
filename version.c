#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "version.h"
#include "version-def.h"
#include "strbuf.h"
#include "gettext.h"
#include "config.h"
#include "run-command.h"
#include "alias.h"

const char git_version_string[] = GIT_VERSION;
const char git_built_from_commit_string[] = GIT_BUILT_FROM_COMMIT;

/*
 * Trim and replace each character with ascii code below 32 or above
 * 127 (included) using a dot '.' character.
 * TODO: ensure consecutive non-printable characters are only replaced once
*/
static void redact_non_printables(struct strbuf *buf)
{
	strbuf_trim(buf);
	for (size_t i = 0; i < buf->len; i++) {
		if (buf->buf[i] <= 32 || buf->buf[i] >= 127)
			buf->buf[i] = '.';
	}
}

const char *git_user_agent(void)
{
	static const char *agent = NULL;

	if (!agent) {
		agent = getenv("GIT_USER_AGENT");
		if (!agent)
			agent = GIT_USER_AGENT;
	}

	return agent;
}

const char *git_user_agent_sanitized(void)
{
	static const char *agent = NULL;

	if (!agent) {
		struct strbuf buf = STRBUF_INIT;

		strbuf_addstr(&buf, git_user_agent());
		redact_non_printables(&buf);
		agent = strbuf_detach(&buf, NULL);
	}

	return agent;
}

int get_uname_info(struct strbuf *buf, unsigned int full)
{
	struct utsname uname_info;

	if (uname(&uname_info)) {
		strbuf_addf(buf, _("uname() failed with error '%s' (%d)\n"),
			    strerror(errno),
			    errno);
		return -1;
	}

	if (full)
		strbuf_addf(buf, "%s %s %s %s\n",
			    uname_info.sysname,
			    uname_info.release,
			    uname_info.version,
			    uname_info.machine);
	else
		strbuf_addf(buf, "%s\n", uname_info.sysname);
	return 0;
}

/*
 * Return -1 if unable to retrieve the osversion.command config or
 * if the command is malformed; otherwise, return 0 if successful.
 */
static int fill_os_version_command(struct child_process *cmd)
{
	const char *os_version_command;
	const char **argv;
	char *os_version_copy;
	int n;

	if (git_config_get_string_tmp("osversion.command", &os_version_command))
		return -1;

	os_version_copy = xstrdup(os_version_command);
	n = split_cmdline(os_version_copy, &argv);

	if (n < 0) {
		warning(_("malformed osVersion.command config option: %s"),
			_(split_cmdline_strerror(n)));
		free(os_version_copy);
		return -1;
	}

	for (int i = 0; i < n; i++)
		strvec_push(&cmd->args, argv[i]);
	free(os_version_copy);
	free(argv);

	return 0;
}

static int capture_os_version(struct strbuf *buf)
{
	struct child_process cmd = CHILD_PROCESS_INIT;

	if (fill_os_version_command(&cmd))
		return -1;
	if (capture_command(&cmd, buf, 0))
		return -1;

	return 0;
}

const char *os_version(void)
{
	static const char *os = NULL;

	if (!os) {
		struct strbuf buf = STRBUF_INIT;

		if (capture_os_version(&buf))
			get_uname_info(&buf, 0);
		os = strbuf_detach(&buf, NULL);
	}

	return os;
}

const char *os_version_sanitized(void)
{
	static const char *os_sanitized = NULL;

	if (!os_sanitized) {
		struct strbuf buf = STRBUF_INIT;

		strbuf_addstr(&buf, os_version());
		redact_non_printables(&buf);
		os_sanitized = strbuf_detach(&buf, NULL);
	}

	return os_sanitized;
}

int advertise_os_version(struct repository *r)
{
	static int transfer_advertise_os_version = -1;

	if (transfer_advertise_os_version == -1) {
		repo_config_get_bool(r, "transfer.advertiseosversion", &transfer_advertise_os_version);
		/* enabled by default */
		transfer_advertise_os_version = !!transfer_advertise_os_version;
	}
	return transfer_advertise_os_version;
}
