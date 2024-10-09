#!/bin/sh

test_description='Test reffiles backend consistency check'

GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME
GIT_TEST_DEFAULT_REF_FORMAT=files
export GIT_TEST_DEFAULT_REF_FORMAT
TEST_PASSES_SANITIZE_LEAK=true

. ./test-lib.sh

test_expect_success 'ref name should be checked' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&

	git commit --allow-empty -m initial &&
	git checkout -b branch-1 &&
	git tag tag-1 &&
	git commit --allow-empty -m second &&
	git checkout -b branch-2 &&
	git tag tag-2 &&
	git tag multi_hierarchy/tag-2 &&

	cp $branch_dir_prefix/branch-1 $branch_dir_prefix/.branch-1 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/heads/.branch-1: badRefName: invalid refname format
	EOF
	rm $branch_dir_prefix/.branch-1 &&
	test_cmp expect err &&

	cp $branch_dir_prefix/branch-1 $branch_dir_prefix/@ &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/heads/@: badRefName: invalid refname format
	EOF
	rm $branch_dir_prefix/@ &&
	test_cmp expect err &&

	cp $tag_dir_prefix/multi_hierarchy/tag-2 $tag_dir_prefix/multi_hierarchy/@ &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/tags/multi_hierarchy/@: badRefName: invalid refname format
	EOF
	rm $tag_dir_prefix/multi_hierarchy/@ &&
	test_cmp expect err &&

	cp $tag_dir_prefix/tag-1 $tag_dir_prefix/tag-1.lock &&
	git refs verify 2>err &&
	rm $tag_dir_prefix/tag-1.lock &&
	test_must_be_empty err &&

	cp $tag_dir_prefix/tag-1 $tag_dir_prefix/.lock &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/tags/.lock: badRefName: invalid refname format
	EOF
	rm $tag_dir_prefix/.lock &&
	test_cmp expect err
'

test_expect_success 'ref name check should be adapted into fsck messages' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&
	git commit --allow-empty -m initial &&
	git checkout -b branch-1 &&
	git tag tag-1 &&
	git commit --allow-empty -m second &&
	git checkout -b branch-2 &&
	git tag tag-2 &&

	cp $branch_dir_prefix/branch-1 $branch_dir_prefix/.branch-1 &&
	git -c fsck.badRefName=warn refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/.branch-1: badRefName: invalid refname format
	EOF
	rm $branch_dir_prefix/.branch-1 &&
	test_cmp expect err &&

	cp $branch_dir_prefix/branch-1 $branch_dir_prefix/@ &&
	git -c fsck.badRefName=ignore refs verify 2>err &&
	test_must_be_empty err
'

test_expect_success 'ref name check should work for multiple worktrees' '
	test_when_finished "rm -rf repo" &&
	git init repo &&

	cd repo &&
	test_commit initial &&
	git checkout -b branch-1 &&
	test_commit second &&
	git checkout -b branch-2 &&
	test_commit third &&
	git checkout -b branch-3 &&
	git worktree add ./worktree-1 branch-1 &&
	git worktree add ./worktree-2 branch-2 &&
	worktree1_refdir_prefix=.git/worktrees/worktree-1/refs/worktree &&
	worktree2_refdir_prefix=.git/worktrees/worktree-2/refs/worktree &&

	(
		cd worktree-1 &&
		git update-ref refs/worktree/branch-4 refs/heads/branch-3
	) &&
	(
		cd worktree-2 &&
		git update-ref refs/worktree/branch-4 refs/heads/branch-3
	) &&

	cp $worktree1_refdir_prefix/branch-4 $worktree1_refdir_prefix/.branch-2 &&
	cp $worktree2_refdir_prefix/branch-4 $worktree2_refdir_prefix/@ &&

	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/worktree/.branch-2: badRefName: invalid refname format
	error: refs/worktree/@: badRefName: invalid refname format
	EOF
	sort err >sorted_err &&
	test_cmp expect sorted_err &&

	(
		cd worktree-1 &&
		test_must_fail git refs verify 2>err &&
		cat >expect <<-EOF &&
		error: refs/worktree/.branch-2: badRefName: invalid refname format
		error: refs/worktree/@: badRefName: invalid refname format
		EOF
		sort err >sorted_err &&
		test_cmp expect sorted_err
	) &&

	(
		cd worktree-2 &&
		test_must_fail git refs verify 2>err &&
		cat >expect <<-EOF &&
		error: refs/worktree/.branch-2: badRefName: invalid refname format
		error: refs/worktree/@: badRefName: invalid refname format
		EOF
		sort err >sorted_err &&
		test_cmp expect sorted_err
	)
