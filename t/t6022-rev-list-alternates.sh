#!/bin/sh

test_description='handling of alternates in rev-list'

TEST_PASSES_SANITIZE_LEAK=true
. ./test-lib.sh

# We create 5 commits and move them to the alt directory and
# create 5 more commits which will stay in the main odb.
test_expect_success 'create repository and alternate directory' '
	test_commit_bulk 5 &&
	git clone --reference=. --shared . alt &&
	test_commit_bulk --start=6 -C alt 5
'

# When the alternate odb is provided, all commits are listed along with the boundary
# commit.
test_expect_success 'rev-list passes with alternate object directory' '
	git -C alt rev-list --all --objects --no-object-names >actual.raw &&
	{
		git rev-list --all --objects --no-object-names &&
		git -C alt rev-list --all --objects --no-object-names --not \
			--alternate-refs
	} >expect.raw &&
	sort actual.raw >actual &&
	sort expect.raw >expect &&
	test_cmp expect actual
'

alt=alt/.git/objects/info/alternates

hide_alternates () {
	test -f "$alt.bak" || mv "$alt" "$alt.bak"
}

show_alternates () {
	test -f "$alt" || mv "$alt.bak" "$alt"
}

# When the alternate odb is not provided, rev-list fails since the 5th commit's
# parent is not present in the main odb.
test_expect_success 'rev-list fails without alternate object directory' '
	hide_alternates &&
	test_must_fail git -C alt rev-list HEAD
'

# With `--ignore-missing-links`, we stop the traversal when we encounter a
# missing link. The boundary commit is not listed as we haven't used the
# `--boundary` options.
test_expect_success 'rev-list only prints main odb commits with --ignore-missing-links' '
	hide_alternates &&

	git -C alt rev-list --objects --no-object-names \
		--ignore-missing-links --missing=allow-any HEAD >actual.raw &&
	git -C alt cat-file  --batch-check="%(objectname)" \
		--batch-all-objects >expect.raw &&

	sort actual.raw >actual &&
	sort expect.raw >expect &&
	test_cmp expect actual
'

# With `--ignore-missing-links` and `--boundary`, we can even print those boundary
# commits.
test_expect_success 'rev-list prints boundary commit with --ignore-missing-links' '
	git -C alt rev-list --ignore-missing-links --boundary HEAD >got &&
	grep "^-$(git rev-parse HEAD)" got
'

test_expect_success "setup for rev-list --ignore-missing-links with missing objects" '
	show_alternates &&
	test_commit -C alt 11
'

for obj in "HEAD^{tree}" "HEAD:11.t"
do
	# The `--ignore-missing-links` option should ensure that git-rev-list(1)
	# doesn't fail when used alongside `--objects` when a tree/blob is
	# missing.
	test_expect_success "rev-list --ignore-missing-links with missing $type" '
		oid="$(git -C alt rev-parse $obj)" &&
		path="alt/.git/objects/$(test_oid_to_path $oid)" &&

		mv "$path" "$path.hidden" &&
		test_when_finished "mv $path.hidden $path" &&

		git -C alt rev-list --ignore-missing-links --missing=allow-any --objects HEAD \
			>actual &&
		! grep $oid actual
       '
done

test_done
