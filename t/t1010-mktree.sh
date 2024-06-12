#!/bin/sh

test_description='git mktree'

TEST_PASSES_SANITIZE_LEAK=true
. ./test-lib.sh

test_expect_success setup '
	for d in folder folder- folder0
	do
		mkdir "$d" && echo "$d/one" >"$d/one" &&
		git add "$d" || return 1
	done &&
	for f in before folder.txt later
	do
		echo "$f" >"$f" &&
		git add "$f" || return 1
	done &&
	echo zero >one &&
	git update-index --add --info-only one &&
	git write-tree --missing-ok >tree.missing &&
	git ls-tree $(cat tree.missing) >top.missing &&
	git ls-tree -r $(cat tree.missing) >all.missing &&
	echo one >one &&
	git add one &&
	git write-tree >tree &&
	git ls-tree $(cat tree) >top &&
	git ls-tree -r $(cat tree) >all &&
	test_tick &&
	git commit -q -m one &&
	H=$(git rev-parse HEAD) &&
	git update-index --add --cacheinfo 160000 $H sub &&
	test_tick &&
	git commit -q -m two &&
	git rev-parse HEAD^{tree} >tree.withsub &&
	git ls-tree HEAD >top.withsub &&
	git ls-tree -r HEAD >all.withsub
'

test_expect_success 'ls-tree piped to mktree (1)' '
	git mktree <top >actual &&
	test_cmp tree actual
'

test_expect_success 'ls-tree piped to mktree (2)' '
	git mktree <top.withsub >actual &&
	test_cmp tree.withsub actual
'

test_expect_success 'ls-tree output in wrong order given to mktree (1)' '
	perl -e "print reverse <>" <top |
	git mktree >actual &&
	test_cmp tree actual
'

test_expect_success 'ls-tree output in wrong order given to mktree (2)' '
	perl -e "print reverse <>" <top.withsub |
	git mktree >actual &&
	test_cmp tree.withsub actual
'

test_expect_success '--batch creates multiple trees' '
	cat top >multi-tree &&
	echo "" >>multi-tree &&
	cat top.withsub >>multi-tree &&

	cat tree >expect &&
	cat tree.withsub >>expect &&
	git mktree --batch <multi-tree >actual &&
	test_cmp expect actual
'

test_expect_success 'allow missing object with --missing' '
	git mktree --missing <top.missing >actual &&
	test_cmp tree.missing actual
'

test_expect_success 'mktree with invalid submodule OIDs' '
	# non-existent OID - ok
	printf "160000 commit $(test_oid numeric)\tA\n" >in &&
	git mktree <in >tree.actual &&
	git ls-tree $(cat tree.actual) >actual &&
	test_cmp in actual &&

	# existing OID, wrong type - error
	tree_oid="$(cat tree)" &&
	printf "160000 commit $tree_oid\tA" |
	test_must_fail git mktree 2>err &&
	grep "object $tree_oid is a tree but specified type was (commit)" err
'

test_expect_success 'mktree reads ls-tree -r output (1)' '
	git mktree <all >actual &&
	test_cmp tree actual
'

test_expect_success 'mktree reads ls-tree -r output (2)' '
	git mktree <all.withsub >actual &&
	test_cmp tree.withsub actual
'

test_expect_success 'mktree de-duplicates files inside directories' '
	git ls-tree $(cat tree) >everything &&
	cat <all >top_and_all &&
	git mktree <top_and_all >actual &&
	test_cmp tree actual
'

test_expect_success 'mktree fails on malformed input' '
	# empty line without --batch
	echo "" |
	test_must_fail git mktree 2>err &&
	grep "blank line only valid in batch mode" err &&

	# bad whitespace
	printf "100644 blob $EMPTY_BLOB A" |
	test_must_fail git mktree 2>err &&
	grep "malformed input line" err &&

	# invalid type
	printf "100644 bad $EMPTY_BLOB\tA" |
	test_must_fail git mktree 2>err &&
	grep "invalid object type" err &&

	# invalid OID length
	printf "100755 blob abc123\tA" |
	test_must_fail git mktree 2>err &&
	grep "malformed input line" err &&

	# bad quoting
	printf "100644 blob $EMPTY_BLOB\t\"A" |
	test_must_fail git mktree 2>err &&
	grep "bad quoting of path name" err