'

test_expect_success 'regular ref content should be checked (individual)' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&
	test_commit default &&
	mkdir -p "$branch_dir_prefix/a/b" &&

	git refs verify 2>err &&
	test_must_be_empty err &&

	bad_content=$(git rev-parse main)x &&
	printf "%s" $bad_content >$tag_dir_prefix/tag-bad-1 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/tags/tag-bad-1: badRefContent: $bad_content
	EOF
	rm $tag_dir_prefix/tag-bad-1 &&
	test_cmp expect err &&

	bad_content=xfsazqfxcadas &&
	printf "%s" $bad_content >$tag_dir_prefix/tag-bad-2 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/tags/tag-bad-2: badRefContent: $bad_content
	EOF
	rm $tag_dir_prefix/tag-bad-2 &&
	test_cmp expect err &&

	bad_content=Xfsazqfxcadas &&
	printf "%s" $bad_content >$branch_dir_prefix/a/b/branch-bad &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/heads/a/b/branch-bad: badRefContent: $bad_content
	EOF
	rm $branch_dir_prefix/a/b/branch-bad &&
	test_cmp expect err &&

	printf "%s" "$(git rev-parse main)" >$branch_dir_prefix/branch-no-newline &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/branch-no-newline: unofficialFormattedRef: misses LF at the end
	EOF
	rm $branch_dir_prefix/branch-no-newline &&
	test_cmp expect err &&

	printf "%s garbage" "$(git rev-parse main)" >$branch_dir_prefix/branch-garbage &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/branch-garbage: unofficialFormattedRef: has trailing garbage: '\'' garbage'\''
	EOF
	rm $branch_dir_prefix/branch-garbage &&
	test_cmp expect err &&

	printf "%s\n\n\n" "$(git rev-parse main)" >$tag_dir_prefix/tag-garbage-1 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/tags/tag-garbage-1: unofficialFormattedRef: has trailing garbage: '\''


	'\''
	EOF
	rm $tag_dir_prefix/tag-garbage-1 &&
	test_cmp expect err &&

	printf "%s\n\n\n  garbage" "$(git rev-parse main)" >$tag_dir_prefix/tag-garbage-2 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/tags/tag-garbage-2: unofficialFormattedRef: has trailing garbage: '\''


	  garbage'\''
	EOF
	rm $tag_dir_prefix/tag-garbage-2 &&
	test_cmp expect err &&

	printf "%s    garbage\na" "$(git rev-parse main)" >$tag_dir_prefix/tag-garbage-3 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/tags/tag-garbage-3: unofficialFormattedRef: has trailing garbage: '\''    garbage
	a'\''
	EOF
	rm $tag_dir_prefix/tag-garbage-3 &&
	test_cmp expect err &&

	printf "%s garbage" "$(git rev-parse main)" >$tag_dir_prefix/tag-garbage-4 &&
	test_must_fail git -c fsck.unofficialFormattedRef=error refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/tags/tag-garbage-4: unofficialFormattedRef: has trailing garbage: '\'' garbage'\''
	EOF
	rm $tag_dir_prefix/tag-garbage-4 &&
	test_cmp expect err
'

