#!/bin/sh

test_description='Test git-std-lib compilation'

. ./test-lib.sh

test_expect_success !WINDOWS 'stdlib-test compiles and runs' '
	test-stdlib
'

test_done
