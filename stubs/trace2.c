#include "git-compat-util.h"
#include "trace2.h"

struct child_process { int stub; };
struct repository { int stub; };
struct json_writer { int stub; };

void trace2_region_enter_fl(const char *file, int line, const char *category,
			    const char *label, const struct repository *repo, ...) { }
void trace2_region_leave_fl(const char *file, int line, const char *category,
			    const char *label, const struct repository *repo, ...) { }
void trace2_data_string_fl(const char *file, int line, const char *category,
			   const struct repository *repo, const char *key,
			   const char *value) { }
void trace2_cmd_ancestry_fl(const char *file, int line, const char **parent_names) { }
void trace2_cmd_error_va_fl(const char *file, int line, const char *fmt,
			    va_list ap) { }
void trace2_cmd_name_fl(const char *file, int line, const char *name) { }
void trace2_thread_start_fl(const char *file, int line,
			    const char *thread_base_name) { }
void trace2_thread_exit_fl(const char *file, int line) { }
void trace2_data_intmax_fl(const char *file, int line, const char *category,
			   const struct repository *repo, const char *key,
			   intmax_t value) { }
int trace2_is_enabled(void) { return 0; }
void trace2_counter_add(enum trace2_counter_id cid, uint64_t value) { }
void trace2_collect_process_info(enum trace2_process_info_reason reason) { }
