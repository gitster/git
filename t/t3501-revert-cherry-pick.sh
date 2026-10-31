#!/bin/sh

test_description='miscellaneous basic tests for cherry-pick and revert'

GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh

test_expect_success setup '

	for l in a b c d e f g h i j k l m n o
	do
		echo $l$l$l$l$l$l$l$l$l || return 1
	done >oops &&

	test_tick &&
	git add oops &&
	git commit -m initial &&
	git tag initial &&

	test_tick &&
	echo "Add extra line at the end" >>oops &&
	git commit -a -m added &&
	git tag added &&

	test_tick &&
	git mv oops spoo &&
	git commit -m rename1 &&
	git tag rename1 &&

	test_tick &&
	git checkout -b side initial &&
	git mv oops opos &&
	git commit -m rename2 &&
	git tag rename2
'

test_expect_success 'cherry-pick --nonsense' '

	pos=$(git rev-parse HEAD) &&
	git diff --exit-code HEAD &&
	test_must_fail git cherry-pick --nonsense 2>msg &&
	git diff --exit-code HEAD "$pos" &&
	test_grep "[Uu]sage:" msg
'

test_expect_success 'revert --nonsense' '

	pos=$(git rev-parse HEAD) &&
	git diff --exit-code HEAD &&
	test_must_fail git revert --nonsense 2>msg &&
	git diff --exit-code HEAD "$pos" &&
	test_grep "[Uu]sage:" msg
'

# the following two test cherry-pick and revert with renames
#
# --
#  + rename2: renames oops to opos
# +  rename1: renames oops to spoo
# +  added:   adds extra line to oops
# ++ initial: has lines in oops

test_expect_success 'cherry-pick after renaming branch' '

	git checkout rename2 &&
	git cherry-pick added &&
	test_cmp_rev rename2 HEAD^ &&
	test_grep "Add extra line at the end" opos &&
	git reflog -1 | grep cherry-pick

'

test_expect_success 'revert after renaming branch' '

	git checkout rename1 &&
	git revert added &&
	test_cmp_rev rename1 HEAD^ &&
	test_path_is_file spoo &&
	test_cmp_rev initial:oops HEAD:spoo &&
	git reflog -1 | grep revert

'

test_expect_success 'cherry-pick on stat-dirty working tree' '
	git clone . copy &&
	(
		cd copy &&
		git checkout initial &&
		test-tool chmtime +40 oops &&
		git cherry-pick added
	)
'

test_expect_success 'revert forbidden on dirty working tree' '

	echo content >extra_file &&
	git add extra_file &&
	test_must_fail git revert HEAD 2>errors &&
	test_grep "your local changes would be overwritten by " errors

'

test_expect_success 'cherry-pick on unborn branch' '
	git switch --orphan unborn &&
	git rm --cached -r . &&
	git cherry-pick initial &&
	git diff --exit-code initial &&
	test_cmp_rev ! initial HEAD
'

test_expect_success 'cherry-pick on unborn branch with --allow-empty' '
	git checkout --detach &&
	git branch -D unborn &&
	git switch --orphan unborn &&
	git cherry-pick initial --allow-empty &&
	git diff --exit-code initial &&
	test_cmp_rev ! initial HEAD
'

test_expect_success 'cherry-pick "-" to pick from previous branch' '
	git checkout unborn &&
	test_commit to-pick actual content &&
	git checkout main &&
	git cherry-pick - &&
	echo content >expect &&
	test_cmp expect actual
'

test_expect_success 'cherry-pick "-" is meaningless without checkout' '
	test_create_repo afresh &&
	(
		cd afresh &&
		test_commit one &&
		test_commit two &&
		test_commit three &&
		test_must_fail git cherry-pick -
	)
'

test_expect_success 'cherry-pick "-" works with arguments' '
	git checkout -b side-branch &&
	test_commit change actual change &&
	git checkout main &&
	git cherry-pick -s - &&
	echo "Signed-off-by: C O Mitter <committer@example.com>" >expect &&
	git cat-file commit HEAD | grep ^Signed-off-by: >signoff &&
	test_cmp expect signoff &&
	echo change >expect &&
	test_cmp expect actual
'