'

test_expect_success 'mktree fails on mode mismatch' '
	tree_oid="$(cat tree)" &&

	# mode-type mismatch
	printf "100644 tree $tree_oid\tA" |
	test_must_fail git mktree 2>err &&
	grep "object type (tree) doesn${SQ}t match mode type (blob)" err &&

	# mode-object mismatch (no --missing)
	printf "100644 $tree_oid\tA" |
	test_must_fail git mktree 2>err &&
	grep "object $tree_oid is a tree but specified type was (blob)" err
'

test_expect_success '--literally can create invalid trees' '
	tree_oid="$(cat tree)" &&
	blob_oid="$(git rev-parse ${tree_oid}:one)" &&

	# duplicate entries
	{
		printf "040000 tree $tree_oid\tmy-tree\n" &&
		printf "100644 blob $blob_oid\ttest-file\n" &&
		printf "100755 blob $blob_oid\ttest-file\n"
	} | git mktree --literally >tree.bad &&
	git cat-file tree $(cat tree.bad) >top.bad &&
	test_must_fail git hash-object --stdin -t tree <top.bad 2>err &&
	grep "contains duplicate file entries" err &&

	# disallowed path
	{
		printf "100644 blob $blob_oid\t.git\n"
	} | git mktree --literally >tree.bad &&
	git cat-file tree $(cat tree.bad) >top.bad &&
	test_must_fail git hash-object --stdin -t tree <top.bad 2>err &&
	grep "contains ${SQ}.git${SQ}" err &&

	# nested entry
	{
		printf "100644 blob $blob_oid\tdeeper/my-file\n"
	} | git mktree --literally >tree.bad &&
	git cat-file tree $(cat tree.bad) >top.bad &&
	test_must_fail git hash-object --stdin -t tree <top.bad 2>err &&
	grep "contains full pathnames" err &&

	# bad entry ordering
	{
		printf "100644 blob $blob_oid\tB\n" &&
		printf "040000 tree $tree_oid\tA\n"
	} | git mktree --literally >tree.bad &&
	git cat-file tree $(cat tree.bad) >top.bad &&
	test_must_fail git hash-object --stdin -t tree <top.bad 2>err &&
	grep "not properly sorted" err
'

test_expect_success 'mktree validates path' '
	tree_oid="$(cat tree)" &&
	blob_oid="$(git rev-parse $tree_oid:folder.txt)" &&
	head_oid="$(git rev-parse HEAD)" &&

	# Valid: tree with or without trailing slash, blob without trailing slash
	{
		printf "040000 tree $tree_oid\tfolder1/\n" &&
		printf "040000 tree $tree_oid\tfolder2\n" &&
		printf "100644 blob $blob_oid\tfile.txt\n"
	} | git mktree >actual &&

	# Invalid: blob with trailing slash
	printf "100644 blob $blob_oid\ttest/" |
	test_must_fail git mktree 2>err &&
	grep "invalid path ${SQ}test/${SQ}" err &&

	# Invalid: dotdot
	printf "040000 tree $tree_oid\t../" |
	test_must_fail git mktree 2>err &&
	grep "invalid path ${SQ}../${SQ}" err &&

	# Invalid: dot
	printf "040000 tree $tree_oid\t." |
	test_must_fail git mktree 2>err &&
	grep "invalid path ${SQ}.${SQ}" err &&

	# Invalid: .git
	printf "040000 tree $tree_oid\t.git/" |
	test_must_fail git mktree 2>err &&
	grep "invalid path ${SQ}.git/${SQ}" err
'

