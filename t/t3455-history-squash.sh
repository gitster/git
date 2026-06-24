#!/bin/sh

test_description='tests for git-history squash subcommand'

. ./test-lib.sh

test_expect_success 'setup linear history touching two files' '
	test_commit base file a &&
	git tag start &&
	test_commit --no-tag one other x &&
	test_commit --no-tag two file c &&
	test_commit three file d
'

test_expect_success 'errors on missing range argument' '
	test_must_fail git history squash 2>err &&
	test_grep "command expects a single revision range" err
'

test_expect_success 'errors on too many arguments' '
	test_must_fail git history squash start.. HEAD 2>err &&
	test_grep "command expects a single revision range" err
'

test_expect_success 'errors on an empty range' '
	test_must_fail git history squash HEAD..HEAD 2>err &&
	test_grep "the range .* is empty" err
'

test_expect_success 'errors on a single revision that is not a range' '
	test_must_fail git history squash HEAD 2>err &&
	test_grep "is not a .*range" err &&
	test_must_fail git history squash HEAD~1 2>err &&
	test_grep "is not a .*range" err
'

test_expect_success 'squashes a range into a single commit without changing the tree' '
	git reset --hard three &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test_cmp_rev start HEAD^ &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})" &&
	git log --format="%s" -1 >subject &&
	echo one >expect &&
	test_cmp expect subject &&
	git reflog >reflog &&
	test_grep "squash: updating" reflog
'

test_expect_success 'squashes an interior range and replays descendants verbatim' '
	git reset --hard three &&
	final_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start..@~1 &&

	git log --format="%s" start..HEAD >actual &&
	cat >expect <<-\EOF &&
	three
	one
	EOF
	test_cmp expect actual &&

	test_cmp_rev start HEAD~2 &&
	test "$final_tree" = "$(git rev-parse HEAD^{tree})"
'

test_expect_success 'squashes when the base is the root commit' '
	git reset --hard three &&
	root=$(git rev-list --max-parents=0 HEAD) &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash "$root.." &&

	git rev-list --count "$root..HEAD" >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test_cmp_rev "$root" HEAD^ &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})"
'

test_expect_success 'squashing a single-commit range replays the rest' '
	git reset --hard three &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start..@~2 &&

	git log --format="%s" start..HEAD >actual &&
	cat >expect <<-\EOF &&
	three
	two
	one
	EOF
	test_cmp expect actual &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})"
'

test_expect_success 'reuses the message of a fixup! commit in the range' '
	git reset --hard start &&
	test_commit --no-tag reg1 file b &&
	git commit --allow-empty -m "fixup! reg1" &&
	test_commit reg2 file c &&

	git history squash start.. &&

	git log --format="%s" -1 >actual &&
	echo reg1 >expect &&
	test_cmp expect actual
'

test_expect_success 'keeps the oldest message even if it is a fixup!' '
	git reset --hard start &&
	test_commit --no-tag "fixup! something" file b &&
	test_commit tail file c &&

	git history squash start.. &&

	git log --format="%s" -1 >actual &&
	echo "fixup! something" >expect &&
	test_cmp expect actual
'

test_expect_success 'preserves authorship of the oldest commit' '
	git reset --hard start &&
	GIT_AUTHOR_NAME=Squasher GIT_AUTHOR_EMAIL=squash@example.com \
		test_commit --no-tag oldest file b &&
	test_commit newest file c &&

	git history squash start.. &&

	git log -1 --format="%an <%ae>" >actual &&
	echo "Squasher <squash@example.com>" >expect &&
	test_cmp expect actual
'

test_expect_success '--dry-run predicts the rewrite without performing it' '
	git reset --hard three &&
	head_before=$(git rev-parse HEAD) &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash --dry-run start.. >out &&
	predicted=$(awk "/^update refs\/heads\// {print \$3}" out) &&
	test_cmp_rev "$head_before" HEAD &&

	git history squash start.. &&
	test "$predicted" = "$(git rev-parse HEAD)" &&
	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test_cmp_rev start HEAD^ &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})"
'

test_expect_success '--update-refs=head only moves HEAD' '
	git reset --hard three &&
	git branch -f other HEAD &&
	other_before=$(git rev-parse other) &&

	git history squash --update-refs=head start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test_cmp_rev "$other_before" other
'

