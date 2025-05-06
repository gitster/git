#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "gettext.h"
#include "hex.h"
#include "odb.h"
#include "promisor-remote.h"
#include "config.h"
#include "trace2.h"
#include "transport.h"
#include "strvec.h"
#include "packfile.h"
#include "environment.h"
#include "url.h"
#include "version.h"

struct promisor_remote_config {
	struct promisor_remote *promisors;
	struct promisor_remote **promisors_tail;
};

static int fetch_objects(struct repository *repo,
			 const char *remote_name,
			 const struct object_id *oids,
			 int oid_nr)
{
	struct child_process child = CHILD_PROCESS_INIT;
	int i;
	FILE *child_in;
	int quiet;

	if (git_env_bool(NO_LAZY_FETCH_ENVIRONMENT, 0)) {
		static int warning_shown;
		if (!warning_shown) {
			warning_shown = 1;
			warning(_("lazy fetching disabled; some objects may not be available"));
		}
		return -1;
	}

	child.git_cmd = 1;
	child.in = -1;
	if (repo != the_repository)
		prepare_other_repo_env(&child.env, repo->gitdir);
	strvec_pushl(&child.args, "-c", "fetch.negotiationAlgorithm=noop",
		     "fetch", remote_name, "--no-tags",
		     "--no-write-fetch-head", "--recurse-submodules=no",
		     "--filter=blob:none", "--stdin", NULL);
	if (!git_config_get_bool("promisor.quiet", &quiet) && quiet)
		strvec_push(&child.args, "--quiet");
	if (start_command(&child))
		die(_("promisor-remote: unable to fork off fetch subprocess"));
	child_in = xfdopen(child.in, "w");

	trace2_data_intmax("promisor", repo, "fetch_count", oid_nr);

	for (i = 0; i < oid_nr; i++) {
		if (fputs(oid_to_hex(&oids[i]), child_in) < 0)
			die_errno(_("promisor-remote: could not write to fetch subprocess"));
		if (fputc('\n', child_in) < 0)
			die_errno(_("promisor-remote: could not write to fetch subprocess"));
	}

	if (fclose(child_in) < 0)
		die_errno(_("promisor-remote: could not close stdin to fetch subprocess"));
	return finish_command(&child) ? -1 : 0;
}

static struct promisor_remote *promisor_remote_new(struct promisor_remote_config *config,
						   const char *remote_name)
{
	struct promisor_remote *r;

	if (*remote_name == '/') {
		warning(_("promisor remote name cannot begin with '/': %s"),
			remote_name);
		return NULL;
	}

	FLEX_ALLOC_STR(r, name, remote_name);

	*config->promisors_tail = r;
	config->promisors_tail = &r->next;

	return r;
}

static struct promisor_remote *promisor_remote_lookup(struct promisor_remote_config *config,
						      const char *remote_name,
						      struct promisor_remote **previous)
{
	struct promisor_remote *r, *p;

	for (p = NULL, r = config->promisors; r; p = r, r = r->next)
		if (!strcmp(r->name, remote_name)) {
			if (previous)
				*previous = p;
			return r;
		}

	return NULL;
}

static void promisor_remote_move_to_tail(struct promisor_remote_config *config,
					 struct promisor_remote *r,
					 struct promisor_remote *previous)
{
	if (!r->next)
		return;

	if (previous)
		previous->next = r->next;
	else
		config->promisors = r->next ? r->next : r;
	r->next = NULL;
	*config->promisors_tail = r;
	config->promisors_tail = &r->next;
}

static int promisor_remote_config(const char *var, const char *value,
				  const struct config_context *ctx UNUSED,
				  void *data)
{
	struct promisor_remote_config *config = data;
	const char *name;
	size_t namelen;
	const char *subkey;

	if (parse_config_key(var, "remote", &name, &namelen, &subkey) < 0)
		return 0;

	if (!strcmp(subkey, "promisor")) {
		char *remote_name;

		if (!git_config_bool(var, value))
			return 0;

		remote_name = xmemdupz(name, namelen);

		if (!promisor_remote_lookup(config, remote_name, NULL))
			promisor_remote_new(config, remote_name);

		free(remote_name);
		return 0;
	}
	if (!strcmp(subkey, "partialclonefilter")) {
		struct promisor_remote *r;
		char *remote_name = xmemdupz(name, namelen);

		r = promisor_remote_lookup(config, remote_name, NULL);
		if (!r)
			r = promisor_remote_new(config, remote_name);

		free(remote_name);

		if (!r)
			return 0;

		FREE_AND_NULL(r->partial_clone_filter);
		return git_config_string(&r->partial_clone_filter, var, value);
	}

	return 0;
}