test_expect_success 'cherry-pick works with dirty renamed file' '
	test_commit to-rename &&
	git checkout -b unrelated &&
	test_commit unrelated &&
	git checkout @{-1} &&
	git mv to-rename.t renamed &&
	test_tick &&
	git commit -m renamed &&
	echo modified >renamed &&
	git cherry-pick refs/heads/unrelated &&
	test $(git rev-parse :0:renamed) = $(git rev-parse HEAD~2:to-rename.t) &&
	test_grep -q "^modified$" renamed
'

test_expect_success 'advice from failed revert' '
	test_when_finished "git reset --hard" &&
	test_commit --no-tag "add dream" dream dream &&
	dream_oid=$(git rev-parse --short HEAD) &&
	cat <<-EOF >expected &&
	error: could not revert $dream_oid... add dream
	hint: After resolving the conflicts, mark them with
	hint: "git add/rm <pathspec>", then run
	hint: "git revert --continue".
	hint: You can instead skip this commit with "git revert --skip".
	hint: To abort and get back to the state before "git revert",
	hint: run "git revert --abort".
	hint: Disable this message with "git config set advice.mergeConflict false"
	EOF
	test_commit --append --no-tag "double-add dream" dream dream &&
	test_must_fail git revert HEAD^ 2>actual &&
	test_cmp expected actual
'

test_expect_subject () {
	echo "$1" >expect &&
	git log -1 --pretty=%s >actual &&
	test_cmp expect actual
}

test_expect_success 'titles of fresh reverts' '
	test_commit --no-tag A file1 &&
	test_commit --no-tag B file1 &&
	git revert --no-edit HEAD &&
	test_expect_subject "Revert \"B\"" &&
	git revert --no-edit HEAD &&
	test_expect_subject "Reapply \"B\"" &&
	git revert --no-edit HEAD &&
	test_expect_subject "Revert \"Reapply \"B\"\""
'

test_expect_success 'title of legacy double revert' '
	test_commit --no-tag "Revert \"Revert \"B\"\"" file1 &&
	git revert --no-edit HEAD &&
	test_expect_subject "Revert \"Revert \"Revert \"B\"\"\""
'

test_expect_success 'identification of reverted commit (default)' '
	test_commit to-ident &&
	test_when_finished "git reset --hard to-ident" &&
	git checkout --detach to-ident &&
	git revert --no-edit HEAD &&
	git cat-file commit HEAD >actual.raw &&
	grep "^This reverts " actual.raw >actual &&
	echo "This reverts commit $(git rev-parse HEAD^)." >expect &&
	test_cmp expect actual
'

test_expect_success 'identification of reverted commit (--reference)' '
	git checkout --detach to-ident &&
	git revert --reference --no-edit HEAD &&
	git cat-file commit HEAD >actual.raw &&
	grep "^This reverts " actual.raw >actual &&
	echo "This reverts commit $(git show -s --pretty=reference HEAD^)." >expect &&
	test_cmp expect actual
'

test_expect_success 'git revert --reference with core.commentChar' '
	test_when_finished "git reset --hard to-ident" &&
	git checkout --detach to-ident &&
	GIT_EDITOR="head -n4 >actual" git -c core.commentChar=% revert \
		--edit --reference HEAD &&
	cat <<-EOF >expect &&
	% *** SAY WHY WE ARE REVERTING ON THE TITLE LINE ***

	This reverts commit $(git show -s --pretty=reference HEAD^).

	EOF
	test_cmp expect actual
'

test_expect_success 'identification of reverted commit (revert.reference)' '
	git checkout --detach to-ident &&
	git -c revert.reference=true revert --no-edit HEAD &&
	git cat-file commit HEAD >actual.raw &&
	grep "^This reverts " actual.raw >actual &&
	echo "This reverts commit $(git show -s --pretty=reference HEAD^)." >expect &&
	test_cmp expect actual
'

test_expect_success 'cherry-pick is unaware of --reference (for now)' '
	test_when_finished "git reset --hard" &&
	test_must_fail git cherry-pick --reference HEAD 2>actual &&
	test_grep "^usage: git cherry-pick" actual
'

# Tests for shallow boundary safeguard

