#!/bin/sh

test_description='submodules handle mixed legacy and new (encoded) style gitdir paths'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-verify-submodule-gitdir-path.sh

test_expect_success 'setup: allow file protocol' '
	git config --global protocol.file.allow always
'

test_expect_success 'create repo with mixed new and legacy submodules' '
	git init legacy-sub &&
	test_commit -C legacy-sub legacy-initial &&
	git -C legacy-sub config receive.denyCurrentBranch updateInstead &&
	legacy_rev=$(git -C legacy-sub rev-parse HEAD) &&

	git init new-sub &&
	test_commit -C new-sub new-initial &&
	git -C new-sub config receive.denyCurrentBranch updateInstead &&
	new_rev=$(git -C new-sub rev-parse HEAD) &&

	git init main &&
	(
		cd main &&

		git config receive.denyCurrentBranch updateInstead &&

		git submodule add ../new-sub new &&
		test_commit new-sub &&

		git submodule add ../legacy-sub legacy &&
		test_commit legacy-sub &&

		# simulate legacy .git/modules path by moving submodule
		mkdir -p .git/modules &&
		mv .git/submodules/legacy .git/modules/ &&
		echo "gitdir: ../.git/modules/legacy" > legacy/.git
	)
'

test_expect_success 'clone from repo with both legacy and new-style submodules' '
	git clone --recurse-submodules main cloned &&
	(
		cd cloned &&

		# At this point, .git/modules/<name> should not exist as
		# submodules are checked out into the new path
		test_path_is_dir .git/submodules/legacy &&
		test_path_is_dir .git/submodules/new &&

		git submodule status >list &&
		grep "$legacy_rev legacy" list &&
		grep "$new_rev new" list
	)
'

test_expect_success 'commit and push changes to submodules' '
	(
		cd cloned &&

		git -C legacy switch --track -C master origin/master  &&
		test_commit -C legacy second-commit &&
		git -C legacy push &&

		git -C new switch --track -C master origin/master &&
		test_commit -C new second-commit &&
		git -C new push &&

		# Stage and commit submodule changes in superproject
		git switch --track -C master origin/master  &&
		git add legacy new &&
		git commit -m "update submodules" &&

		# push superproject commit to main repo
		git push
	) &&

	# update expected legacy & new submodule checksums
	legacy_rev=$(git -C legacy-sub rev-parse HEAD) &&
	new_rev=$(git -C new-sub rev-parse HEAD)
'

test_expect_success 'fetch mixed submodule changes and verify updates' '
	(
		cd main &&

		# only update submodules because superproject was
		# pushed into at the end of last test
		git submodule update --init --recursive &&

		test_path_is_dir .git/modules/legacy &&
		test_path_is_dir .git/submodules/new &&

		# Verify both submodules are at the expected commits
		git submodule status >list &&
		grep "$legacy_rev legacy" list &&
		grep "$new_rev new" list
	)
'

test_expect_success 'setup submodules with nested git dirs' '
	git init nested &&
	test_commit -C nested nested &&
	(
		cd nested &&
		cat >.gitmodules <<-EOF &&
		[submodule "hippo"]
			url = .
			path = thing1
		[submodule "hippo/hooks"]
			url = .
			path = thing2
		EOF
		git clone . thing1 &&
		git clone . thing2 &&
		git add .gitmodules thing1 thing2 &&
		test_tick &&
		git commit -m nested
	)
'

test_expect_success 'git dirs of sibling submodules must not be nested' '
	git clone --recurse-submodules nested clone_nested &&
	verify_submodule_gitdir_path clone_nested hippo submodules/hippo &&
	verify_submodule_gitdir_path clone_nested hippo/hooks submodules/hippo%2fhooks
'

test_expect_success 'submodule git dir nesting detection must work with parallel cloning' '
	git clone --recurse-submodules --jobs=2 nested clone_parallel &&
	verify_submodule_gitdir_path clone_nested hippo submodules/hippo &&
	verify_submodule_gitdir_path clone_nested hippo/hooks submodules/hippo%2fhooks
'

test_expect_success 'checkout -f --recurse-submodules must corectly handle nested gitdirs' '
	git clone nested clone_recursive_checkout &&
	(
		cd clone_recursive_checkout &&

		git submodule init &&
		git submodule update thing1 thing2 &&

		# simulate a malicious nested alternate which git should not follow
		mkdir -p .git/submodules/hippo/hooks/refs &&
		mkdir -p .git/submodules/hippo/hooks/objects/info &&
		echo "../../../../objects" >.git/submodules/hippo/hooks/objects/info/alternates &&
		echo "ref: refs/heads/master" >.git/submodules/hippo/hooks/HEAD &&

		git checkout -f --recurse-submodules HEAD
	) &&
	verify_submodule_gitdir_path clone_nested hippo submodules/hippo &&
	verify_submodule_gitdir_path clone_nested hippo/hooks submodules/hippo%2fhooks
'

test_expect_success 'new style submodule gitdir paths are properly encoded' '
	(
		cd main &&

		# add new-style submodule name containing /
		git submodule add ../new-sub foo/bar &&
		git commit -m "add foo/bar" &&

		# simulate existing legacy submodule name containing escaping char %
		git clone --separate-git-dir .git/modules/foo%bar ../legacy-sub foo%bar  &&
		cat >>.gitmodules <<-EOF &&
		[submodule "foo%bar"]
			path = foo%bar
			url = ../legacy-sub
		EOF
		git add .gitmodules &&
		git commit -m "add foo%bar" &&

		# add new style submodule name containing escaping char %
		git submodule add ../new-sub fooish%bar &&
		git commit -m "add fooish%bar" &&

		# add a mixed case submdule name
		git submodule add ../new-sub FooBar &&
		git commit -m "add FooBar" &&

		# add a reserved name on Windows
		git submodule add ../new-sub COM1 &&
		git commit -m "add COM1"
	) &&
	verify_submodule_gitdir_path main foo/bar submodules/foo%2fbar &&
	verify_submodule_gitdir_path main foo%bar modules/foo%bar &&
	verify_submodule_gitdir_path main fooish%bar submodules/fooish%25bar &&
	verify_submodule_gitdir_path main FooBar submodules/_foo_bar &&
	verify_submodule_gitdir_path main COM1 submodules/_c_o_m1
'

test_expect_success 'submodule encoded name exceeds max name limit' '
	(
		cd main &&

		# find the system NAME_MAX (fall back to 255 if unknown)
		name_max=$(getconf NAME_MAX . 2>/dev/null || echo 255) &&

		# each "%" char encodes to "%25" (3 chars), ensure we exceed NAME_MAX
		count=$((name_max + 10)) &&
		longname=$(printf "%%%0.s" $(seq 1 $count)) &&

		test_must_fail git submodule add ../new-sub "$longname"
	)
'

test_done
