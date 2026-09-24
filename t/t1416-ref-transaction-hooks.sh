#!/bin/sh

test_description='reference transaction hooks'

GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh

test_expect_success setup '
	test_commit PRE &&
	PRE_OID=$(git rev-parse PRE) &&
	test_commit POST &&
	POST_OID=$(git rev-parse POST)
'

test_expect_success 'hook gets old values for batched branch/tag deletion' '
	test_when_finished "rm -f actual" &&
	git branch to-delete PRE &&
	git tag delete-tag POST &&
	git pack-refs --all &&
	test_hook reference-transaction <<-\EOF &&
		if test "$1" = committed
		then
			# Ignore backend-internal zero-to-zero records.
			while read -r old new ref
			do
				case "$old" in
				*[!0]*)
					echo "$old $new $ref"
					;;
				esac
			done >>actual
		fi
	EOF
	cat >expect <<-EOF &&
		$PRE_OID $ZERO_OID refs/heads/to-delete
		$POST_OID $ZERO_OID refs/tags/delete-tag
	EOF
	git branch -D to-delete &&
	git tag -d delete-tag &&
	test_cmp expect actual
'

test_expect_success 'branch deletion rejects a concurrent update' '
	git branch delete-race PRE &&
	test_hook reference-transaction <<-\EOF &&
		marker=$(git rev-parse --git-path delete-race-once)
		if test "$1" = preparing && test ! -e "$marker"
		then
			>"$marker"
			git update-ref refs/heads/delete-race POST
		fi
		exit 0
	EOF
	test_must_fail git branch -D delete-race 2>err &&
	test_grep "is at $POST_OID but expected $PRE_OID" err &&
	test_cmp_rev POST refs/heads/delete-race
'

test_expect_success 'hook gets old values when pruning remote refs' '
	test_when_finished "rm -rf empty.git prune" &&
	git init --bare empty.git &&
	git init prune &&
	(
		cd prune &&
		git remote add origin ../empty.git &&
		git commit --allow-empty -m one &&
		one=$(git rev-parse HEAD) &&
		git commit --allow-empty -m two &&
		two=$(git rev-parse HEAD) &&
		git update-ref refs/remotes/origin/remote-prune-z "$one" &&
		git update-ref refs/remotes/origin/remote-prune-a "$two"
	) &&
	test_hook -C prune reference-transaction <<-\EOF &&
		if test "$1" = committed
		then
			# Ignore backend-internal zero-to-zero records.
			while read -r old new ref
			do
				case "$old" in
				*[!0]*)
					echo "$old $new $ref"
					;;
				esac
			done >>actual
		fi
	EOF
	(
		cd prune &&
		one=$(git rev-parse HEAD^) &&
		two=$(git rev-parse HEAD) &&
		git remote prune origin &&
		git update-ref refs/remotes/origin/fetch-prune "$one" &&
		git fetch --prune origin &&
		git update-ref refs/remotes/origin/atomic-prune "$one" &&
		git fetch --atomic --prune origin &&
		cat >expect <<-EOF &&
			$two $ZERO_OID refs/remotes/origin/remote-prune-a
			$one $ZERO_OID refs/remotes/origin/remote-prune-z
			$one $ZERO_OID refs/remotes/origin/fetch-prune
			$one $ZERO_OID refs/remotes/origin/atomic-prune
		EOF
		test_cmp expect actual
	)
'

test_expect_success 'remote prune reports deletions around a concurrent update' '
	test_when_finished "rm -rf race-empty.git race-prune" &&
	git init --bare race-empty.git &&
	git init race-prune &&
	(
		cd race-prune &&
		git commit --allow-empty -m one &&
		one=$(git rev-parse HEAD) &&
		git commit --allow-empty -m two &&
		two=$(git rev-parse HEAD) &&
		git remote add origin ../race-empty.git &&
		git update-ref refs/remotes/origin/race "$one" &&
		git update-ref refs/remotes/origin/other "$one"
	) &&
	test_hook -C race-prune reference-transaction <<-\EOF &&
		marker=$(git rev-parse --git-path prune-race-once)
		if test "$1" = preparing && test ! -e "$marker"
		then
			>"$marker"
			git update-ref refs/remotes/origin/race HEAD
		fi
		exit 0
	EOF
	(
		cd race-prune &&
		two=$(git rev-parse HEAD) &&
		test_must_fail git remote prune origin >out 2>err &&
		test_cmp_rev "$two" refs/remotes/origin/race &&
		test_must_fail git rev-parse --verify refs/remotes/origin/other &&
		test_grep "\[pruned\].*origin/other" out &&
		test_grep ! "\[pruned\].*origin/race" out &&
		test_grep "could not delete reference refs/remotes/origin/race" err
	)
