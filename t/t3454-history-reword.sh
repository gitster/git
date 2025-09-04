#!/bin/sh

test_description='tests for git-history reword subcommand'

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
		test_must_fail git history reword HEAD~ 2>err &&
		test_grep "cannot rearrange commit history with merges" err &&
		test_must_fail git history reword HEAD 2>err &&
		test_grep "cannot rearrange commit history with merges" err
	)
'

test_expect_success 'refuses to work with changes in the worktree or index' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit base file &&
		echo foo >file &&
		test_must_fail git history reword HEAD 2>err &&
		test_grep "Your local changes to the following files would be overwritten" err &&
		git add file &&
		test_must_fail git history reword HEAD 2>err &&
		test_grep "Your local changes to the following files would be overwritten" err
	)
'

test_expect_success 'can reword tip of a branch' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&

		git symbolic-ref HEAD >expect &&
		git history reword -m "third reworded" HEAD &&
		git symbolic-ref HEAD >actual &&
		test_cmp expect actual &&

		cat >expect <<-EOF &&
		third reworded
		second
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'can reword commit in the middle' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&

		git symbolic-ref HEAD >expect &&
		git history reword -m "second reworded" HEAD~ &&
		git symbolic-ref HEAD >actual &&
		test_cmp expect actual &&

		cat >expect <<-EOF &&
		third
		second reworded
		first
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'can reword root commit' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&
		test_commit second &&
		test_commit third &&
		git history reword -m "first reworded" HEAD~2 &&

		cat >expect <<-EOF &&
		third
		second
		first reworded
		EOF
		git log --format=%s >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'can use editor to rewrite commit message' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&

		write_script fake-editor.sh <<-\EOF &&
		cp "$1" . &&
		printf "\namend a comment\n" >>"$1"
		EOF
		test_set_editor "$(pwd)"/fake-editor.sh &&
		git history reword HEAD &&

		cat >expect <<-EOF &&
		first

		# Please enter the commit message for the reworded changes. Lines starting
		# with ${SQ}#${SQ} will be kept; you may remove them yourself if you want to.
		# Changes to be committed:
		#	new file:   first.t
		#
		EOF
		test_cmp expect COMMIT_EDITMSG &&

		cat >expect <<-EOF &&
		first

		amend a comment

		EOF
		git log --format=%B >actual &&
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

		git history reword -m "second reworded" HEAD~ &&

		cat >expect <<-EOF &&
		third
		second reworded
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
		$(git rev-parse second) $(git rev-parse HEAD~)
		$(git rev-parse third) $(git rev-parse HEAD)
		EOF
		test_cmp expect hooks.log
	)
'

test_expect_success 'aborts with empty commit message' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit first &&

		test_must_fail git history reword -m "" HEAD 2>err &&
		test_grep "Aborting commit due to empty commit message." err
	)
'

test_done