static void promisor_remote_init(struct repository *r)
{
	struct promisor_remote_config *config;

	if (r->promisor_remote_config)
		return;
	config = r->promisor_remote_config =
		xcalloc(1, sizeof(*r->promisor_remote_config));
	config->promisors_tail = &config->promisors;

	repo_config(r, promisor_remote_config, config);

	if (r->repository_format_partial_clone) {
		struct promisor_remote *o, *previous;

		o = promisor_remote_lookup(config,
					   r->repository_format_partial_clone,
					   &previous);
		if (o)
			promisor_remote_move_to_tail(config, o, previous);
		else
			promisor_remote_new(config, r->repository_format_partial_clone);
	}
}

void promisor_remote_clear(struct promisor_remote_config *config)
{
	while (config->promisors) {
		struct promisor_remote *r = config->promisors;
		free(r->partial_clone_filter);
		config->promisors = config->promisors->next;
		free(r);
	}

	config->promisors_tail = &config->promisors;
}

void repo_promisor_remote_reinit(struct repository *r)
{
	promisor_remote_clear(r->promisor_remote_config);
	FREE_AND_NULL(r->promisor_remote_config);
	promisor_remote_init(r);
}

struct promisor_remote *repo_promisor_remote_find(struct repository *r,
						  const char *remote_name)
{
	promisor_remote_init(r);

	if (!remote_name)
		return r->promisor_remote_config->promisors;

	return promisor_remote_lookup(r->promisor_remote_config, remote_name, NULL);
}

int repo_has_promisor_remote(struct repository *r)
{
	return !!repo_promisor_remote_find(r, NULL);
}

int repo_has_accepted_promisor_remote(struct repository *r)
{
	struct promisor_remote *p;

	promisor_remote_init(r);

	for (p = r->promisor_remote_config->promisors; p; p = p->next)
		if (p->accepted)
			return 1;
	return 0;
}

static int remove_fetched_oids(struct repository *repo,
			       struct object_id **oids,
			       int oid_nr, int to_free)
{
	int i, remaining_nr = 0;
	int *remaining = xcalloc(oid_nr, sizeof(*remaining));
	struct object_id *old_oids = *oids;
	struct object_id *new_oids;

	for (i = 0; i < oid_nr; i++)
		if (odb_read_object_info_extended(repo->objects, &old_oids[i], NULL,
						  OBJECT_INFO_SKIP_FETCH_OBJECT)) {
			remaining[i] = 1;
			remaining_nr++;
		}

	if (remaining_nr) {
		int j = 0;
		CALLOC_ARRAY(new_oids, remaining_nr);
		for (i = 0; i < oid_nr; i++)
			if (remaining[i])
				oidcpy(&new_oids[j++], &old_oids[i]);
		*oids = new_oids;
		if (to_free)
			free(old_oids);
	}

	free(remaining);

	return remaining_nr;
}

void promisor_remote_get_direct(struct repository *repo,
				const struct object_id *oids,
				int oid_nr)
{
	struct promisor_remote *r;
	struct object_id *remaining_oids = (struct object_id *)oids;
	int remaining_nr = oid_nr;
	int to_free = 0;
	int i;

	if (oid_nr == 0)
		return;

	promisor_remote_init(repo);

	for (r = repo->promisor_remote_config->promisors; r; r = r->next) {
		if (fetch_objects(repo, r->name, remaining_oids, remaining_nr) < 0) {
			if (remaining_nr == 1)
				continue;
			remaining_nr = remove_fetched_oids(repo, &remaining_oids,
							 remaining_nr, to_free);
			if (remaining_nr) {
				to_free = 1;
				continue;
			}
		}
		goto all_fetched;
	}

	for (i = 0; i < remaining_nr; i++) {
		if (is_promisor_object(repo, &remaining_oids[i]))
			die(_("could not fetch %s from promisor remote"),
			    oid_to_hex(&remaining_oids[i]));
	}

all_fetched:
	if (to_free)
		free(remaining_oids);
}

