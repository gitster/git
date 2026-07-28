#!/bin/sh

test_description='test refs_reflog_edit_in_bulk via test-tool reflog-edit'

. ./test-lib.sh

test_expect_success 'reflog-edit replace message' '
	git init repo &&
	(
		cd repo &&
		test_commit one &&
		test_commit two &&
		test_commit three &&
		test-tool reflog-edit refs/heads/master <<-\EOF &&
		replace 0 msg=reworded three
		EOF
		git reflog refs/heads/master | head -n 1 >actual &&
		test_grep "reworded three" actual
	)
'

test_expect_success 'reflog-edit delete entry' '
	git init repo-del &&
	(
		cd repo-del &&
		test_commit one &&
		test_commit two &&
		test_commit three &&
		test-tool reflog-edit refs/heads/master <<-\EOF &&
		delete 1
		EOF
		git reflog refs/heads/master >actual &&
		! test_grep "commit: two" actual
	)
'

test_expect_success 'reflog-edit multi-edit (replace and delete)' '
	git init repo-multi &&
	(
		cd repo-multi &&
		test_commit one &&
		test_commit two &&
		test_commit three &&
		test-tool reflog-edit refs/heads/master <<-\EOF &&
		replace 0 msg=new three
		delete 2
		EOF
		git reflog refs/heads/master >actual &&
		test_grep "new three" actual &&
		! test_grep "commit: one" actual
	)
'

test_expect_success 'reflog-edit multi-line message gets normalized' '
	git init repo-norm &&
	(
		cd repo-norm &&
		test_commit one &&
		test-tool reflog-edit refs/heads/master <<-\EOF &&
		replace 0 msg=first line\nsecond line
		EOF
		git reflog refs/heads/master | head -n 1 >actual &&
		test_grep "first line second line" actual
	)
'

test_expect_success 'reflog-edit out of bounds' '
	git init repo-oob &&
	(
		cd repo-oob &&
		test_commit one &&
		test_must_fail test-tool reflog-edit refs/heads/master <<-\EOF
		replace 10 msg=out of bounds
		EOF
	)
'

test_expect_success 'reflog-edit duplicate index' '
	git init repo-dup &&
	(
		cd repo-dup &&
		test_commit one &&
		test_must_fail test-tool reflog-edit refs/heads/master <<-\EOF
		replace 0 msg=first
		replace 0 msg=second
		EOF
	)
'

test_done
