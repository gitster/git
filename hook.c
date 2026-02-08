#include "git-compat-util.h"
#include "abspath.h"
#include "advice.h"
#include "gettext.h"
#include "hook.h"
#include "path.h"
#include "run-command.h"
#include "config.h"
#include "strbuf.h"
#include "environment.h"
#include "setup.h"

const char *find_hook(struct repository *r, const char *name)
{
	static struct strbuf path = STRBUF_INIT;

	int found_hook;

	repo_git_path_replace(r, &path, "hooks/%s", name);
	found_hook = access(path.buf, X_OK) >= 0;
#ifdef STRIP_EXTENSION
	if (!found_hook) {
		int err = errno;

		strbuf_addstr(&path, STRIP_EXTENSION);
		found_hook = access(path.buf, X_OK) >= 0;
		if (!found_hook)
			errno = err;
	}
#endif

	if (!found_hook) {
		if (errno == EACCES && advice_enabled(ADVICE_IGNORED_HOOK)) {
			static struct string_list advise_given = STRING_LIST_INIT_DUP;

			if (!string_list_lookup(&advise_given, name)) {
				string_list_insert(&advise_given, name);
				advise(_("The '%s' hook was ignored because "
					 "it's not set as executable.\n"
					 "You can disable this warning with "
					 "`git config set advice.ignoredHook false`."),
				       path.buf);
			}
		}
		return NULL;
	}
	return path.buf;
}

struct hook_config_cb
{
	const char *hook_event;
	struct string_list *list;
};

/*
 * Callback for git_config which adds configured hooks to a hook list.  Hooks
 * can be configured by specifying both hook.<friendly-name>.command = <path>
 * and hook.<friendly-name>.event = <hook-event>.
 */
static int hook_config_lookup(const char *key, const char *value,
			      const struct config_context *ctx UNUSED,
			      void *cb_data)
{
	struct hook_config_cb *data = cb_data;
	const char *name, *event_key;
	size_t name_len = 0;
	struct string_list_item *item;
	char *hook_name;

	/*
	 * Don't bother doing the expensive parse if there's no
	 * chance that the config matches 'hook.myhook.event = hook_event'.
	 */
	if (!value || strcmp(value, data->hook_event))
		return 0;

	/* Look for "hook.friendly-name.event = hook_event" */
	if (parse_config_key(key, "hook", &name, &name_len, &event_key) ||
	    strcmp(event_key, "event"))
		return 0;

	/* Extract the hook name */
	hook_name = xmemdupz(name, name_len);

	/* Remove the hook if already in the list, so we append in config order. */
	if ((item = unsorted_string_list_lookup(data->list, hook_name)))
		unsorted_string_list_delete_item(data->list, item - data->list->items, 0);

	/* The list takes ownership of hook_name, so append with nodup */
	string_list_append_nodup(data->list, hook_name);

	return 0;
}

struct string_list *list_hooks(struct repository *r, const char *hookname)
{
	struct hook_config_cb cb_data;

	if (!hookname)
		BUG("null hookname was provided to hook_list()!");

	cb_data.hook_event = hookname;
	cb_data.list = xmalloc(sizeof(struct string_list));
	string_list_init_dup(cb_data.list);

	/* Add the hooks from the config, e.g. hook.myhook.event = pre-commit */
	repo_config(r, hook_config_lookup, &cb_data);

	/*
	 * Add the default hook from hookdir. It does not have a friendly name
	 * like the hooks specified via configs, so add it with an empty name.
	 */
	if (r->gitdir && find_hook(r, hookname))
		string_list_append(cb_data.list, "");

	return cb_data.list;
}

int hook_exists(struct repository *r, const char *name)
{
	int exists = 0;
	struct string_list *hooks = list_hooks(r, name);

	exists = hooks->nr > 0;

	string_list_clear(hooks, 1);
	free(hooks);
	return exists;
}

static int pick_next_hook(struct child_process *cp,
			  struct strbuf *out UNUSED,
			  void *pp_cb,
			  void **pp_task_cb)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct string_list *hook_list = hook_cb->hook_command_list;
	struct string_list_item *to_run = hook_cb->next_hook++;

	if (!to_run || to_run >= hook_list->items + hook_list->nr)
		return 0; /* no hook left to run */

	cp->no_stdin = 1;
	strvec_pushv(&cp->env, hook_cb->options->env.v);

	if (hook_cb->options->path_to_stdin && hook_cb->options->feed_pipe)
		BUG("options path_to_stdin and feed_pipe are mutually exclusive");

	/* reopen the file for stdin; run_command closes it. */
	if (hook_cb->options->path_to_stdin) {
		cp->no_stdin = 0;
		cp->in = xopen(hook_cb->options->path_to_stdin, O_RDONLY);
	}

	if (hook_cb->options->feed_pipe) {
		cp->no_stdin = 0;
		/* start_command() will allocate a pipe / stdin fd for us */
		cp->in = -1;
	}

	cp->stdout_to_stderr = hook_cb->options->stdout_to_stderr;
	cp->trace2_hook_name = hook_cb->hook_name;
	cp->dir = hook_cb->options->dir;

	/* find hook commands */
	if (!*to_run->string) {
		/* ...from hookdir signified by empty name */
		const char *hook_path = find_hook(hook_cb->repository,
						  hook_cb->hook_name);
		if (!hook_path)
			BUG("hookdir in hook list but no hook present in filesystem");

		if (hook_cb->options->dir)
			hook_path = absolute_path(hook_path);

		strvec_push(&cp->args, hook_path);
	} else {
		/* ...from config */
		struct strbuf cmd_key = STRBUF_INIT;
		char *command = NULL;

		/* to enable oneliners, let config-specified hooks run in shell. */
		cp->use_shell = true;

		strbuf_addf(&cmd_key, "hook.%s.command", to_run->string);
		if (repo_config_get_string(hook_cb->repository,
					   cmd_key.buf, &command)) {
			die(_("'hook.%s.command' must be configured or"
			      "'hook.%s.event' must be removed; aborting.\n"),
			    to_run->string, to_run->string);
		}
		strbuf_release(&cmd_key);

		strvec_push_nodup(&cp->args, command);
	}

	if (!cp->args.nr)
		BUG("configured hook must have at least one command");

	strvec_pushv(&cp->args, hook_cb->options->args.v);

	/*
	 * Provide per-hook internal state via task_cb for easy access, so
	 * hook callbacks don't have to go through hook_cb->options.
	 */
	*pp_task_cb = to_run;

	return 1;
}