static int allow_unsanitized(char ch)
{
	if (ch == ',' || ch == ';' || ch == '%')
		return 0;
	return ch > 32 && ch < 127;
}

/*
 * List of field names allowed to be used in the "promisor-remote"
 * protocol capability. Each field should correspond to a configurable
 * property of a remote that can be relevant for the client.
 */
static const char *allowed_fields[] = {
	"partialCloneFilter", /* Filter used for partial clone */
	"token",              /* Authentication token for the remote */
	NULL
};

/*
 * Check if 'field' is in the list of allowed field names for the
 * "promisor-remote" protocol capability.
 */
static int is_allowed_field(const char *field)
{
	const char **p;

	for (p = allowed_fields; *p; p++)
		if (!strcasecmp(*p, field))
			return 1;
	return 0;
}

static int valid_field(struct string_list_item *item, void *cb_data)
{
	const char *field = item->string;
	const char *config_key = (const char *)cb_data;

	if (!is_allowed_field(field)) {
		warning(_("unsupported field '%s' in '%s' config"), field, config_key);
		return 0;
	}
	return 1;
}

static char *fields_from_config(struct string_list *fields_list, const char *config_key)
{
	char *fields = NULL;

	if (!git_config_get_string(config_key, &fields) && *fields) {
		string_list_split_in_place(fields_list, fields, ", ", -1);
		filter_string_list(fields_list, 0, valid_field, (void *)config_key);
	}

	return fields;
}

static struct string_list *fields_sent(void)
{
	static struct string_list fields_list = STRING_LIST_INIT_NODUP;
	static int initialized = 0;

	if (!initialized) {
		fields_list.cmp = strcasecmp;
		fields_from_config(&fields_list, "promisor.sendFields");
		initialized = 1;
	}

	return &fields_list;
}

static struct string_list *fields_checked(void)
{
	static struct string_list fields_list = STRING_LIST_INIT_NODUP;
	static int initialized = 0;

	if (!initialized) {
		fields_list.cmp = strcasecmp;
		fields_from_config(&fields_list, "promisor.checkFields");
		initialized = 1;
	}

	return &fields_list;
}

static void append_fields(struct string_list *fields,
			  struct string_list *field_names,
			  const char *name)
{
	struct string_list_item *item;

	for_each_string_list_item(item, field_names) {
		char *key = xstrfmt("remote.%s.%s", name, item->string);
		const char *val;
		if (!git_config_get_string_tmp(key, &val) && *val)
			string_list_append(fields, item->string)->util = (char *)val;
		free(key);
	}
}

/*
 * Linked list for promisor remotes involved in the "promisor-remote"
 * protocol capability.
 *
 * 'fields' contains a defined set of field name/value pairs for
 * each promisor remote. Field names are stored in the 'string'
 * member, and values in the 'util' member.
 *
 * Currently supported field names:
 * - "name": The name of the promisor remote,
 * - "url": The URL of the promisor remote,
 * - the fields in 'allowed_fields[]' above.
 *
 * Except for "name", each "<field_name>/<field_value>" pair should
 * correspond to a "remote.<name>.<field_name>" config variable set to
 * <field_value> where "<name>" is a promisor remote name.
 *
 * 'fields' should not be sorted, as we will rely on the order we put
 * things into it. So, for example, 'string_list_append()' should be
 * used instead of 'string_list_insert()'.
 */
struct promisor_info {
	struct promisor_info *next;
	struct string_list fields;
};

static void promisor_info_list_free(struct promisor_info *p)
{
	struct promisor_info *next;

	for (; p; p = next) {
		next = p->next;
		string_list_clear(&p->fields, 0);
		free(p);
	}
}

/*
 * Prepare a 'struct promisor_info' linked list of promisor
 * remotes. For each promisor remote, some of its fields, starting
 * with "name" and "url", are put in the 'fields' string_list.
 */
static struct promisor_info *promisor_info_list(struct repository *repo,
						struct string_list *field_names)
{
	struct promisor_info *infos = NULL;
	struct promisor_info **last_info = &infos;
	struct promisor_remote *r;

	promisor_remote_init(repo);

