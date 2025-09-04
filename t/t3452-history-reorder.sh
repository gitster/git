#!/bin/sh

test_description='tests for git-history reorder subcommand'

. ./test-lib.sh

test_expect_success 'refuses to work with merge commits' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit base &&
		git branch branch &&
		test_commit ours &&
		git switch branch &&
		test_commit theirs &&
		git switch - &&
		git merge theirs &&
		test_must_fail git history reorder HEAD --before=HEAD~ 2>err &&
		test_grep "commit to be reordered must not be a merge commit" err &&
		test_must_fail git history reorder HEAD~ --after=HEAD 2>err &&
		test_grep "cannot rearrange commit history with merges" err
	)
'

test_expect_success 'refuses to work with changes in the worktree or index' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit initial &&
		test_commit file file &&
		echo foo >file &&
		test_must_fail git history reorder HEAD --before=HEAD~ 2>err &&
		test_grep "Your local changes to the following files would be overwritten" err &&
		git add file &&
		test_must_fail git history reorder HEAD --before=HEAD~ 2>err &&
		test_grep "Your local changes to the following files would be overwritten" err
	)
'

test_expect_success 'requires exactly one of --before or --after' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_must_fail git history reorder HEAD 2>err &&
		test_grep "exactly one option of ${SQ}before${SQ} or ${SQ}after${SQ} must be given" err &&
		test_must_fail git history reorder HEAD --before=a --after=b 2>err &&
		test_grep "options ${SQ}before${SQ} and ${SQ}after${SQ} cannot be used together" err
	)
'

test_expect_success 'refuses to reorder commit with itself' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_must_fail git history reorder HEAD --after=HEAD 2>err &&
		test_grep "commit to reorder and anchor must not be the same" err
	)
'

test_expect_success '--before can move commit back in history' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		test_commit fourth &&
		test_commit fifth &&
		git history reorder :/fourth --before=:/second &&
		cat >expect <<-EOF &&
		fifth
		third
		second
		fourth
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success '--before can move commit forward in history' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		test_commit fourth &&
		test_commit fifth &&
		git history reorder :/second --before=:/fourth &&
		cat >expect <<-EOF &&
		fifth
		fourth
		second
		third
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success '--before can make a commit a root commit' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		git history reorder :/third --before=:/first &&
		cat >expect <<-EOF &&
		second
		first
		third
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success '--after can move commit back in history' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		test_commit fourth &&
		test_commit fifth &&
		git history reorder :/fourth --after=:/second &&
		cat >expect <<-EOF &&
		fifth
		third
		fourth
		second
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success '--after can move commit forward in history' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		test_commit fourth &&
		test_commit fifth &&
		git history reorder :/second --after=:/fourth &&
		cat >expect <<-EOF &&
		fifth
		second
		fourth
		third
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success '--after can make commit the tip' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		git history reorder :/first --after=:/third &&
		cat >expect <<-EOF &&
		first
		third
		second
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'hooks are executed for rewritten commits' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&

		write_script .git/hooks/prepare-commit-msg <<-EOF &&
		echo "prepare-commit-msg: \$@" >>"$(pwd)/hooks.log"
		EOF
		write_script .git/hooks/post-commit <<-EOF &&
		echo "post-commit" >>"$(pwd)/hooks.log"
		EOF
		write_script .git/hooks/post-rewrite <<-EOF &&
		{
			echo "post-rewrite: \$@"
			cat
		} >>"$(pwd)/hooks.log"
		EOF

		git history reorder :/third --before=:/second &&
		cat >expect <<-EOF &&
		second
		third
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual &&

		cat >expect <<-EOF &&
		prepare-commit-msg: .git/COMMIT_EDITMSG message
		post-commit
		prepare-commit-msg: .git/COMMIT_EDITMSG message
		post-commit
		post-rewrite: history
		$(git rev-parse third) $(git rev-parse HEAD~)
		$(git rev-parse second) $(git rev-parse HEAD)
		EOF
		test_cmp expect hooks.log
	)
'

test_expect_success 'conflicts are detected' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		echo base >file &&
		git add file &&
		git commit -m base &&
		echo "first edit" >file &&
		git commit -am "first edit" &&
		echo "second edit" >file &&
		git commit -am "second edit" &&

		git symbolic-ref HEAD >expect-head &&
		test_must_fail git history reorder HEAD --before=HEAD~ &&
		test_must_fail git symbolic-ref HEAD &&
		echo "second edit" >file &&
		git add file &&
		test_must_fail git history continue &&
		echo "first edit" >file &&
		git add file &&
		git history continue &&

		cat >expect <<-EOF &&
		first edit
		second edit
		base
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual &&

		git symbolic-ref HEAD >actual-head &&
		test_cmp expect-head actual-head
	)
'

test_done
