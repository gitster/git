#include "git-compat-util.h"
#include "version.h"
#include "strbuf.h"
#include "sane-ctype.h"
#include "gettext.h"
#include "config.h"

#ifndef GIT_VERSION_H
# include "version-def.h"
#else
# include GIT_VERSION_H
#endif

const char git_version_string[] = GIT_VERSION;
const char git_built_from_commit_string[] = GIT_BUILT_FROM_COMMIT;

/*
 * Trim and replace each character with ascii code below 32 or above
 * 127 (included) using a dot '.' character.
 */
static void redact_non_printables(struct strbuf *buf)
{
	strbuf_trim(buf);
	for (size_t i = 0; i < buf->len; i++) {
		if (!isprint(buf->buf[i]) || buf->buf[i] == ' ')
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

const char *os_version_sanitized(void)
{
	static const char *os = NULL;

	if (!os) {
		struct strbuf buf = STRBUF_INIT;

		get_uname_info(&buf, 0);
		/* Sanitize the os information immediately */
		redact_non_printables(&buf);
		os = strbuf_detach(&buf, NULL);
	}

	return os;
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
