#ifndef PARSE_H
#define PARSE_H

bool git_parse_signed(const char *value, intmax_t *ret, intmax_t max);
bool git_parse_unsigned(const char *value, uintmax_t *ret, uintmax_t max);
bool git_parse_ssize_t(const char *, ssize_t *);
bool git_parse_ulong(const char *, unsigned long *);
bool git_parse_int(const char *value, int *ret);
bool git_parse_int64(const char *value, int64_t *ret);
bool git_parse_double(const char *value, double *ret);

/**
 * Same as `git_config_bool`, except that it returns -1 on error rather
 * than dying.
 */
int git_parse_maybe_bool(const char *);
int git_parse_maybe_bool_text(const char *value);

int git_env_bool(const char *, int);
unsigned long git_env_ulong(const char *, unsigned long);

#endif /* PARSE_H */