test_expect_success 'refuses to fold a range a ref points into' '
	git reset --hard three &&
	git branch -f mid HEAD~1 &&
	head_before=$(git rev-parse HEAD) &&

	test_must_fail git history squash start.. 2>err &&
	test_grep "error: .* points into the squashed range" err &&
	test_grep "hint: .*--update-refs=head" err &&
	test_cmp_rev "$head_before" HEAD &&

	git branch -D mid
'

test_expect_success 'advice.historyUpdateRefs silences the hint' '
	git reset --hard three &&
	git branch -f mid HEAD~1 &&

	test_must_fail git -c advice.historyUpdateRefs=false \
		history squash start.. 2>err &&
	test_grep "points into the squashed range" err &&
	test_grep ! "hint:" err &&

	git branch -D mid
'

test_expect_success '--update-refs=head folds past a ref pointing into the range' '
	git reset --hard three &&
	git branch -f mid HEAD~1 &&
	mid_before=$(git rev-parse mid) &&

	git history squash --update-refs=head start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test_cmp_rev "$mid_before" mid &&

	git branch -D mid
'

test_expect_success 'refuses to fold a range a tag points into' '
	git reset --hard three &&
	git tag -f mark HEAD~1 &&
	head_before=$(git rev-parse HEAD) &&

	test_must_fail git history squash start.. 2>err &&
	test_grep "refs/tags/mark" err &&
	test_grep "points into the squashed range" err &&
	test_cmp_rev "$head_before" HEAD &&

	git tag -d mark
'

test_expect_success 'squashes a range whose internal merge has a single base' '
	git reset --hard start &&
	test_commit --no-tag before-side file b &&
	git checkout -b inner-side &&
	test_commit --no-tag on-inner-side inner x &&
	git checkout - &&
	test_commit --no-tag after-side file c &&
	git merge --no-ff -m merge inner-side &&
	git branch -D inner-side &&
	test_commit --no-tag after-merge file d &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	git log --format="%s" -1 >subject &&
	echo before-side >expect &&
	test_cmp expect subject &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})" &&
	test_path_is_file inner
'

test_expect_success 'folds a merge of a branch that forked at the base' '
	git reset --hard start &&
	git checkout -b base-fork-side &&
	test_commit --no-tag base-fork-side side x &&
	git checkout - &&
	test_commit --no-tag base-fork-main file b &&
	git merge --no-ff -m "merge base-fork-side" base-fork-side &&
	git branch -D base-fork-side &&
	test_commit --no-tag base-fork-tail file c &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test_cmp_rev start HEAD^ &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})" &&
	test_path_is_file side
'

test_expect_success 'folds a range whose tip is a merge commit' '
	git reset --hard start &&
	test_commit --no-tag tipmerge-base file b &&
	git checkout -b tipmerge-side &&
	test_commit --no-tag tipmerge-side side x &&
	git checkout - &&
	test_commit --no-tag tipmerge-main file c &&
	git merge --no-ff -m "merge tipmerge-side" tipmerge-side &&
	git branch -D tipmerge-side &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})" &&
	test_path_is_file side
'

test_expect_success 'folds a range whose base is a merge commit' '
	git reset --hard start &&
	git checkout -b basemerge-side &&
	test_commit --no-tag basemerge-side side x &&
	git checkout - &&
	test_commit --no-tag basemerge-main file b &&
	git merge --no-ff -m "merge basemerge-side" basemerge-side &&
	git branch -D basemerge-side &&
	base=$(git rev-parse HEAD) &&
	test_commit --no-tag basemerge-one file c &&
	test_commit --no-tag basemerge-two file d &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash "$base.." &&

	git rev-list --count "$base..HEAD" >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test_cmp_rev "$base" HEAD^ &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})"
'

test_expect_success 'refuses to squash a range with more than one base' '
	git reset --hard start &&
	head_before=$(git rev-parse HEAD) &&
	git checkout -b forked-before &&
	test_commit forked-side fside x &&
	git checkout - &&
	test_commit forked-main file b &&
	git merge --no-ff -m merge forked-before &&
	merged=$(git rev-parse HEAD) &&

	test_must_fail git history squash forked-main.. 2>err &&
	test_grep "more than one base" err &&
	test_cmp_rev "$merged" HEAD
'