test_expect_success 'regular ref content should be checked (aggregate)' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&
	test_commit default &&
	mkdir -p "$branch_dir_prefix/a/b" &&

	bad_content_1=$(git rev-parse main)x &&
	bad_content_2=xfsazqfxcadas &&
	bad_content_3=Xfsazqfxcadas &&
	printf "%s" $bad_content_1 >$tag_dir_prefix/tag-bad-1 &&
	printf "%s" $bad_content_2 >$tag_dir_prefix/tag-bad-2 &&
	printf "%s" $bad_content_3 >$branch_dir_prefix/a/b/branch-bad &&
	printf "%s" "$(git rev-parse main)" >$branch_dir_prefix/branch-no-newline &&
	printf "%s garbage" "$(git rev-parse main)" >$branch_dir_prefix/branch-garbage &&

	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/heads/a/b/branch-bad: badRefContent: $bad_content_3
	error: refs/tags/tag-bad-1: badRefContent: $bad_content_1
	error: refs/tags/tag-bad-2: badRefContent: $bad_content_2
	warning: refs/heads/branch-garbage: unofficialFormattedRef: has trailing garbage: '\'' garbage'\''
	warning: refs/heads/branch-no-newline: unofficialFormattedRef: misses LF at the end
	EOF
	sort err >sorted_err &&
	test_cmp expect sorted_err
'

test_expect_success 'textual symref content should be checked (individual)' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&
	test_commit default &&
	mkdir -p "$branch_dir_prefix/a/b" &&

	printf "ref: refs/heads/branch\n" >$branch_dir_prefix/branch-good &&
	git refs verify 2>err &&
	rm $branch_dir_prefix/branch-good &&
	test_must_be_empty err &&

	printf "ref: refs/heads/branch" >$branch_dir_prefix/branch-no-newline-1 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/branch-no-newline-1: unofficialFormattedRef: misses LF at the end
	EOF
	rm $branch_dir_prefix/branch-no-newline-1 &&
	test_cmp expect err &&

	printf "ref: refs/heads/branch     " >$branch_dir_prefix/a/b/branch-trailing-1 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/a/b/branch-trailing-1: unofficialFormattedRef: misses LF at the end
	warning: refs/heads/a/b/branch-trailing-1: unofficialFormattedRef: has trailing whitespaces or newlines
	EOF
	rm $branch_dir_prefix/a/b/branch-trailing-1 &&
	test_cmp expect err &&

	printf "ref: refs/heads/branch\n\n" >$branch_dir_prefix/a/b/branch-trailing-2 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/a/b/branch-trailing-2: unofficialFormattedRef: has trailing whitespaces or newlines
	EOF
	rm $branch_dir_prefix/a/b/branch-trailing-2 &&
	test_cmp expect err &&

	printf "ref: refs/heads/branch \n" >$branch_dir_prefix/a/b/branch-trailing-3 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/a/b/branch-trailing-3: unofficialFormattedRef: has trailing whitespaces or newlines
	EOF
	rm $branch_dir_prefix/a/b/branch-trailing-3 &&
	test_cmp expect err &&

	printf "ref: refs/heads/branch \n  " >$branch_dir_prefix/a/b/branch-complicated &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/a/b/branch-complicated: unofficialFormattedRef: misses LF at the end
	warning: refs/heads/a/b/branch-complicated: unofficialFormattedRef: has trailing whitespaces or newlines
	EOF
	rm $branch_dir_prefix/a/b/branch-complicated &&
	test_cmp expect err &&

	printf "ref: refs/heads/.branch\n" >$branch_dir_prefix/branch-bad-1 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/heads/branch-bad-1: badReferent: points to invalid refname '\''refs/heads/.branch'\''
	EOF
	rm $branch_dir_prefix/branch-bad-1 &&
	test_cmp expect err
'

