#!/bin/sh

test_description='reference transactions for copy and rename'

GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh

snapshot () {
	git for-each-ref --format="%(refname) %(objectname) %(symref)" >"$1.refs" &&
	for ref in HEAD refs/heads/source refs/heads/destination
	do
		if git reflog exists "$ref"
		then
			test-tool ref-store main for-each-reflog-ent "$ref"
		else
			echo missing
		fi >"$1.$(basename "$ref").log" || return 1
	done
}

compare_snapshot () {
	for suffix in refs HEAD.log source.log destination.log
	do
		test_cmp "$1.$suffix" "$2.$suffix" || return 1
	done
}

test_expect_success 'setup' '
	test_commit A &&
	test_commit B &&
	A=$(git rev-parse A) &&
	B=$(git rev-parse B)
'

for operation in rename copy
do
	test_expect_success "$operation reports one logical transaction" '
		test_when_finished "git config --unset core.hooksPath" &&
		git branch -f source "$A" &&
		git branch -f destination "$B" &&
		mkdir -p hooks &&
		write_script hooks/reference-transaction <<-\EOF &&
			echo "$1" >>actual &&
			cat >>actual
		EOF
		git config core.hooksPath hooks &&
		>actual &&
		test-tool ref-store main copy-transaction "$operation" refs/heads/source refs/heads/destination &&
		{
			echo preparing &&
			if test "$operation" = rename
			then
				echo "$ZERO_OID $ZERO_OID refs/heads/source"
			fi &&
			echo "$ZERO_OID $A refs/heads/destination" &&
			for state in prepared committed
			do
				echo "$state" &&
				if test "$operation" = rename
				then
					echo "$A $ZERO_OID refs/heads/source"
				fi &&
				echo "$B $A refs/heads/destination" || return 1
			done
		} >expect &&
		test_cmp expect actual
	'

	for state in preparing prepared
	do
		test_expect_success "$operation rejected at $state leaves refs and full reflogs unchanged" '
			test_when_finished "git config --unset core.hooksPath" &&
			git branch -f source "$A" &&
			git branch -f destination "$B" &&
			git symbolic-ref HEAD refs/heads/source &&
			test_when_finished "git -c core.hooksPath=/dev/null symbolic-ref HEAD refs/heads/main" &&
			snapshot before &&
			write_script hooks/reference-transaction <<-EOF &&
				test "\$1" != "$state"
			EOF
			git config core.hooksPath hooks &&
			test_must_fail test-tool ref-store main copy-transaction "$operation" refs/heads/source refs/heads/destination &&
			snapshot after &&
			compare_snapshot before after
		'
	done
done

test_expect_success 'prepared hook sees old refs and cannot modify the source' '
	test_when_finished "git config --unset core.hooksPath" &&
	git branch -f source "$A" &&
	git branch -f destination "$B" &&
	write_script hooks/reference-transaction <<-EOF &&
		test "\$1" = prepared || exit 0
		git rev-parse refs/heads/source >source-seen &&
		git rev-parse refs/heads/destination >destination-seen &&
		if git -c core.hooksPath=/dev/null -c core.filesRefLockTimeout=0 \
			update-ref refs/heads/source $B
		then
			exit 1
		fi
	EOF
	git config core.hooksPath hooks &&
	git branch -M source destination &&
	echo "$A" >expect &&
	test_cmp expect source-seen &&
	echo "$B" >expect &&
	test_cmp expect destination-seen &&
	test_cmp_rev "$A" destination
'

test_expect_success 'preparing may update source and its reflog' '
	test_when_finished "git config --unset core.hooksPath" &&
	git branch source "$A" &&
	write_script hooks/reference-transaction <<-EOF &&
		test "\$1" = preparing || exit 0
		git -c core.hooksPath=/dev/null update-ref -m concurrent refs/heads/source $B
	EOF
	git config core.hooksPath hooks &&
	git branch -M source destination &&
	test_cmp_rev "$B" destination &&
	git reflog show --format=%gs destination >actual &&
	test_grep concurrent actual
'

test_expect_success 'rename without a source reflog can be rejected without losing destination history' '
	test_when_finished "git config --unset core.hooksPath" &&
	git -c core.logAllRefUpdates=false branch source "$A" &&
	test_must_fail git reflog exists refs/heads/source &&
	test-tool ref-store main for-each-reflog-ent refs/heads/destination >before &&
	write_script hooks/reference-transaction <<-\EOF &&
		test "$1" != prepared
	EOF
	git config core.hooksPath hooks &&
	test_must_fail git branch -M source destination &&
	test-tool ref-store main for-each-reflog-ent refs/heads/destination >after &&
	test_cmp before after
'

