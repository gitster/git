#!/bin/sh

test_description='tests for git-history command'

. ./test-lib.sh

test_expect_success 'refuses to do anything without subcommand' '
	test_must_fail git history 2>err &&
	test_grep foo err
'

test_done