	for (r = repo->promisor_remote_config->promisors; r; r = r->next) {
		const char *url;
		char *url_key = xstrfmt("remote.%s.url", r->name);

		/* Only add remotes with a non empty URL */
		if (!git_config_get_string_tmp(url_key, &url) && *url) {
			struct promisor_info *new_info = xcalloc(1, sizeof(*new_info));

			string_list_init_dup(&new_info->fields);
			new_info->fields.cmp = strcasecmp;

			string_list_append(&new_info->fields, "name")->util = (char *)r->name;
			string_list_append(&new_info->fields, "url")->util = (char *)url;

			if (field_names)
				append_fields(&new_info->fields, field_names, r->name);

			*last_info = new_info;
			last_info = &new_info->next;
		}

		free(url_key);
	}

	return infos;
}

char *promisor_remote_info(struct repository *repo)
{
	struct strbuf sb = STRBUF_INIT;
	int advertise_promisors = 0;
	struct promisor_info *info_list;
	struct promisor_info *r;

	git_config_get_bool("promisor.advertise", &advertise_promisors);

	if (!advertise_promisors)
		return NULL;

	info_list = promisor_info_list(repo, fields_sent());

	if (!info_list)
		return NULL;

	for (r = info_list; r; r = r->next) {
		struct string_list_item *item;
		int first = 1;

		if (r != info_list)
			strbuf_addch(&sb, ';');

		for_each_string_list_item(item, &r->fields) {
			if (first)
				first = 0;
			else
				strbuf_addch(&sb, ',');
			strbuf_addf(&sb, "%s=", item->string);
			strbuf_addstr_urlencode(&sb, (char *)item->util, allow_unsanitized);
		}
	}

	promisor_info_list_free(info_list);

	return strbuf_detach(&sb, NULL);
}

/*
 * Find first element of 'p' where the 'name' field is 'nick'. 'nick'
 * is compared case sensitively to the strings in 'p'. If not found
 * NULL is returned.
 */
static struct promisor_info *remote_nick_find(struct promisor_info *p, const char *nick)
{
	for (; p; p = p->next) {
		if (strcmp(p->fields.items[0].string, "name"))
			BUG("First field of promisor info should be 'name', but was '%s'.",
			    p->fields.items[0].string);
		if (!strcmp(p->fields.items[0].util, nick))
			return p;
	}
	return NULL;
}

enum accept_promisor {
	ACCEPT_NONE = 0,
	ACCEPT_KNOWN_URL,
	ACCEPT_KNOWN_NAME,
	ACCEPT_ALL
};

static int check_field_one(struct string_list_item *item_value,
			   struct promisor_info *p)
{
	struct string_list_item *item;

	item = unsorted_string_list_lookup(&p->fields, item_value->string);
	if (!item)
		return 0;

	return !strcmp(item->util, item_value->util);
}


static int check_field(struct string_list_item *item_value,
		       struct promisor_info *p, int in_list)
{
	if (!in_list)
		return check_field_one(item_value, p);

	for (; p; p = p->next)
		if (check_field_one(item_value, p))
			return 1;

	return 0;
}

static int check_all_fields(struct string_list* values,
			    struct promisor_info *p,
			    int in_list)
{
	struct string_list* fields = fields_checked();
	struct string_list_item *item_checked;

	string_list_sort(values);

	for_each_string_list_item(item_checked, fields) {
		struct string_list_item *item_value;

		item_value = string_list_lookup(values, item_checked->string);
		if (!item_value)
			return 0;
		if (!check_field(item_value, p, in_list))
			return 0;
	}

	return 1;
}

static int should_accept_remote(enum accept_promisor accept,
				const char *remote_name,
				const char *remote_url,
				struct string_list* values,
				struct promisor_info *info_list)
{
	struct promisor_info *p;
	const char *local_url;

	if (accept == ACCEPT_ALL)
		return check_all_fields(values, info_list, 1);

	p = remote_nick_find(info_list, remote_name);

	if (!p)
		/* We don't know about that remote */
		return 0;

	if (accept == ACCEPT_KNOWN_NAME)
		return check_all_fields(values, p, 0);

	if (accept != ACCEPT_KNOWN_URL)
		BUG("Unhandled 'enum accept_promisor' value '%d'", accept);

	if (!remote_url || !*remote_url) {
		warning(_("no or empty URL advertised for remote '%s'"), remote_name);
		return 0;
	}

	if (strcmp(p->fields.items[1].string, "url"))
		BUG("Bad info_list for remote '%s'.\n"
		    "Second field of promisor info should be 'url', but was '%s'.",
		    remote_name, p->fields.items[1].string);