for names in "parent parent/child" "child/branch child"
do
	set -- $names
	from=$1 to=$2
	test_expect_success "D/F rename $from to $to can be aborted and committed" '
		test_when_finished "git config --unset core.hooksPath" &&
		git branch "$from" "$A" &&
		test-tool ref-store main for-each-reflog-ent "refs/heads/$from" >before &&
		write_script hooks/reference-transaction <<-EOF &&
			test "\$1" = prepared || exit 0
			git rev-parse refs/heads/$from >seen
			exit 1
		EOF
		git config core.hooksPath hooks &&
		test_must_fail git branch -m "$from" "$to" &&
		echo "$A" >expect &&
		test_cmp expect seen &&
		test_cmp_rev "$A" "$from" &&
		test-tool ref-store main for-each-reflog-ent "refs/heads/$from" >after &&
		test_cmp before after &&
		git -c core.hooksPath=/dev/null branch -m "$from" "$to" &&
		test_cmp_rev "$A" "$to"
	'
done

test_expect_success REFFILES,POSIXPERM 'D/F reflog installation failure restores source history' '
	test_when_finished "chmod u+w .git/logs/refs/heads" &&
	test_when_finished "git config --unset core.hooksPath" &&
	git branch rollback/branch "$A" &&
	test-tool ref-store main for-each-reflog-ent refs/heads/rollback/branch >before &&
	write_script hooks/reference-transaction <<-\EOF &&
		test "$1" = prepared || exit 0
		chmod a-w .git/logs/refs/heads
	EOF
	git config core.hooksPath hooks &&
	test_must_fail git branch -m rollback/branch rollback &&
	chmod u+w .git/logs/refs/heads &&
	test_cmp_rev "$A" rollback/branch &&
	test_must_fail git show-ref --verify refs/heads/rollback &&
	test-tool ref-store main for-each-reflog-ent refs/heads/rollback/branch >after &&
	test_cmp before after
'

test_expect_success 'multiple renames, a copy and an ordinary update compose' '
	git branch first "$A" &&
	git branch second "$B" &&
	git branch third "$A" &&
	test-tool ref-store main copy-transaction \
		rename refs/heads/first refs/heads/first-new \
		rename refs/heads/second refs/heads/second-new \
		copy refs/heads/third refs/heads/third-new \
		update refs/heads/fourth "$B" &&
	test_must_fail git show-ref --verify refs/heads/first &&
	test_must_fail git show-ref --verify refs/heads/second &&
	test_cmp_rev "$A" first-new &&
	test_cmp_rev "$B" second-new &&
	test_cmp_rev "$A" third-new &&
	test_cmp_rev "$A" third &&
	test_cmp_rev "$B" fourth
'

test_expect_success 'copy preserves complete source history and replaces destination history' '
	git branch history-source "$A" &&
	git update-ref -m history-step refs/heads/history-source "$B" &&
	git branch history-destination "$B" &&
	test-tool ref-store main for-each-reflog-ent refs/heads/history-source >before &&
	git branch -C history-source history-destination &&
	test-tool ref-store main for-each-reflog-ent refs/heads/history-source >after &&
	test_cmp before after &&
	test-tool ref-store main for-each-reflog-ent refs/heads/history-destination >copied &&
	sed "$ d" copied >history &&
	test_cmp before history &&
	test_grep "Branch: copied refs/heads/history-source to refs/heads/history-destination" copied
'

test_expect_success 'packed rename reports no internal transactions' '
	test_when_finished "git config --unset core.hooksPath" &&
	git branch packed-source "$A" &&
	git pack-refs --all &&
	write_script hooks/reference-transaction <<-\EOF &&
		echo "$1" >>states
		cat >/dev/null
	EOF
	git config core.hooksPath hooks &&
	git branch -m packed-source packed-destination &&
	printf "%s\n" preparing prepared committed >expect &&
	test_cmp expect states
'

test_expect_success REFFILES 'unrelated forced copies use independent reflog staging files' '
	test_when_finished "git config --unset core.hooksPath" &&
	git branch nested-a "$A" &&
	git branch nested-b "$B" &&
	git branch outer-a "$A" &&
	git branch outer-b "$B" &&
	write_script hooks/reference-transaction <<-\EOF &&
		test "$1" = prepared || exit 0
		git -c core.hooksPath=/dev/null branch -C nested-a nested-b
	EOF
	git config core.hooksPath hooks &&
	git branch -C outer-a outer-b &&
	test_cmp_rev "$A" outer-b &&
	test_cmp_rev "$A" nested-b
'

test_expect_success REFFILES 'rename with reflogs disabled does not create a reflog' '
	git -c core.logAllRefUpdates=false branch no-log "$A" &&
	git -c core.logAllRefUpdates=false branch -m no-log still-no-log &&
	test_must_fail git reflog exists refs/heads/still-no-log
