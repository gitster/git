#!/bin/sh

test_description='tests for git-history command'

. ./test-lib.sh

test_expect_success 'refuses to do anything without subcommand' '
	test_must_fail git history 2>err &&
	test_grep "need a subcommand" err
'

test_expect_success 'abort complains about arguments' '
	test_must_fail git history abort foo 2>err &&
	test_grep "command does not take arguments" err
'

test_expect_success 'abort complains when no history edit is active' '
	test_must_fail git history abort 2>err &&
	test_grep "no history edit in progress" err
'

test_expect_success 'continue complains about arguments' '
	test_must_fail git history continue foo 2>err &&
	test_grep "command does not take arguments" err
'

test_expect_success 'continue complains when no history edit is active' '
	test_must_fail git history continue 2>err &&
	test_grep "no history edit in progress" err
'

test_expect_success 'quit complains about arguments' '
	test_must_fail git history quit foo 2>err &&
	test_grep "command does not take arguments" err
'

test_expect_success 'quit does not complain when no history edit is active' '
	git history quit 2>err &&
	test_must_be_empty err
'

test_done