	local_url = p->fields.items[1].util;

	if (!strcmp(local_url, remote_url))
		return check_all_fields(values, p, 0);

	warning(_("known remote named '%s' but with URL '%s' instead of '%s'"),
		remote_name, local_url, remote_url);

	return 0;
}

static void filter_promisor_remote(struct repository *repo,
				   struct strvec *accepted,
				   const char *info)
{
	struct strbuf **remotes;
	const char *accept_str;
	enum accept_promisor accept = ACCEPT_NONE;
	struct promisor_info *info_list = NULL;

	if (!git_config_get_string_tmp("promisor.acceptfromserver", &accept_str)) {
		if (!*accept_str || !strcasecmp("None", accept_str))
			accept = ACCEPT_NONE;
		else if (!strcasecmp("KnownUrl", accept_str))
			accept = ACCEPT_KNOWN_URL;
		else if (!strcasecmp("KnownName", accept_str))
			accept = ACCEPT_KNOWN_NAME;
		else if (!strcasecmp("All", accept_str))
			accept = ACCEPT_ALL;
		else
			warning(_("unknown '%s' value for '%s' config option"),
				accept_str, "promisor.acceptfromserver");
	}

	if (accept == ACCEPT_NONE)
		return;

	/* Parse remote info received */

	remotes = strbuf_split_str(info, ';', 0);

	for (size_t i = 0; remotes[i]; i++) {
		struct strbuf **elems;
		const char *remote_name = NULL;
		const char *remote_url = NULL;
		char *decoded_name = NULL;
		char *decoded_url = NULL;
		struct string_list field_values = STRING_LIST_INIT_NODUP;

		field_values.cmp = strcasecmp;

		strbuf_strip_suffix(remotes[i], ";");
		elems = strbuf_split(remotes[i], ',');

		for (size_t j = 0; elems[j]; j++) {
			char *p;

			strbuf_strip_suffix(elems[j], ",");
			if (skip_prefix(elems[j]->buf, "name=", &remote_name) ||
			    skip_prefix(elems[j]->buf, "url=", &remote_url))
				continue;

			p = strchr(elems[j]->buf, '=');
			if (p) {
				*p = '\0';
				string_list_append(&field_values, elems[j]->buf)->util = p + 1;
			} else {
				warning(_("invalid element '%s' from remote info"),
					elems[j]->buf);
			}
		}

		if (remote_name)
			decoded_name = url_percent_decode(remote_name);
		if (remote_url)
			decoded_url = url_percent_decode(remote_url);

		if (decoded_name) {
			if (!info_list)
				info_list = promisor_info_list(repo, fields_checked());

			if (should_accept_remote(accept, decoded_name, decoded_url,
						 &field_values, info_list))
				strvec_push(accepted, decoded_name);
		}

		string_list_clear(&field_values, 0);
		strbuf_list_free(elems);
		free(decoded_name);
		free(decoded_url);
	}

	promisor_info_list_free(info_list);
	strbuf_list_free(remotes);
}

char *promisor_remote_reply(const char *info)
{
	struct strvec accepted = STRVEC_INIT;
	struct strbuf reply = STRBUF_INIT;

	filter_promisor_remote(the_repository, &accepted, info);

	if (!accepted.nr)
		return NULL;

	for (size_t i = 0; i < accepted.nr; i++) {
		if (i)
			strbuf_addch(&reply, ';');
		strbuf_addstr_urlencode(&reply, accepted.v[i], allow_unsanitized);
	}

	strvec_clear(&accepted);

	return strbuf_detach(&reply, NULL);
}

void mark_promisor_remotes_as_accepted(struct repository *r, const char *remotes)
{
	struct strbuf **accepted_remotes = strbuf_split_str(remotes, ';', 0);

	for (size_t i = 0; accepted_remotes[i]; i++) {
		struct promisor_remote *p;
		char *decoded_remote;

		strbuf_strip_suffix(accepted_remotes[i], ";");
		decoded_remote = url_percent_decode(accepted_remotes[i]->buf);

		p = repo_promisor_remote_find(r, decoded_remote);
		if (p)
			p->accepted = 1;
		else
			warning(_("accepted promisor remote '%s' not found"),
				decoded_remote);

		free(decoded_remote);
	}

	strbuf_list_free(accepted_remotes);
}
