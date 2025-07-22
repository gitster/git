#!/bin/sh

test_description='Manually write reflog entries'

. ./test-lib.sh

SIGNATURE="C O Mitter <committer@example.com> 1112911993 -0700"

test_reflog_matches () {
	repo="$1" &&
	refname="$2" &&
	cat >actual &&
	test-tool -C "$repo" ref-store main for-each-reflog-ent "$refname" >expected &&
	test_cmp expected actual
}

test_expect_success 'invalid number of arguments' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		for args in "" "1" "1 2" "1 2 3" "1 2 3 4 5"
		do
			test_must_fail git reflog write $args 2>err &&
			test_grep "usage: git reflog write" err || return 1
		done
	)
'

test_expect_success 'invalid refname' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_must_fail git reflog write "refs/heads/ invalid" $ZERO_OID $ZERO_OID first 2>err &&
		test_grep "invalid reference name: " err
	)
'

test_expect_success 'nonexistent old object ID' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_must_fail git reflog write refs/heads/something $(test_oid deadbeef) $ZERO_OID first 2>err &&
		test_grep "old object .* does not exist" err
	)
'

test_expect_success 'nonexistent new object ID' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_must_fail git reflog write refs/heads/something $ZERO_OID $(test_oid deadbeef) first 2>err &&
		test_grep "new object .* does not exist" err
	)
'

test_expect_success 'simple writes' '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		test_commit initial &&
		COMMIT_OID=$(git rev-parse HEAD) &&

		git reflog write refs/heads/something $ZERO_OID $COMMIT_OID first &&
		test_reflog_matches . refs/heads/something <<-EOF &&
		$ZERO_OID $COMMIT_OID $SIGNATURE	first
		EOF

		git reflog write refs/heads/something $COMMIT_OID $COMMIT_OID second &&
		test_reflog_matches . refs/heads/something <<-EOF
		$ZERO_OID $COMMIT_OID $SIGNATURE	first
		$COMMIT_OID $COMMIT_OID $SIGNATURE	second
		EOF
	)
'

test_done
