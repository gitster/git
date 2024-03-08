#!/bin/sh

# A simple wrapper to run shell tests via TEST_SHELL_PATH,
# or exec unit tests directly.

case "$1" in
*.sh)
	exec ${TEST_SHELL_PATH:-/bin/sh} "$@"
	;;
*)
	exec "$@"
	;;
esac
