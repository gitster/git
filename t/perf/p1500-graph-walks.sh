#!/bin/sh

test_description='Commit walk performance tests'
. ./perf-lib.sh

test_perf_large_repo

test_expect_success 'setup' '
	git for-each-ref --format="%(refname)" "refs/heads/*" "refs/tags/*" >allrefs &&
	sort -r allrefs | head -n 50 >refs &&
	for ref in $(cat refs)
	do
		git branch -f ref-$ref $ref &&
		echo ref-$ref ||
		return 1
	done >branches &&
	for ref in $(cat refs)
	do
		git tag -f tag-$ref $ref &&
		echo tag-$ref ||
		return 1
	done >tags &&

	echo "A:HEAD" >test-tool-refs &&
	for line in $(cat refs)
	do
		echo "X:$line" >>test-tool-refs || return 1
	done &&
	echo "A:HEAD" >test-tool-tags &&
	for line in $(cat tags)
	do
		echo "X:$line" >>test-tool-tags || return 1
	done &&

	git rev-list --first-parent --max-count=8192 HEAD >contains-commits &&
	test_file_not_empty contains-commits &&
	git update-ref refs/contains-perf-base "$(tail -n 1 contains-commits)" &&
	awk "{
		printf \"update refs/contains-perf/%04d %s\\n\", NR, \$1
	}" contains-commits |
		git update-ref --stdin &&
	git pack-refs --include "refs/contains-perf/*" &&

	tree=$(git rev-parse HEAD^{tree}) &&
	base=$(git rev-parse HEAD) &&
	target=$(echo target | git commit-tree "$tree" -p "$base") &&
	git update-ref refs/contains-diverged/target "$target" &&
	for i in $(test_seq 1 4)
	do
		commit=$(echo candidate-$i |
			git commit-tree "$tree" -p "$base") &&
		git update-ref refs/contains-diverged/candidate-$i "$commit" ||
		return 1
	done &&

	commit=$(git commit-tree "$tree") &&
	git update-ref refs/heads/disjoint-base $commit &&

	git commit-graph write --reachable
'

test_expect_success 'verify contains results' '
	git for-each-ref --contains=refs/contains-perf-base \
		refs/contains-perf/ >actual &&
	test_line_count = $(wc -l <contains-commits) actual &&

	echo refs/contains-diverged/target >expect &&
	GIT_TEST_COMMIT_GRAPH=0 \
		git -c core.commitGraph=false for-each-ref \
			--format="%(refname)" \
			--contains=refs/contains-diverged/target \
			refs/contains-diverged/ >actual &&
	test_cmp expect actual
'

test_perf 'ahead-behind counts: git for-each-ref' '
	git for-each-ref --format="%(ahead-behind:HEAD)" --stdin <refs
'

test_perf 'ahead-behind counts: git branch' '
	xargs git branch -l --format="%(ahead-behind:HEAD)" <branches
'

test_perf 'ahead-behind counts: git tag' '
	xargs git tag -l --format="%(ahead-behind:HEAD)" <tags
'

test_perf 'contains: git for-each-ref --merged' '
	git for-each-ref --merged=HEAD --stdin <refs
'

test_perf 'contains: git branch --merged' '
	xargs git branch --merged=HEAD <branches
'

test_perf 'contains: git tag --merged' '
	xargs git tag --merged=HEAD <tags
'

test_perf 'contains: git for-each-ref --contains' '
	git for-each-ref --contains=refs/contains-perf-base \
		refs/contains-perf/ >/dev/null
'

test_perf 'contains without generations: divergent refs' '
	GIT_TEST_COMMIT_GRAPH=0 \
		git -c core.commitGraph=false for-each-ref \
			--contains=refs/contains-diverged/target \
			refs/contains-diverged/ >/dev/null
'

test_perf 'is-base check: test-tool reach (refs)' '
	test-tool reach get_branch_base_for_tip <test-tool-refs
'

test_perf 'is-base check: test-tool reach (tags)' '
	test-tool reach get_branch_base_for_tip <test-tool-tags
'

test_perf 'is-base check: git for-each-ref' '
	git for-each-ref --format="%(is-base:HEAD)" --stdin <refs
'

test_perf 'is-base check: git for-each-ref (disjoint-base)' '
	git for-each-ref --format="%(is-base:refs/heads/disjoint-base)" --stdin <refs
'

test_done
