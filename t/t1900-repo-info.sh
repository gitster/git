#!/bin/sh

test_description='test git repo-info'
GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh

parse_json () {
	tr '\n' ' ' | "$PERL_PATH" "$TEST_DIRECTORY/t0019/parse_json.perl"
}

test_lazy_prereq PERLJSON '
	perl -MJSON -e "exit 0"
'

test_expect_success PERLJSON 'json: returns empty output with allow-empty' '
	git repo-info --format=json >output &&
	test_line_count = 2 output
'

test_done