static int notify_start_failure(struct strbuf *out,
				void *pp_cb,
				void *pp_task_cb)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct string_list_item *hook = pp_task_cb;

	hook_cb->rc |= 1;

	if (out && hook) {
		if (*hook->string)
			strbuf_addf(out, _("Couldn't start hook '%s'\n"), hook->string);
		else
			strbuf_addstr(out, _("Couldn't start hook from hooks directory\n"));
	}

	return 1;
}

static int notify_hook_finished(int result,
				struct strbuf *out UNUSED,
				void *pp_cb,
				void *pp_task_cb UNUSED)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct run_hooks_opt *opt = hook_cb->options;

	hook_cb->rc |= result;

	if (opt->invoked_hook)
		*opt->invoked_hook = 1;

	return 0;
}

static void run_hooks_opt_clear(struct run_hooks_opt *options)
{
	strvec_clear(&options->env);
	strvec_clear(&options->args);
}

int run_hooks_opt(struct repository *r, const char *hook_name,
		  struct run_hooks_opt *options)
{
	struct strbuf abs_path = STRBUF_INIT;
	struct hook_cb_data cb_data = {
		.rc = 0,
		.hook_name = hook_name,
		.options = options,
		.hook_command_list = list_hooks(r, hook_name),
		.repository = r,
	};
	int ret = 0;
	const struct run_process_parallel_opts opts = {
		.tr2_category = "hook",
		.tr2_label = hook_name,

		.processes = options->jobs,
		.ungroup = options->jobs == 1,

		.get_next_task = pick_next_hook,
		.start_failure = notify_start_failure,
		.feed_pipe = options->feed_pipe,
		.task_finished = notify_hook_finished,

		.data = &cb_data,
	};

	if (!options)
		BUG("a struct run_hooks_opt must be provided to run_hooks");

	if (options->path_to_stdin && options->feed_pipe)
		BUG("options path_to_stdin and feed_pipe are mutually exclusive");

	if (!options->jobs)
		BUG("run_hooks_opt must be called with options.jobs >= 1");

	/*
	 * Ensure cb_data copy and free functions are either provided together,
	 * or neither one is provided.
	 */
	if ((options->copy_feed_pipe_cb_data && !options->free_feed_pipe_cb_data) ||
	    (!options->copy_feed_pipe_cb_data && options->free_feed_pipe_cb_data))
		BUG("copy_feed_pipe_cb_data and free_feed_pipe_cb_data must be set together");

	if (options->invoked_hook)
		*options->invoked_hook = 0;

	if (!cb_data.hook_command_list->nr) {
		if (options->error_if_missing)
			ret = error("cannot find a hook named %s", hook_name);
		goto cleanup;
	}

	/*
	 * Initialize the iterator/cursor which holds the next hook to run.
	 * run_process_parallel() calls pick_next_hook() which increments it for
	 * each hook command in the list until all hooks have been run.
	 */
	cb_data.next_hook = cb_data.hook_command_list->items;

	/*
	 * Give each hook its own copy of the initial void *pp_task_cb state, if
	 * a copy callback was provided.
	 */
	if (options->copy_feed_pipe_cb_data) {
		struct string_list_item *item;
		for_each_string_list_item(item, cb_data.hook_command_list)
			item->util = options->copy_feed_pipe_cb_data(options->feed_pipe_cb_data);
	}

	run_processes_parallel(&opts);
	ret = cb_data.rc;
cleanup:
	if (options->free_feed_pipe_cb_data) {
		struct string_list_item *item;
		for_each_string_list_item(item, cb_data.hook_command_list)
			options->free_feed_pipe_cb_data(item->util);
	}
	string_list_clear(cb_data.hook_command_list, 0);
	free(cb_data.hook_command_list);
	strbuf_release(&abs_path);
	run_hooks_opt_clear(options);
	return ret;
}

int run_hooks(struct repository *r, const char *hook_name)
{
	struct run_hooks_opt opt = RUN_HOOKS_OPT_INIT;

	return run_hooks_opt(r, hook_name, &opt);
}

int run_hooks_l(struct repository *r, const char *hook_name, ...)
{
	struct run_hooks_opt opt = RUN_HOOKS_OPT_INIT;
	va_list ap;
	const char *arg;

	va_start(ap, hook_name);
	while ((arg = va_arg(ap, const char *)))
		strvec_push(&opt.args, arg);
	va_end(ap);

	return run_hooks_opt(r, hook_name, &opt);
}
