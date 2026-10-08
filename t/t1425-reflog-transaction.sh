#!/bin/sh

test_description='explicit reflog entries in reference transactions'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit A &&
	test_commit B &&
	A=$(git rev-parse A) &&
	B=$(git rev-parse B)
'

test_expect_success 'log-only entry with null new OID does not delete existing history' '
	git branch source "$A" &&
	test-tool ref-store main for-each-reflog-ent refs/heads/source >before &&
	test-tool ref-store main reflog-transaction refs/heads/source append \
		1 "$A" "$ZERO_OID" deletion &&
	test-tool ref-store main for-each-reflog-ent refs/heads/source >after &&
	sed "$ d" after >history &&
	test_cmp before history &&
	test_grep "$A $ZERO_OID .*deletion" after &&
	test_cmp_rev "$A" source
'

test_expect_success 'replace reflog with entries ordered by index' '
	test-tool ref-store main reflog-transaction refs/heads/source replace \
		2 "$A" "$B" second \
		1 "$ZERO_OID" "$A" first &&
	git reflog show --format=%gs source >actual &&
	printf "%s\n" second first >expect &&
	test_cmp expect actual &&
	test_cmp_rev "$A" source
'

test_expect_success 'prepared hook can reject replacement without losing history' '
	test_when_finished "rm -f .git/hooks/reference-transaction" &&
	test-tool ref-store main for-each-reflog-ent refs/heads/source >before &&
	write_script .git/hooks/reference-transaction <<-\EOF &&
		test "$1" != prepared
	EOF
	test_must_fail test-tool ref-store main reflog-transaction refs/heads/source replace \
		1 "$ZERO_OID" "$B" replacement &&
	test-tool ref-store main for-each-reflog-ent refs/heads/source >after &&
	test_cmp before after
'

test_expect_success 'replacement with no entries removes only the reflog' '
	test-tool ref-store main reflog-transaction refs/heads/source replace &&
	test_must_fail git reflog exists refs/heads/source &&
	test_cmp_rev "$A" source
'

test_expect_success 'duplicate replacement is rejected' '
	test_must_fail test-tool ref-store main reflog-transaction \
		refs/heads/source replace-twice
'

test_done
