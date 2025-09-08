#!/bin/sh

test_description='submodules handle mixed legacy and new (encoded) style gitdir paths'

. ./test-lib.sh

test_expect_success 'setup: allow file protocol' '
	git config --global protocol.file.allow always
'

test_expect_success 'create repo with mixed new and legacy submodules' '
	git init -b main legacy-sub &&
	test_commit -C legacy-sub legacy-initial &&
	git -C legacy-sub config receive.denyCurrentBranch updateInstead &&
	legacy_rev=$(git -C legacy-sub rev-parse HEAD) &&

	git init -b main new-sub &&
	test_commit -C new-sub new-initial &&
	git -C new-sub config receive.denyCurrentBranch updateInstead &&
	new_rev=$(git -C new-sub rev-parse HEAD) &&

	git init -b main main &&
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
		test_grep "$legacy_rev legacy" list &&
		test_grep "$new_rev new" list
	)
'

test_expect_success 'commit and push changes to submodules' '
	(
		cd cloned &&

		git -C legacy switch --track -C main origin/main  &&
		test_commit -C legacy second-commit &&
		git -C legacy push &&

		git -C new switch --track -C main origin/main &&
		test_commit -C new second-commit &&
		git -C new push &&

		# Stage and commit submodule changes in superproject
		git switch --track -C main origin/main  &&
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
		test_grep "$legacy_rev legacy" list &&
		test_grep "$new_rev new" list
	)
'

test_done