test_expect_success 'setup shallow boundary tests' '
	git init shallow-src &&
	test_commit -C shallow-src A &&
	test_commit -C shallow-src B &&
	test_commit -C shallow-src C &&
	git clone --no-local --depth=2 "file://$(pwd)/shallow-src" shallow-repo &&
	git -C shallow-repo rev-parse --verify B >boundary &&
	test_cmp shallow-repo/.git/shallow boundary
'

test_expect_success 'revert of shallow boundary commit is refused' '
	git -C shallow-repo reset --hard C &&
	test_must_fail git -C shallow-repo revert --no-edit B 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_missing shallow-repo/.git/sequencer &&
	test_path_is_missing shallow-repo/.git/REVERT_HEAD &&
	git -C shallow-repo rev-parse C >expect &&
	git -C shallow-repo rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_expect_success 'cherry-pick of shallow boundary commit is refused' '
	git -C shallow-repo checkout -q --detach C &&
	git -C shallow-repo rm -rq . &&
	git -C shallow-repo commit -q -m empty &&
	git -C shallow-repo rev-parse HEAD >expect &&
	test_must_fail git -C shallow-repo cherry-pick B 2>err &&
	test_grep "shallow boundary commit" err &&
	git -C shallow-repo rev-parse HEAD >actual &&
	test_cmp expect actual &&
	git -C shallow-repo reset -q --hard C
'

test_expect_success 'revert of non-boundary commit in shallow repo works' '
	git -C shallow-repo revert --no-edit C &&
	git -C shallow-repo reset -q --hard C
'

test_expect_success 'multi-commit revert is refused before anything is reverted' '
	test_must_fail git -C shallow-repo revert --no-edit C B 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_missing shallow-repo/.git/sequencer &&
	git -C shallow-repo rev-parse C >expect &&
	git -C shallow-repo rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_expect_success '--allow-shallow-boundary overrides the revert check' '
	git -C shallow-repo revert --no-edit --allow-shallow-boundary B &&
	test_path_is_missing shallow-repo/B.t &&
	git -C shallow-repo reset -q --hard C
'

test_expect_success '--allow-shallow-boundary overrides the cherry-pick check' '
	git -C shallow-repo checkout -q --detach C &&
	git -C shallow-repo rm -rq . &&
	git -C shallow-repo commit -q -m empty &&
	git -C shallow-repo cherry-pick --allow-shallow-boundary B &&
	test_path_is_file shallow-repo/B.t &&
	git -C shallow-repo reset -q --hard C
'

test_expect_success '--allow-shallow-boundary survives --continue' '
	test_commit -C shallow-repo conflict-1 file one &&
	test_commit -C shallow-repo conflict-2 file two &&
	test_must_fail git -C shallow-repo revert --no-edit \
		--allow-shallow-boundary conflict-1 B &&
	echo resolved >shallow-repo/file &&
	git -C shallow-repo add file &&
	git -C shallow-repo revert --continue &&
	test_path_is_missing shallow-repo/B.t &&
	git -C shallow-repo reset -q --hard C
'

test_expect_success 'revert and cherry-pick in a non-shallow repo are unaffected' '
	git init full-repo &&
	test_commit -C full-repo A &&
	test_commit -C full-repo B &&
	git -C full-repo revert --no-edit B &&
	git -C full-repo cherry-pick B &&
	test_path_is_file full-repo/B.t
'

test_expect_success 'rebase of a range containing a shallow boundary is refused' '
	git -C shallow-repo checkout -q --detach C &&
	git -C shallow-repo rev-parse C >expect &&
	test_must_fail git -C shallow-repo rebase --root 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_missing shallow-repo/.git/rebase-merge &&
	git -C shallow-repo rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_expect_success 'rebase --rebase-merges is refused as well' '
	test_must_fail git -C shallow-repo rebase --rebase-merges --root 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_missing shallow-repo/.git/rebase-merge
'