'

test_expect_success 'fetch prune reports deletions around a concurrent update' '
	test_when_finished "rm -rf fetch-empty.git fetch-prune" &&
	git init --bare fetch-empty.git &&
	git init fetch-prune &&
	(
		cd fetch-prune &&
		git commit --allow-empty -m one &&
		one=$(git rev-parse HEAD) &&
		git commit --allow-empty -m two &&
		git remote add origin ../fetch-empty.git &&
		git update-ref refs/remotes/origin/race "$one" &&
		git update-ref refs/remotes/origin/other "$one"
	) &&
	test_hook -C fetch-prune reference-transaction <<-\EOF &&
		marker=$(git rev-parse --git-path prune-race-once)
		if test "$1" = preparing && test ! -e "$marker"
		then
			>"$marker"
			git update-ref refs/remotes/origin/race HEAD
		fi
		exit 0
	EOF
	(
		cd fetch-prune &&
		two=$(git rev-parse HEAD) &&
		test_must_fail git fetch --prune origin >out 2>err &&
		test_cmp_rev "$two" refs/remotes/origin/race &&
		test_must_fail git rev-parse --verify refs/remotes/origin/other &&
		test_grep "\[deleted\].*origin/other" err &&
		test_grep ! "\[deleted\].*origin/race" err &&
		test_grep "could not delete reference refs/remotes/origin/race" err
	)
'

test_expect_success 'hook allows updating ref if successful' '
	git reset --hard PRE &&
	test_hook reference-transaction <<-\EOF &&
		echo "$*" >>actual
	EOF
	cat >expect <<-EOF &&
		preparing
		prepared
		committed
	EOF
	git update-ref HEAD POST &&
	test_cmp expect actual
'

test_expect_success 'hook aborts updating ref in preparing state' '
	git reset --hard PRE &&
	test_hook reference-transaction <<-\EOF &&
		if test "$1" = preparing
		then
			exit 1
		fi
	EOF
	test_must_fail git update-ref HEAD POST 2>err &&
	test_grep "in '\''preparing'\'' phase, update aborted by the reference-transaction hook" err
'

test_expect_success 'hook aborts updating ref in prepared state' '
	git reset --hard PRE &&
	test_hook reference-transaction <<-\EOF &&
		if test "$1" = prepared
		then
			exit 1
		fi
	EOF
	test_must_fail git update-ref HEAD POST 2>err &&
	test_grep "in '\''prepared'\'' phase, update aborted by the reference-transaction hook" err
'

test_expect_success 'hook gets all queued updates in prepared state' '
	test_when_finished "rm actual" &&
	git reset --hard PRE &&
	test_hook reference-transaction <<-\EOF &&
		if test "$1" = prepared
		then
			while read -r line
			do
				printf "%s\n" "$line"
			done >actual
		fi
	EOF
	cat >expect <<-EOF &&
		$ZERO_OID $POST_OID refs/heads/main
	EOF
	git update-ref HEAD POST <<-EOF &&
		update HEAD $ZERO_OID $POST_OID
		update refs/heads/main $ZERO_OID $POST_OID
	EOF
	test_cmp expect actual
'

test_expect_success 'hook gets all queued updates in committed state' '
	test_when_finished "rm actual" &&
	git reset --hard PRE &&
	test_hook reference-transaction <<-\EOF &&
		if test "$1" = committed
		then
			while read -r line
			do
				printf "%s\n" "$line"
			done >actual
		fi
	EOF
	cat >expect <<-EOF &&
		$ZERO_OID $POST_OID refs/heads/main
	EOF
	git update-ref HEAD POST &&
	test_cmp expect actual
'

