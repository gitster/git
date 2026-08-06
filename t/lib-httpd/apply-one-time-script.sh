#!/bin/sh

# If "one-time-script" exists in $HTTPD_ROOT_PATH, run the script on the HTTP
# response. If the response was modified as a result, delete "one-time-script"
# so that subsequent HTTP responses are no longer modified.
#
# This can be used to simulate the effects of the repository changing in
# between HTTP request-response pairs.
#
# Apache can run this CGI for concurrent requests (for example a partial fetch
# that lazily fetches a missing object while the first response is still in
# flight), so the helper claims the marker atomically with a rename, and only
# once it has decided to modify the response. A request that loses the race
# finds the marker already gone and serves its response unchanged; no request
# is left emitting an empty body, which the server would report as HTTP 500.
# Scratch files are per-request ($$) so concurrent requests do not clobber each
# other.
#
# The script may run more than once: the marker is consumed when the response
# actually changes (the rename after "cmp"), not when the script runs, so a
# request whose response is not the targeted one runs the script, sees no
# change, and leaves the marker for a later request. That is safe because the
# scripts are stateless filters over the captured response.

test -f one-time-script || exec "$GIT_EXEC_PATH/git-http-backend"

LC_ALL=C
export LC_ALL

out=out.$$
modified=out-modified.$$
"$GIT_EXEC_PATH/git-http-backend" >"$out"

if ./one-time-script "$out" 2>/dev/null >"$modified" &&
   ! cmp -s "$out" "$modified" &&
   mv one-time-script one-time-script.$$ 2>/dev/null
then
	cat "$modified"
else
	cat "$out"
fi
rm -f "$out" "$modified" one-time-script.$$