test_expect_success 'rebase --apply of a range containing a shallow boundary is refused' '
	git -C shallow-repo checkout -q --detach C &&
	git -C shallow-repo checkout -q --orphan unrelated &&
	git -C shallow-repo rm -rfq . &&
	test_commit -C shallow-repo unrelated &&
	git -C shallow-repo checkout -q --detach C &&
	git -C shallow-repo rev-parse C >expect &&
	test_must_fail git -C shallow-repo rebase --apply \
		--onto unrelated unrelated 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_missing shallow-repo/.git/rebase-apply &&
	git -C shallow-repo rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_expect_success 'rebase --apply restores an autostash when refusing' '
	echo dirty >shallow-repo/C.t &&
	test_must_fail git -C shallow-repo rebase --apply --autostash \
		--onto unrelated unrelated 2>err &&
	test_grep "shallow boundary commit" err &&
	echo dirty >expect &&
	test_cmp expect shallow-repo/C.t &&
	git -C shallow-repo stash list >stashes &&
	test_must_be_empty stashes &&
	git -C shallow-repo checkout -q -- C.t
'

test_expect_success 'rebase --apply that does not replay the boundary is unaffected' '
	git -C shallow-repo checkout -q -B apply-topic C &&
	test_commit -C shallow-repo E &&
	git -C shallow-repo rebase --apply --onto unrelated C &&
	test_path_is_file shallow-repo/E.t
'

test_expect_success 'rebase that does not replay the boundary is unaffected' '
	git -C shallow-repo checkout -q -B topic C &&
	test_commit -C shallow-repo D &&
	git -C shallow-repo rebase C
'

test_expect_success 'rebase restores an autostash when refusing (merge backend)' '
	git -C shallow-repo checkout -q --detach C &&
	echo dirty >shallow-repo/C.t &&
	test_must_fail git -C shallow-repo rebase --autostash --root 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_missing shallow-repo/.git/rebase-merge &&
	echo dirty >expect &&
	test_cmp expect shallow-repo/C.t &&
	git -C shallow-repo stash list >stashes &&
	test_must_be_empty stashes &&
	git -C shallow-repo checkout -q -- C.t
'

test_expect_success 'interactive rebase restores an autostash when refusing' '
	echo dirty >shallow-repo/C.t &&
	test_must_fail env GIT_SEQUENCE_EDITOR=: \
		git -C shallow-repo rebase -i --autostash --root 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_missing shallow-repo/.git/rebase-merge &&
	echo dirty >expect &&
	test_cmp expect shallow-repo/C.t &&
	git -C shallow-repo stash list >stashes &&
	test_must_be_empty stashes &&
	git -C shallow-repo checkout -q -- C.t
'

test_expect_success 'rebase onto new upstream work in a shallow clone still works' '
	git clone --no-local --depth=1 "file://$(pwd)/shallow-src" shallow-local &&
	test_commit -C shallow-local local &&
	test_commit -C shallow-src upstream &&
	git -C shallow-local fetch origin &&
	git -C shallow-local rebase origin/HEAD &&
	test_path_is_file shallow-local/upstream.t &&
	test_path_is_file shallow-local/local.t
'

test_expect_success 'single-commit shallow clone: revert and cherry-pick are refused' '
	git init one-src &&
	test_commit -C one-src only &&
	git clone --no-local --depth=1 "file://$(pwd)/one-src" one-shallow &&
	git -C one-shallow rev-parse HEAD >expect &&
	test_must_fail git -C one-shallow revert --no-edit HEAD 2>err &&
	test_grep "shallow boundary commit" err &&
	test_must_fail git -C one-shallow cherry-pick -n HEAD 2>err &&
	test_grep "shallow boundary commit" err &&
	test_path_is_file one-shallow/only.t &&
	git -C one-shallow rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_expect_success 'revert and cherry-pick work once the clone is unshallowed' '
	git clone --no-local --depth=2 "file://$(pwd)/shallow-src" unshallowed &&
	test_must_fail git -C unshallowed revert --no-edit HEAD~1 2>err &&
	test_grep "shallow boundary commit" err &&
	git -C unshallowed fetch --unshallow &&
	git -C unshallowed revert --no-edit C &&
	test_path_is_missing unshallowed/C.t &&
	git -C unshallowed cherry-pick C &&
	test_path_is_file unshallowed/C.t
'

test_expect_success '--allow-shallow-boundary cannot be combined with --abort' '
	test_must_fail git -C shallow-repo revert --abort \
		--allow-shallow-boundary 2>err &&
	test_grep "cannot be used with" err
'

test_done