test_expect_success 'hook gets all queued updates in aborted state' '
	test_when_finished "rm actual" &&
	git reset --hard PRE &&
	test_hook reference-transaction <<-\EOF &&
		if test "$1" = aborted
		then
			while read -r line
			do
				printf "%s\n" "$line"
			done >actual
		fi
	EOF
	cat >expect <<-EOF &&
		$ZERO_OID $POST_OID HEAD
		$ZERO_OID $POST_OID refs/heads/main
	EOF
	git update-ref --stdin <<-EOF &&
		start
		update HEAD POST $ZERO_OID
		update refs/heads/main POST $ZERO_OID
		abort
	EOF
	test_cmp expect actual
'

test_expect_success 'interleaving hook calls succeed' '
	test_when_finished "rm -r target-repo.git" &&

	git init --bare target-repo.git &&

	test_hook -C target-repo.git reference-transaction <<-\EOF &&
		echo $0 "$@" >>actual
	EOF

	test_hook -C target-repo.git update <<-\EOF &&
		echo $0 "$@" >>actual
	EOF

	cat >expect <<-EOF &&
		hooks/update refs/tags/PRE $ZERO_OID $PRE_OID
		hooks/update refs/tags/POST $ZERO_OID $POST_OID
		hooks/reference-transaction preparing
		hooks/reference-transaction prepared
		hooks/reference-transaction committed
	EOF

	git push ./target-repo.git PRE POST &&
	test_cmp expect target-repo.git/actual
'

test_expect_success 'hook captures git-symbolic-ref updates' '
	test_when_finished "rm actual" &&

	test_hook reference-transaction <<-\EOF &&
		echo "$*" >>actual
		while read -r line
		do
			printf "%s\n" "$line"
		done >>actual
	EOF

	git symbolic-ref refs/heads/symref refs/heads/main &&

	cat >expect <<-EOF &&
	preparing
	$ZERO_OID ref:refs/heads/main refs/heads/symref
	prepared
	$ZERO_OID ref:refs/heads/main refs/heads/symref
	committed
	$ZERO_OID ref:refs/heads/main refs/heads/symref
	EOF

	test_cmp expect actual
'

test_expect_success 'hook gets all queued symref updates' '
	test_when_finished "rm actual" &&

	git update-ref refs/heads/branch $POST_OID &&
	git symbolic-ref refs/heads/symref refs/heads/main &&
	git symbolic-ref refs/heads/symrefd refs/heads/main &&
	git symbolic-ref refs/heads/symrefu refs/heads/main &&

	test_hook reference-transaction <<-\EOF &&
	echo "$*" >>actual
	while read -r line
	do
		printf "%s\n" "$line"
	done >>actual
	EOF

	# In the files backend, "delete" also triggers an additional transaction
	# update on the packed-refs backend, which constitutes additional reflog
	# entries.
	cat >expect <<-EOF &&
	preparing
	ref:refs/heads/main $ZERO_OID refs/heads/symref
	ref:refs/heads/main $ZERO_OID refs/heads/symrefd
	$ZERO_OID ref:refs/heads/main refs/heads/symrefc
	ref:refs/heads/main ref:refs/heads/branch refs/heads/symrefu
	EOF

	if test_have_prereq REFFILES
	then
		cat >>expect <<-EOF
		aborted
		$ZERO_OID $ZERO_OID refs/heads/symrefd
		EOF
	fi &&

	cat >>expect <<-EOF &&
	prepared
	ref:refs/heads/main $ZERO_OID refs/heads/symref
	ref:refs/heads/main $ZERO_OID refs/heads/symrefd
	$ZERO_OID ref:refs/heads/main refs/heads/symrefc
	ref:refs/heads/main ref:refs/heads/branch refs/heads/symrefu
	committed
	ref:refs/heads/main $ZERO_OID refs/heads/symref
	ref:refs/heads/main $ZERO_OID refs/heads/symrefd
	$ZERO_OID ref:refs/heads/main refs/heads/symrefc
	ref:refs/heads/main ref:refs/heads/branch refs/heads/symrefu
	EOF

	git update-ref --no-deref --stdin <<-EOF &&
	start
	symref-verify refs/heads/symref refs/heads/main
	symref-delete refs/heads/symrefd refs/heads/main
	symref-create refs/heads/symrefc refs/heads/main
	symref-update refs/heads/symrefu refs/heads/branch ref refs/heads/main
	prepare
	commit
	EOF
	test_cmp expect actual
'

test_done