test_expect_success 'mktree with duplicate entries' '
	tree_oid=$(cat tree) &&
	folder_oid=$(git rev-parse ${tree_oid}:folder) &&
	before_oid=$(git rev-parse ${tree_oid}:before) &&
	head_oid=$(git rev-parse HEAD) &&

	{
		printf "100755 blob $before_oid\ttest\n" &&
		printf "040000 tree $folder_oid\ttest-\n" &&
		printf "160000 commit $head_oid\ttest.txt\n" &&
		printf "040000 tree $folder_oid\ttest\n" &&
		printf "100644 blob $before_oid\ttest0\n" &&
		printf "160000 commit $head_oid\ttest-\n"
	} >top.dup &&
	git mktree <top.dup >tree.actual &&

	{
		printf "160000 commit $head_oid\ttest-\n" &&
		printf "160000 commit $head_oid\ttest.txt\n" &&
		printf "040000 tree $folder_oid\ttest\n" &&
		printf "100644 blob $before_oid\ttest0\n"
	} >expect &&
	git ls-tree $(cat tree.actual) >actual &&

	test_cmp expect actual
'

test_expect_success 'mktree adds entry after nested entry' '
	tree_oid=$(cat tree) &&
	folder_oid=$(git rev-parse ${tree_oid}:folder) &&
	one_oid=$(git rev-parse ${tree_oid}:folder/one) &&

	{
		printf "040000 tree $folder_oid\tearly\n" &&
		printf "100644 blob $one_oid\tearly/one\n" &&
		printf "100644 blob $one_oid\tlater\n" &&
		printf "040000 tree $EMPTY_TREE\tnew-tree\n" &&
		printf "100644 blob $one_oid\tnew-tree/one\n" &&
		printf "100644 blob $one_oid\tzzz\n"
	} >top.rec &&
	git mktree <top.rec >tree.actual &&

	{
		printf "040000 tree $folder_oid\tearly\n" &&
		printf "100644 blob $one_oid\tlater\n" &&
		printf "040000 tree $folder_oid\tnew-tree\n" &&
		printf "100644 blob $one_oid\tzzz\n"
	} >expect &&
	git ls-tree $(cat tree.actual) >actual &&

	test_cmp expect actual
'

test_expect_success 'mktree inserts entries into directories' '
	folder_oid=$(git rev-parse ${tree_oid}:folder) &&
	one_oid=$(git rev-parse ${tree_oid}:folder/one) &&
	blob_oid=$(git rev-parse ${tree_oid}:before) &&
	{
		printf "040000 tree $folder_oid\tfolder\n" &&
		printf "100644 blob $blob_oid\tfolder/two\n"
	} | git mktree >actual &&

	{
		printf "100644 blob $one_oid\tfolder/one\n" &&
		printf "100644 blob $blob_oid\tfolder/two\n"
	} >expect &&
	git ls-tree -r $(cat actual) >actual &&

	test_cmp expect actual
'

test_expect_success 'mktree with base tree' '
	tree_oid=$(cat tree) &&
	folder_oid=$(git rev-parse ${tree_oid}:folder) &&
	before_oid=$(git rev-parse ${tree_oid}:before) &&
	head_oid=$(git rev-parse HEAD) &&

	{
		printf "040000 tree $folder_oid\ttest\n" &&
		printf "100644 blob $before_oid\ttest.txt\n" &&
		printf "040000 tree $folder_oid\ttest-\n" &&
		printf "160000 commit $head_oid\ttest0\n"
	} >top.base &&
	git mktree <top.base >tree.base &&

	{
		printf "100755 blob $before_oid\tz\n" &&
		printf "160000 commit $head_oid\ttest.xyz\n" &&
		printf "040000 tree $folder_oid\ta\n" &&
		printf "100644 blob $before_oid\ttest\n"
	} >top.append &&
	git mktree $(cat tree.base) <top.append >tree.actual &&

	{
		printf "040000 tree $folder_oid\ta\n" &&
		printf "100644 blob $before_oid\ttest\n" &&
		printf "040000 tree $folder_oid\ttest-\n" &&
		printf "100644 blob $before_oid\ttest.txt\n" &&
		printf "160000 commit $head_oid\ttest.xyz\n" &&
		printf "160000 commit $head_oid\ttest0\n" &&
		printf "100755 blob $before_oid\tz\n"
	} >expect &&
	git ls-tree $(cat tree.actual) >actual &&

	test_cmp expect actual
