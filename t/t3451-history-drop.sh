#!/bin/sh

test_description='tests for git-history drop subcommand'

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
		test_must_fail git history drop HEAD 2>err &&
		test_grep "commit to be dropped must not be a merge commit" err &&
		test_must_fail git history drop HEAD~ 2>err &&
		test_grep "cannot rearrange commit history with merges" err
	)
'

test_expect_success 'refuses to work when history becomes empty' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit base &&
		test_must_fail git history drop HEAD 2>err &&
		test_grep "cannot drop the only commit on this branch" err
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
		test_must_fail git history drop HEAD 2>err &&
		test_grep "Your local changes to the following files would be overwritten" err &&
		git add file &&
		test_must_fail git history drop HEAD 2>err &&
		test_grep "Your local changes to the following files would be overwritten" err
	)
'

test_expect_success 'can drop tip of a branch' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&

		git symbolic-ref HEAD >expect &&
		git history drop HEAD &&
		git symbolic-ref HEAD >actual &&
		test_cmp expect actual &&

		cat >expect <<-EOF &&
		second
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'can drop commit in the middle' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		test_commit fourth &&
		test_commit fifth &&

		git symbolic-ref HEAD >expect &&
		git history drop HEAD~2 &&
		git symbolic-ref HEAD >actual &&
		test_cmp expect actual &&

		cat >expect <<-EOF &&
		fifth
		fourth
		second
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'correct order is retained' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		test_commit fourth &&
		test_commit fifth &&
		git history drop HEAD~3 &&
		cat >expect <<-EOF &&
		fifth
		fourth
		third
		first
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

		git history drop HEAD~ &&
		cat >expect <<-EOF &&
		prepare-commit-msg: .git/COMMIT_EDITMSG message
		post-commit
		post-rewrite: history
		$(git rev-parse third) $(git rev-parse HEAD)
		EOF
		test_cmp expect hooks.log
	)
'

test_expect_success 'can drop root commit' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		git history drop HEAD~2 &&
		cat >expect <<-EOF &&
		third
		second
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'conflicts are detected' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit base &&
		echo original >file &&
		git add . &&
		git commit -m original &&
		echo modified >file &&
		git commit -am modified &&

		test_must_fail git history drop HEAD~ >err 2>&1 &&
		test_grep CONFLICT err &&
		test_grep "git history continue" err &&
		echo resolved >file &&
		git add file &&
		git history continue &&

		cat >expect <<-EOF &&
		modified
		base
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual &&
		echo resolved >expect &&
		git cat-file -p HEAD:file >actual &&
		test_cmp expect actual
	)
'

test_done
