#!/bin/sh

test_description='git replay --[no-]gpg-sign'

GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh
. "$TEST_DIRECTORY/lib-gpg.sh"

if ! test_have_prereq GPG
then
	skip_all='skip all git replay --[no-]gpg-sign tests, gpg not available'
	test_done
fi

# Checks that "topic" was replayed onto "main", and that the replayed
# commits are all signed ("signed") or all unsigned ("unsigned").
check_replayed () {
	git merge-base --is-ancestor main topic &&
	git rev-list main..topic >replayed &&
	test_line_count = 2 replayed &&
	for commit in $(cat replayed)
	do
		case "$1" in
		signed)
			git verify-commit $commit || return 1
			;;
		unsigned)
			test_must_fail git verify-commit $commit || return 1
			;;
		esac
	done
}

test_expect_success 'setup' '
	test_commit A &&
	test_commit B &&
	git switch -c topic A &&
	echo C >C &&
	git add C &&
	git commit -S -m C &&
	git tag C &&
	echo D >D &&
	git add D &&
	git commit -S -m D &&
	git tag D &&
	git switch main
'

test_expect_success 'replay without --gpg-sign does not sign' '
	git branch -f topic D &&
	git verify-commit C &&
	git verify-commit D &&
	git replay --onto main A..topic &&
	check_replayed unsigned
'

test_expect_success 'replay --gpg-sign signs with the default key' '
	git branch -f topic D &&
	git replay --gpg-sign --onto main A..topic &&
	check_replayed signed &&
	echo "C O Mitter <committer@example.com>" >expect &&
	git log -1 --format="%GS" topic >actual &&
	test_cmp expect actual
'

test_expect_success 'replay -S<keyid> signs with the given key' '
	git branch -f topic D &&
	git replay -SB7227189 --onto main A..topic &&
	git rev-list main..topic >replayed &&
	test_line_count = 2 replayed &&
	echo D4BE22311AD3131E5EDA29A461092E85B7227189 >expect &&
	for commit in $(cat replayed)
	do
		git log -1 --format="%GP" $commit >actual &&
		test_cmp expect actual || return 1
	done
'

test_expect_success 'replay --no-gpg-sign countermands --gpg-sign' '
	git branch -f topic D &&
	git replay --gpg-sign --no-gpg-sign --onto main A..topic &&
	check_replayed unsigned
'

test_expect_success 'replay ignores commit.gpgSign' '
	git branch -f topic D &&
	git -c commit.gpgSign=true replay --onto main A..topic &&
	check_replayed unsigned
'

test_expect_success 'replay fails and updates no ref when signing fails' '
	git branch -f topic D &&
	test_must_fail git replay -Snonexistent-key --onto main A..topic &&
	test_cmp_rev D topic
'

test_expect_success 'replay --ref fails and updates no ref when signing fails' '
	git branch -f topic D &&
	test_must_fail git replay -Snonexistent-key --onto main \
		--ref refs/heads/other A..topic &&
	test_must_fail git rev-parse --verify refs/heads/other &&
	test_cmp_rev D topic
'

test_done