'

test_expect_success REFFILES 'copy without source history retains existing destination history' '
	git -c core.logAllRefUpdates=false branch no-copy-log "$A" &&
	git branch existing-copy-log "$B" &&
	test-tool ref-store main for-each-reflog-ent refs/heads/existing-copy-log >before &&
	git branch -C no-copy-log existing-copy-log &&
	test-tool ref-store main for-each-reflog-ent refs/heads/existing-copy-log >after &&
	sed "$ d" after >history &&
	test_cmp before history
'

test_expect_success REFFILES,CASE_INSENSITIVE_FS 'case-only rename retains history and supports abort' '
	test_when_finished "git config --unset core.hooksPath" &&
	git branch case-source "$A" &&
	test-tool ref-store main for-each-reflog-ent refs/heads/case-source >before &&
	write_script hooks/reference-transaction <<-\EOF &&
		test "$1" != prepared
	EOF
	git config core.hooksPath hooks &&
	test_must_fail git branch -M case-source CASE-SOURCE &&
	test-tool ref-store main for-each-reflog-ent refs/heads/case-source >after &&
	test_cmp before after &&
	git -c core.hooksPath=/dev/null branch -M case-source CASE-SOURCE &&
	git for-each-ref --format="%(refname)" refs/heads/CASE-SOURCE >actual &&
	echo refs/heads/CASE-SOURCE >expect &&
	test_cmp expect actual
'

test_expect_success REFFILES,SYMLINKS 'symlink source reflog is rejected without changing refs' '
	git branch symlink-source "$A" &&
	mv .git/logs/refs/heads/symlink-source saved-log &&
	ln -s "$PWD/saved-log" .git/logs/refs/heads/symlink-source &&
	test_must_fail git branch -m symlink-source symlink-destination &&
	test_cmp_rev "$A" symlink-source &&
	test_must_fail git show-ref --verify refs/heads/symlink-destination
'

for operation in copy rename
do
	test_expect_success "$operation over HEAD referent preserves HEAD history" '
		git branch -f head-source "$A" &&
		git update-ref refs/heads/main "$B" &&
		test-tool ref-store main for-each-reflog-ent HEAD >before &&
		test-tool ref-store main copy-transaction "$operation" refs/heads/head-source refs/heads/main &&
		test-tool ref-store main for-each-reflog-ent HEAD >after &&
		sed "$ d" after >history &&
		test_cmp before history &&
		test_cmp_rev "$A" HEAD
	'
done

test_expect_success 'preparing may delete source but cannot cause a partial rename' '
	test_when_finished "git config --unset core.hooksPath" &&
	git branch race-source "$A" &&
	write_script hooks/reference-transaction <<-\EOF &&
		test "$1" = preparing || exit 0
		git -c core.hooksPath=/dev/null update-ref -d refs/heads/race-source
	EOF
	git config core.hooksPath hooks &&
	test_must_fail git branch -m race-source race-destination &&
	test_must_fail git show-ref --verify refs/heads/race-source &&
	test_must_fail git show-ref --verify refs/heads/race-destination
'

test_expect_success 'copy cannot replace a D/F-conflicting source' '
	git branch conflict-source "$A" &&
	test_must_fail git branch -c conflict-source conflict-source/child &&
	test_cmp_rev "$A" conflict-source &&
	test_must_fail git show-ref --verify refs/heads/conflict-source/child
'

test_expect_success REFFILES,POSIXPERM 'unreadable source reflog leaves refs and history unchanged' '
	git branch unreadable "$A" &&
	test-tool ref-store main for-each-reflog-ent refs/heads/unreadable >before &&
	test_when_finished "chmod u+r .git/logs/refs/heads/unreadable" &&
	chmod a-r .git/logs/refs/heads/unreadable &&
	test_must_fail git branch -m unreadable readable &&
	chmod u+r .git/logs/refs/heads/unreadable &&
	test_cmp_rev "$A" unreadable &&
	test_must_fail git show-ref --verify refs/heads/readable &&
	test-tool ref-store main for-each-reflog-ent refs/heads/unreadable >after &&
	test_cmp before after
'

test_expect_success REFFILES 'destination lock failure preserves source and destination history' '
	git branch -f source "$A" &&
	git branch -f destination "$B" &&
	snapshot before &&
	test_when_finished "rm -f .git/refs/heads/destination.lock" &&
	>.git/refs/heads/destination.lock &&
	test_must_fail git -c core.filesRefLockTimeout=0 branch -M source destination &&
	snapshot after &&
	compare_snapshot before after
'

test_expect_success REFFILES 'staged reflog files are cleaned up after success and abort' '
	find .git/logs -name ".tmp-reflog-*" >actual &&
	test_must_be_empty actual
'

test_done
