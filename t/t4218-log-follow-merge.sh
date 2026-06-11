#!/bin/sh

test_description='Test --follow follows renames across merges'

GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=master
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh

test_expect_success 'setup subtree-merged repository' '
	git init inner &&
	echo inner >inner/inner.txt &&
	git -C inner add inner.txt &&
	git -C inner commit -m "inner init" &&

	git init outer &&
	echo outer >outer/outer.txt &&
	git -C outer add outer.txt &&
	git -C outer commit -m "outer init" &&

	git -C outer fetch ../inner master &&
	git -C outer merge -s ours --no-commit --allow-unrelated-histories \
		FETCH_HEAD &&
	git -C outer read-tree --prefix=inner/ -u FETCH_HEAD &&
	git -C outer commit -m "Merge inner repo into inner/ subdirectory"
'

test_expect_success '--follow finds the pre-merge commit through a subtree merge' '
	git -C outer log --follow --pretty=tformat:%s inner/inner.txt >actual &&
	echo "inner init" >expect &&
	test_cmp expect actual
'

test_expect_success 'setup merge of two branches that both renamed a file to README' '
	git init foo &&
	mkdir foo/foo &&
	echo "foo readme" >foo/foo/README &&
	git -C foo add foo/README &&
	git -C foo commit -m "add foo README" &&

	git -C foo mv foo/README README &&
	git -C foo commit -m "promote foo README to toplevel" &&

	echo "foo c" >foo/foo.c &&
	git -C foo add foo.c &&
	git -C foo commit -m "add foo C impl" &&

	git init bar &&
	mkdir bar/bar &&
	echo "bar readme" >bar/bar/README &&
	git -C bar add bar/README &&
	git -C bar commit -m "add bar README" &&

	git -C bar mv bar/README README &&
	git -C bar commit -m "promote bar README to toplevel" &&

	echo "bar c" >bar/bar.c &&
	git -C bar add bar.c &&
	git -C bar commit -m "add bar C impl" &&

	git -C foo fetch ../bar master &&
	git -C foo merge -s ours --no-commit --allow-unrelated-histories \
		FETCH_HEAD &&
	git -C foo checkout FETCH_HEAD -- bar.c &&
	git -C foo commit -m "merge bar into foo"
'

test_expect_success '--follow follows renames across both sides of a merge' '
	git -C foo log --follow --pretty=tformat:%s README >actual &&
	sort actual >actual.sorted &&
	cat >expect <<-\EOF &&
	add bar README
	add foo README
	promote bar README to toplevel
	promote foo README to toplevel
	EOF
	test_cmp expect actual.sorted
'

# When two branches rename a different file to the same name and then meet again
# in a merge, log --follow needs to keep track both paths.
test_expect_success 'setup criss-cross merge where two paths converge in ancestor' '
	git init crisscross &&
	echo "alpha content" >crisscross/alpha.txt &&
	git -C crisscross add alpha.txt &&
	git -C crisscross commit -m "root: add alpha.txt" &&

	echo "beta content" >crisscross/beta.txt &&
	git -C crisscross add beta.txt &&
	git -C crisscross commit -m "fork: add beta.txt" &&

	git -C crisscross checkout -b branchB &&
	git -C crisscross mv alpha.txt combined.txt &&
	git -C crisscross rm beta.txt &&
	git -C crisscross commit -m "B: rename alpha to combined" &&

	git -C crisscross checkout master &&
	git -C crisscross checkout -b branchC &&
	git -C crisscross mv beta.txt combined.txt &&
	git -C crisscross rm alpha.txt &&
	git -C crisscross commit -m "C: rename beta to combined" &&

	git -C crisscross checkout branchB &&
	git -C crisscross merge -s ours -m "merge C into B" branchC
'

test_expect_success '--follow follows two diverged paths past their common ancestor' '
	git -C crisscross log --follow --pretty=tformat:%s combined.txt >actual &&
	sort actual >actual.sorted &&
	cat >expect <<-\EOF &&
	B: rename alpha to combined
	C: rename beta to combined
	fork: add beta.txt
	root: add alpha.txt
	EOF
	test_cmp expect actual.sorted
'

test_done