'

test_expect_success 'mktree with base tree (deep)' '
	tree_oid=$(cat tree) &&
	folder_oid=$(git rev-parse ${tree_oid}:folder) &&
	before_oid=$(git rev-parse ${tree_oid}:before) &&
	folder_one_oid=$(git rev-parse ${tree_oid}:folder/one) &&
	head_oid=$(git rev-parse HEAD) &&

	{
		printf "100755 blob $before_oid\tfolder/before\n" &&
		printf "100644 blob $before_oid\tfolder/one.txt\n" &&
		printf "160000 commit $head_oid\tfolder/sub\n" &&
		printf "040000 tree $folder_oid\tfolder/one\n" &&
		printf "040000 tree $folder_oid\tfolder/one/deeper\n"
	} >top.append &&
	git mktree <top.append $(cat tree) >tree.actual &&

	{
		printf "100755 blob $before_oid\tfolder/before\n" &&
		printf "100644 blob $before_oid\tfolder/one.txt\n" &&
		printf "100644 blob $folder_one_oid\tfolder/one/deeper/one\n" &&
		printf "100644 blob $folder_one_oid\tfolder/one/one\n" &&
		printf "160000 commit $head_oid\tfolder/sub\n"
	} >expect &&
	git ls-tree -r $(cat tree.actual) -- folder/ >actual &&

	test_cmp expect actual
'

test_expect_success 'mktree fails on directory-file conflict' '
	tree_oid="$(cat tree)" &&
	blob_oid="$(git rev-parse $tree_oid:folder.txt)" &&

	{
		printf "100644 blob $blob_oid\ttest\n" &&
		printf "100644 blob $blob_oid\ttest/deeper\n"
	} |
	test_must_fail git mktree 2>err &&
	grep "You have both test and test/deeper" err &&

	{
		printf "100644 blob $blob_oid\tfolder/one/deeper/deep\n"
	} |
	test_must_fail git mktree $tree_oid 2>err &&
	grep "You have both folder/one and folder/one/deeper/deep" err
'

test_expect_success 'mktree with remove entries' '
	tree_oid="$(cat tree)" &&
	blob_oid="$(git rev-parse $tree_oid:folder.txt)" &&

	{
		printf "100644 blob $blob_oid\ttest/deeper/deep.txt\n" &&
		printf "100644 blob $blob_oid\ttest.txt\n" &&
		printf "100644 blob $blob_oid\texample\n" &&
		printf "100644 blob $blob_oid\texample.a/file\n" &&
		printf "100644 blob $blob_oid\texample.txt\n" &&
		printf "040000 tree $tree_oid\tfolder\n" &&
		printf "0 $ZERO_OID\tfolder\n" &&
		printf "0 $ZERO_OID\tmissing\n"
	} | git mktree >tree.base &&

	{
		printf "0 $ZERO_OID\texample.txt\n" &&
		printf "0 $ZERO_OID\ttest/deeper\n"
	} | git mktree $(cat tree.base) >tree.actual &&

	{
		printf "100644 blob $blob_oid\texample\n" &&
		printf "100644 blob $blob_oid\texample.a/file\n" &&
		printf "100644 blob $blob_oid\ttest.txt\n"
	} >expect &&
	git ls-tree -r $(cat tree.actual) >actual &&

	test_cmp expect actual
'

test_expect_success 'type and oid not checked if entry mode is 0' '
	# type and oid do not match
	printf "0 commit $EMPTY_TREE\tfolder.txt\n" |
	git mktree >tree.actual &&

	test "$(cat tree.actual)" = $EMPTY_TREE
'

test_done