test_expect_success 'textual symref content should be checked (aggregate)' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&
	test_commit default &&
	mkdir -p "$branch_dir_prefix/a/b" &&

	printf "ref: refs/heads/branch\n" >$branch_dir_prefix/branch-good &&
	printf "ref: refs/heads/branch" >$branch_dir_prefix/branch-no-newline-1 &&
	printf "ref: refs/heads/branch     " >$branch_dir_prefix/a/b/branch-trailing-1 &&
	printf "ref: refs/heads/branch\n\n" >$branch_dir_prefix/a/b/branch-trailing-2 &&
	printf "ref: refs/heads/branch \n" >$branch_dir_prefix/a/b/branch-trailing-3 &&
	printf "ref: refs/heads/branch \n  " >$branch_dir_prefix/a/b/branch-complicated &&
	printf "ref: refs/heads/.branch\n" >$branch_dir_prefix/branch-bad-1 &&

	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/heads/branch-bad-1: badReferent: points to invalid refname '\''refs/heads/.branch'\''
	warning: refs/heads/a/b/branch-complicated: unofficialFormattedRef: has trailing whitespaces or newlines
	warning: refs/heads/a/b/branch-complicated: unofficialFormattedRef: misses LF at the end
	warning: refs/heads/a/b/branch-trailing-1: unofficialFormattedRef: has trailing whitespaces or newlines
	warning: refs/heads/a/b/branch-trailing-1: unofficialFormattedRef: misses LF at the end
	warning: refs/heads/a/b/branch-trailing-2: unofficialFormattedRef: has trailing whitespaces or newlines
	warning: refs/heads/a/b/branch-trailing-3: unofficialFormattedRef: has trailing whitespaces or newlines
	warning: refs/heads/branch-no-newline-1: unofficialFormattedRef: misses LF at the end
	EOF
	sort err >sorted_err &&
	test_cmp expect sorted_err
'

test_expect_success 'textual symref should be checked whether it is escaped' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&
	test_commit default &&
	mkdir -p "$branch_dir_prefix/a/b" &&

	printf "ref: refs-back/heads/main\n" >$branch_dir_prefix/branch-bad-1 &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/branch-bad-1: escapeReferent: referent '\''refs-back/heads/main'\'' is outside of refs/ or worktrees/
	EOF
	rm $branch_dir_prefix/branch-bad-1 &&
	test_cmp expect err
'

test_expect_success 'textual symref escape check should work with worktrees' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	cd repo &&
	test_commit default &&
	git branch branch-1 &&
	git branch branch-2 &&
	git branch branch-3 &&
	git worktree add ./worktree-1 branch-2 &&
	git worktree add ./worktree-2 branch-3 &&

	(
		cd worktree-1 &&
		git branch refs/worktree/w1-branch &&
		git symbolic-ref refs/worktree/branch-4 refs/heads/branch-1 &&
		git symbolic-ref refs/worktree/branch-5 worktrees/worktree-2/refs/worktree/w2-branch
	) &&
	(
		cd worktree-2 &&
		git branch refs/worktree/w2-branch &&
		git symbolic-ref refs/worktree/branch-4 refs/heads/branch-1 &&
		git symbolic-ref refs/worktree/branch-5 worktrees/worktree-1/refs/worktree/w1-branch
	) &&


	git symbolic-ref refs/heads/branch-5 worktrees/worktree-1/refs/worktree/w1-branch &&
	git symbolic-ref refs/heads/branch-6 worktrees/worktree-2/refs/worktree/w2-branch &&

	git refs verify 2>err &&
	test_must_be_empty err
'

