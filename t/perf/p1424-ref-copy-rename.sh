#!/bin/sh

test_description='Copy and rename references with short and long reflogs'

. ./perf-lib.sh

test_perf_fresh_repo

for entries in 10 10000
do
	export entries
	test_expect_success "setup $entries reflog entries" '
		git init --ref-format=files "repo-$entries" &&
		(
			cd "repo-$entries" &&
			test_commit A &&
			git branch source &&
			oid=$(git rev-parse HEAD) &&
			awk -v oid="$oid" -v count="$entries" '\''
				BEGIN {
					for (i = 0; i < count; i++)
						printf "%s %s A <a@example.com> %d +0000\tentry %d\n", oid, oid, 1700000000 + i, i
				}
			'\'' >.git/logs/refs/heads/source &&
			if test "$GIT_TEST_DEFAULT_REF_FORMAT" = reftable
			then
				git refs migrate --ref-format=reftable
			fi
		)
	'

	test_perf "copy $entries reflog entries (10 operations)" '
		for i in $(test_seq 10)
		do
			git -C "repo-$entries" branch -C source destination || return 1
		done
	'

	test_perf "rename $entries reflog entries (10 operations)" '
		for i in $(test_seq 5)
		do
			git -C "repo-$entries" branch -m source renamed &&
			git -C "repo-$entries" branch -m renamed source || return 1
		done
	'
done

test_done