test_expect_success 'folds a range with two interior merges' '
	git reset --hard start &&
	test_commit --no-tag two-merge-a file a1 &&
	git checkout -b two-merge-s1 &&
	test_commit --no-tag two-merge-s1 s1 x &&
	git checkout - &&
	git merge --no-ff -m "merge s1" two-merge-s1 &&
	test_commit --no-tag two-merge-b file b1 &&
	git checkout -b two-merge-s2 &&
	test_commit --no-tag two-merge-s2 s2 y &&
	git checkout - &&
	git merge --no-ff -m "merge s2" two-merge-s2 &&
	git branch -D two-merge-s1 two-merge-s2 &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})" &&
	test_path_is_file s1 &&
	test_path_is_file s2
'

test_expect_success 'folds a range with a nested merge' '
	git reset --hard start &&
	main=$(git symbolic-ref --short HEAD) &&
	git checkout -b nested-outer &&
	test_commit --no-tag nested-outer outer x &&
	git checkout -b nested-inner &&
	test_commit --no-tag nested-inner inner y &&
	git checkout nested-outer &&
	git merge --no-ff -m "merge inner" nested-inner &&
	git checkout "$main" &&
	test_commit --no-tag nested-main file b1 &&
	git merge --no-ff -m "merge outer" nested-outer &&
	git branch -D nested-outer nested-inner &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})" &&
	test_path_is_file outer &&
	test_path_is_file inner
'

test_expect_success 'folds a range with an octopus merge' '
	git reset --hard start &&
	main=$(git symbolic-ref --short HEAD) &&
	test_commit --no-tag octo-base file a1 &&
	git checkout -b octo-1 &&
	test_commit --no-tag octo-1 o1 x &&
	git checkout "$main" &&
	git checkout -b octo-2 &&
	test_commit --no-tag octo-2 o2 y &&
	git checkout "$main" &&
	git merge --no-ff -m octopus octo-1 octo-2 &&
	git branch -D octo-1 octo-2 &&
	tip_tree=$(git rev-parse HEAD^{tree}) &&

	git history squash start.. &&

	git rev-list --count start..HEAD >count &&
	echo 1 >expect &&
	test_cmp expect count &&
	test "$tip_tree" = "$(git rev-parse HEAD^{tree})" &&
	test_path_is_file o1 &&
	test_path_is_file o2
'

test_expect_success 'refuses an octopus merge with an arm forked before the base' '
	git reset --hard start &&
	main=$(git symbolic-ref --short HEAD) &&
	git checkout -b octo-pre &&
	test_commit octo-pre-side pside x &&
	git checkout "$main" &&
	test_commit octo-pre-main file b1 &&
	octo_base=$(git rev-parse HEAD) &&
	git checkout -b octo-within &&
	test_commit --no-tag octo-within wside y &&
	git checkout "$main" &&
	git merge --no-ff -m octopus octo-pre octo-within &&
	merged=$(git rev-parse HEAD) &&
	git branch -D octo-pre octo-within &&

	test_must_fail git history squash "$octo_base.." 2>err &&
	test_grep "more than one base" err &&
	test_cmp_rev "$merged" HEAD
'

test_expect_success 'refuses when a descendant above the range is a merge' '
	git reset --hard start &&
	main=$(git symbolic-ref --short HEAD) &&
	test_commit --no-tag desc-base file b &&
	git tag desc-tip &&
	git checkout -b desc-above &&
	test_commit --no-tag desc-above above x &&
	git checkout "$main" &&
	test_commit --no-tag desc-main file c &&
	git merge --no-ff -m "merge desc-above" desc-above &&
	git branch -D desc-above &&
	head_before=$(git rev-parse HEAD) &&

	test_must_fail git history squash start..desc-tip 2>err &&
	test_grep "merge commits is not supported" err &&
	test_cmp_rev "$head_before" HEAD
'

test_expect_success 'refuses to fold a range a ref points into at a merge' '
	git reset --hard start &&
	main=$(git symbolic-ref --short HEAD) &&
	test_commit --no-tag refmerge-base file b &&
	git checkout -b refmerge-side &&
	test_commit --no-tag refmerge-side side x &&
	git checkout "$main" &&
	test_commit --no-tag refmerge-main file c &&
	git merge --no-ff -m "interior merge" refmerge-side &&
	git branch -D refmerge-side &&
	git branch at-merge HEAD &&
	test_commit --no-tag refmerge-tail file d &&
	head_before=$(git rev-parse HEAD) &&

	test_must_fail git history squash start.. 2>err &&
	test_grep "at-merge" err &&
	test_grep "points into the squashed range" err &&
	test_cmp_rev "$head_before" HEAD &&

	git branch -D at-merge
'

test_done