test_expect_success 'all textual symref checks should work with worktrees' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	cd repo &&
	test_commit default &&
	git branch branch-1 &&
	git branch branch-2 &&
	git branch branch-3 &&
	git worktree add ./worktree-1 branch-2 &&
	git worktree add ./worktree-2 branch-3 &&
	worktree1_refdir_prefix=.git/worktrees/worktree-1/refs/worktree &&
	worktree2_refdir_prefix=.git/worktrees/worktree-2/refs/worktree &&

	(
		cd worktree-1 &&
		git update-ref refs/worktree/branch-4 refs/heads/branch-1
	) &&
	(
		cd worktree-2 &&
		git update-ref refs/worktree/branch-4 refs/heads/branch-1
	) &&

	bad_content_1=$(git rev-parse HEAD)x &&
	bad_content_2=xfsazqfxcadas &&
	bad_content_3=Xfsazqfxcadas &&

	printf "%s" $bad_content_1 >$worktree1_refdir_prefix/bad-branch-1 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/worktree/bad-branch-1: badRefContent: $bad_content_1
	EOF
	rm $worktree1_refdir_prefix/bad-branch-1 &&
	test_cmp expect err &&

	printf "%s" $bad_content_2 >$worktree2_refdir_prefix/bad-branch-2 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/worktree/bad-branch-2: badRefContent: $bad_content_2
	EOF
	rm $worktree2_refdir_prefix/bad-branch-2 &&
	test_cmp expect err &&

	printf "%s" $bad_content_3 >$worktree1_refdir_prefix/bad-branch-3 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	error: refs/worktree/bad-branch-3: badRefContent: $bad_content_3
	EOF
	rm $worktree1_refdir_prefix/bad-branch-3 &&
	test_cmp expect err &&

	printf "%s" "$(git rev-parse HEAD)" >$worktree1_refdir_prefix/branch-no-newline &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/worktree/branch-no-newline: unofficialFormattedRef: misses LF at the end
	EOF
	rm $worktree1_refdir_prefix/branch-no-newline &&
	test_cmp expect err &&

	printf "%s garbage" "$(git rev-parse HEAD)" >$worktree2_refdir_prefix/branch-garbage &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/worktree/branch-garbage: unofficialFormattedRef: has trailing garbage: '\'' garbage'\''
	EOF
	rm $worktree2_refdir_prefix/branch-garbage
'

test_expect_success SYMLINKS 'symlink symref content should be checked (individual)' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	branch_dir_prefix=.git/refs/heads &&
	tag_dir_prefix=.git/refs/tags &&
	cd repo &&
	test_commit default &&
	mkdir -p "$branch_dir_prefix/a/b" &&

	ln -sf ./main $branch_dir_prefix/branch-symbolic-good &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/branch-symbolic-good: symlinkRef: use deprecated symbolic link for symref
	EOF
	rm $branch_dir_prefix/branch-symbolic-good &&
	test_cmp expect err &&

	ln -sf ../../logs/branch-escape $branch_dir_prefix/branch-symbolic &&
	git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/branch-symbolic: symlinkRef: use deprecated symbolic link for symref
	warning: refs/heads/branch-symbolic: escapeReferent: referent '\''logs/branch-escape'\'' is outside of refs/ or worktrees/
	EOF
	rm $branch_dir_prefix/branch-symbolic &&
	test_cmp expect err &&

	ln -sf ./"branch   space" $branch_dir_prefix/branch-symbolic-bad &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/heads/branch-symbolic-bad: symlinkRef: use deprecated symbolic link for symref
	error: refs/heads/branch-symbolic-bad: badReferent: points to invalid refname '\''refs/heads/branch   space'\''
	EOF
	rm $branch_dir_prefix/branch-symbolic-bad &&
	test_cmp expect err &&

	ln -sf ./".tag" $tag_dir_prefix/tag-symbolic-1 &&
	test_must_fail git refs verify 2>err &&
	cat >expect <<-EOF &&
	warning: refs/tags/tag-symbolic-1: symlinkRef: use deprecated symbolic link for symref
	error: refs/tags/tag-symbolic-1: badReferent: points to invalid refname '\''refs/tags/.tag'\''
	EOF
	rm $tag_dir_prefix/tag-symbolic-1 &&
	test_cmp expect err
'

test_done
