#ifndef MISSING_H
#define MISSING_H

enum missing_action {
	MA_ERROR = 0,      /* fail if any missing objects are encountered */
	MA_ALLOW_ANY,      /* silently allow ALL missing objects */
	MA_PRINT,          /* print ALL missing objects in special section */
	MA_ALLOW_PROMISOR, /* silently allow all missing PROMISOR objects */
};

/*
  Return an `enum missing_action` in case parsing is successful or -1
  if parsing failed.
*/
int parse_missing_action_value(const char *value);

/*
  Return a 'res' with the following meaning:
    0 <= res : an MA_FOO value that is OK for packing
    -1 = res : parse_missing_action_value() failed
    -1 > res : (2 - res) is an MA_FOO value which is unsuitable for packing
 */
int parse_missing_action_value_for_packing(const char *value);

/* Return a short string literal describing the action. */
const char *missing_action_to_string(enum missing_action action);

#endif /* MISSING_H */
