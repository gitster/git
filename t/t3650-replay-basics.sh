#!/bin/sh

test_description='basic git replay tests'

GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh

GIT_AUTHOR_NAME=author@name
GIT_AUTHOR_EMAIL=bogus@email@address
export GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL

test_expect_success 'setup' '
	test_commit A &&
	test_commit B &&

	git switch -c topic1 &&
	test_commit C &&
	git switch -c topic2 &&
	test_commit D &&
	test_commit E &&
	git switch topic1 &&
	test_commit F &&
	git switch -c topic3 &&
	test_commit G &&
	test_commit H &&
	git switch -c topic4 main &&
	test_commit I &&
	test_commit J &&

	git switch -c next main &&
	test_commit K &&
	git merge -m "Merge topic1" topic1 &&
	git merge -m "Merge topic2" topic2 &&
	git merge -m "Merge topic3" topic3 &&
	>evil &&
	git add evil &&
	git commit --amend &&
	git merge -m "Merge topic4" topic4 &&

	git switch main &&
	test_commit L &&
	test_commit M &&

	git switch -c conflict B &&
	test_commit C.conflict C.t conflict
'

test_expect_success 'setup bare' '
	git clone --bare . bare
'

test_expect_success 'using replay to rebase two branches, one on top of other' '
	git replay --ref-action=print --onto main topic1..topic2 >result &&

	test_line_count = 1 result &&

	git log --format=%s $(cut -f 3 -d " " result) >actual &&
	test_write_lines E D M L B A >expect &&
	test_cmp expect actual &&

	printf "update refs/heads/topic2 " >expect &&
	printf "%s " $(cut -f 3 -d " " result) >>expect &&
	git rev-parse topic2 >>expect &&

	test_cmp expect result
'

test_expect_success 'using replay on bare repo to rebase two branches, one on top of other' '
	git -C bare replay --ref-action=print --onto main topic1..topic2 >result-bare &&
	test_cmp expect result-bare
'

test_expect_success 'using replay to rebase with a conflict' '
	test_expect_code 1 git replay --onto topic1 B..conflict
'

test_expect_success 'using replay on bare repo to rebase with a conflict' '
	test_expect_code 1 git -C bare replay --onto topic1 B..conflict
'

test_expect_success 'using replay to perform basic cherry-pick' '
	# The differences between this test and previous ones are:
	#   --advance vs --onto
	# 2nd field of result is refs/heads/main vs. refs/heads/topic2
	# 4th field of result is hash for main instead of hash for topic2

	git replay --ref-action=print --advance main topic1..topic2 >result &&

	test_line_count = 1 result &&

	git log --format=%s $(cut -f 3 -d " " result) >actual &&
	test_write_lines E D M L B A >expect &&
	test_cmp expect actual &&

	printf "update refs/heads/main " >expect &&
	printf "%s " $(cut -f 3 -d " " result) >>expect &&
	git rev-parse main >>expect &&

	test_cmp expect result
'

test_expect_success 'using replay on bare repo to perform basic cherry-pick' '
	git -C bare replay --ref-action=print --advance main topic1..topic2 >result-bare &&
	test_cmp expect result-bare
'

test_expect_success 'replay on bare repo fails with both --advance and --onto' '
	test_must_fail git -C bare replay --advance main --onto main topic1..topic2 >result-bare
'

test_expect_success 'replay fails when both --advance and --onto are omitted' '
	test_must_fail git replay topic1..topic2 >result
'

test_expect_success 'using replay to also rebase a contained branch' '
	git replay --ref-action=print --contained --onto main main..topic3 >result &&

	test_line_count = 2 result &&
	cut -f 3 -d " " result >new-branch-tips &&

	git log --format=%s $(head -n 1 new-branch-tips) >actual &&
	test_write_lines F C M L B A >expect &&
	test_cmp expect actual &&

	git log --format=%s $(tail -n 1 new-branch-tips) >actual &&
	test_write_lines H G F C M L B A >expect &&
	test_cmp expect actual &&

	printf "update refs/heads/topic1 " >expect &&
	printf "%s " $(head -n 1 new-branch-tips) >>expect &&
	git rev-parse topic1 >>expect &&
	printf "update refs/heads/topic3 " >>expect &&
	printf "%s " $(tail -n 1 new-branch-tips) >>expect &&
	git rev-parse topic3 >>expect &&

	test_cmp expect result
'

test_expect_success 'using replay on bare repo to also rebase a contained branch' '
	git -C bare replay --ref-action=print --contained --onto main main..topic3 >result-bare &&
	test_cmp expect result-bare
'

test_expect_success 'using replay to rebase multiple divergent branches' '
	git replay --ref-action=print --onto main ^topic1 topic2 topic4 >result &&

	test_line_count = 2 result &&
	cut -f 3 -d " " result >new-branch-tips &&

	git log --format=%s $(head -n 1 new-branch-tips) >actual &&
	test_write_lines E D M L B A >expect &&
	test_cmp expect actual &&

	git log --format=%s $(tail -n 1 new-branch-tips) >actual &&
	test_write_lines J I M L B A >expect &&
	test_cmp expect actual &&

	printf "update refs/heads/topic2 " >expect &&
	printf "%s " $(head -n 1 new-branch-tips) >>expect &&
	git rev-parse topic2 >>expect &&
	printf "update refs/heads/topic4 " >>expect &&
	printf "%s " $(tail -n 1 new-branch-tips) >>expect &&
	git rev-parse topic4 >>expect &&

	test_cmp expect result
'

test_expect_success 'using replay on bare repo to rebase multiple divergent branches, including contained ones' '
	git -C bare replay --ref-action=print --contained --onto main ^main topic2 topic3 topic4 >result &&

	test_line_count = 4 result &&
	cut -f 3 -d " " result >new-branch-tips &&

	>expect &&
	for i in 2 1 3 4
	do
		printf "update refs/heads/topic$i " >>expect &&
		printf "%s " $(grep topic$i result | cut -f 3 -d " ") >>expect &&
		git -C bare rev-parse topic$i >>expect || return 1
	done &&

	test_cmp expect result &&

	test_write_lines F C M L B A >expect1 &&
	test_write_lines E D C M L B A >expect2 &&
	test_write_lines H G F C M L B A >expect3 &&
	test_write_lines J I M L B A >expect4 &&

	for i in 1 2 3 4
	do
		git -C bare log --format=%s $(grep topic$i result | cut -f 3 -d " ") >actual &&
		test_cmp expect$i actual || return 1
	done
'

test_expect_success 'merge.directoryRenames=false' '
	# create a test case that stress-tests the rename caching
	git switch -c rename-onto &&

	mkdir -p to-rename &&
	test_commit to-rename/move &&

	mkdir -p renamed-directory &&
	git mv to-rename/move* renamed-directory/ &&
	test_tick &&
	git commit -m renamed-directory &&

	git switch -c rename-from HEAD^ &&
	test_commit to-rename/add-a-file &&
	echo modified >to-rename/add-a-file.t &&
	test_tick &&
	git commit -m modified to-rename/add-a-file.t &&

	git -c merge.directoryRenames=false replay \
		--onto rename-onto rename-onto..rename-from
'

test_expect_success 'default atomic behavior updates refs directly' '
	# Use a separate branch to avoid contaminating topic2 for later tests
	git branch test-atomic topic2 &&
	test_when_finished "git branch -D test-atomic" &&

	# Test default atomic behavior (no output, refs updated)
	git replay --onto main topic1..test-atomic >output &&
	test_must_be_empty output &&

	# Verify ref was updated
	git log --format=%s test-atomic >actual &&
	test_write_lines E D M L B A >expect &&
	test_cmp expect actual &&

	# Verify reflog message includes SHA of onto commit
	git reflog test-atomic -1 --format=%gs >reflog-msg &&
	ONTO_SHA=$(git rev-parse main) &&
	echo "replay --onto $ONTO_SHA" >expect-reflog &&
	test_cmp expect-reflog reflog-msg
'

test_expect_success 'atomic behavior in bare repository' '
	# Store original state for cleanup
	START=$(git -C bare rev-parse topic2) &&
	test_when_finished "git -C bare update-ref refs/heads/topic2 $START" &&

	# Test atomic updates work in bare repo
	git -C bare replay --onto main topic1..topic2 >output &&
	test_must_be_empty output &&

	# Verify ref was updated in bare repo
	git -C bare log --format=%s topic2 >actual &&
	test_write_lines E D M L B A >expect &&
	test_cmp expect actual
'

test_expect_success 'reflog message for --advance mode' '
	# Store original state
	START=$(git rev-parse main) &&
	test_when_finished "git update-ref refs/heads/main $START" &&

	# Test --advance mode reflog message
	git replay --advance main topic1..topic2 >output &&
	test_must_be_empty output &&

	# Verify reflog message includes --advance and branch name
	git reflog main -1 --format=%gs >reflog-msg &&
	echo "replay --advance main" >expect-reflog &&
	test_cmp expect-reflog reflog-msg
'

test_expect_success 'replay.refAction=print config option' '
	# Store original state
	START=$(git rev-parse topic2) &&
	test_when_finished "git branch -f topic2 $START" &&

	# Test with config set to print
	test_config replay.refAction print &&
	git replay --onto main topic1..topic2 >output &&
	test_line_count = 1 output &&
	test_grep "^update refs/heads/topic2 " output
'

test_expect_success 'replay.refAction=update config option' '
	# Store original state
	START=$(git rev-parse topic2) &&
	test_when_finished "git branch -f topic2 $START" &&

	# Test with config set to update
	test_config replay.refAction update &&
	git replay --onto main topic1..topic2 >output &&
	test_must_be_empty output &&

	# Verify ref was updated
	git log --format=%s topic2 >actual &&
	test_write_lines E D M L B A >expect &&
	test_cmp expect actual
'

test_expect_success 'command-line --ref-action overrides config' '
	# Store original state
	START=$(git rev-parse topic2) &&
	test_when_finished "git branch -f topic2 $START" &&

	# Set config to update but use --ref-action=print
	test_config replay.refAction update &&
	git replay --ref-action=print --onto main topic1..topic2 >output &&
	test_line_count = 1 output &&
	test_grep "^update refs/heads/topic2 " output
'

test_expect_success 'invalid replay.refAction value' '
	test_config replay.refAction invalid &&
	test_must_fail git replay --onto main topic1..topic2 2>error &&
	test_grep "invalid.*replay.refAction.*value" error
'

test_expect_success 'using replay with --revert to revert a commit' '
	# Revert commits D and E from topic2
	git replay --revert --onto topic1 topic1..topic2 >result &&

	test_line_count = 1 result &&
	NEW_TOPIC2=$(cut -f 3 -d " " result) &&

	# Verify the result updates the topic2 branch
	printf "update refs/heads/topic2 " >expect &&
	printf "%s " $NEW_TOPIC2 >>expect &&
	git rev-parse topic2 >>expect &&

	test_cmp expect result &&

	# Verify the commit messages contain "Revert"
	# topic1..topic2 contains D and E, so we get 2 reverts on top of topic1 (which has F, C, B, A)
	git log --format=%s $NEW_TOPIC2 >actual &&
	test_line_count = 6 actual &&
	head -n 1 actual >first-line &&
	test_grep "^Revert" first-line
'

test_expect_success 'using replay with --revert on bare repo' '
	git -C bare replay --revert --onto topic1 topic1..topic2 >result-bare &&

	test_line_count = 1 result-bare &&
	NEW_COMMIT=$(cut -f 3 -d " " result-bare) &&

	# Verify the commit message contains "Revert"
	git -C bare log --format=%s $NEW_COMMIT >actual-bare &&
	test_line_count = 6 actual-bare &&
	head -n 1 actual-bare >first-line-bare &&
	test_grep "^Revert" first-line-bare
'

test_expect_success 'using replay with --revert and --advance' '
	# Revert commits from topic2 and advance main
	git replay --revert --advance main topic1..topic2 >result &&

	test_line_count = 1 result &&
	NEW_MAIN=$(cut -f 3 -d " " result) &&

	# Verify the result updates the main branch
	printf "update refs/heads/main " >expect &&
	printf "%s " $NEW_MAIN >>expect &&
	git rev-parse main >>expect &&

	test_cmp expect result &&

	# Verify the commit message contains "Revert"
	git log --format=%s $NEW_MAIN >actual &&
	head -n 1 actual >first-line &&
	test_grep "^Revert" first-line
'

test_expect_success 'replay with --revert fails with --contained' '
	test_must_fail git replay --revert --contained --onto main main..topic3 2>error &&
	test_grep "revert.*contained.*cannot be used together" error
'

test_expect_success 'verify revert actually reverses changes' '
	# Create a branch with a simple change
	git switch -c revert-test main &&
	echo "new content" >test-file.txt &&
	git add test-file.txt &&
	git commit -m "Add test file" &&

	# Revert the commit
	git replay --revert --advance revert-test HEAD^..HEAD >result &&
	REVERTED=$(cut -f 3 -d " " result) &&

	# The file should no longer exist (reverted)
	test_must_fail git show $REVERTED:test-file.txt
'

test_expect_success 'revert of a revert creates reapply message' '
	# Create a commit
	git switch -c revert-revert main &&
	echo "content" >revert-test-2.txt &&
	git add revert-test-2.txt &&
	git commit -m "Add revert test file" &&

	ORIGINAL=$(git rev-parse HEAD) &&

	# First revert
	git replay --revert --advance revert-revert HEAD^..HEAD >result1 &&
	FIRST_REVERT=$(cut -f 3 -d " " result1) &&

	# Check first revert message starts with "Revert"
	git log --format=%s -1 $FIRST_REVERT >msg1 &&
	test_grep "^Revert" msg1 &&

	# Now revert the revert
	git replay --revert --advance revert-revert $ORIGINAL..$FIRST_REVERT >result2 &&
	REAPPLY=$(cut -f 3 -d " " result2) &&

	# Check second revert message starts with "Reapply"
	git log --format=%s -1 $REAPPLY >msg2 &&
	test_grep "^Reapply" msg2 &&

	# The file should exist again (reapplied)
	git show $REAPPLY:revert-test-2.txt >actual &&
	echo "content" >expected &&
	test_cmp expected actual
'

test_expect_success 'replay --revert includes commit SHA in message' '
	git switch -c revert-sha-test main &&
	echo "test" >sha-test.txt &&
	git add sha-test.txt &&
	git commit -m "Test commit for SHA" &&

	COMMIT_SHA=$(git rev-parse HEAD) &&
	git replay --revert --advance revert-sha-test HEAD^..HEAD >result &&
	REVERT_COMMIT=$(cut -f 3 -d " " result) &&

	# Check that the commit message includes the original SHA
	git log --format=%B -1 $REVERT_COMMIT >msg &&
	test_grep "$COMMIT_SHA" msg
'

test_expect_success 'replay --revert with conflict' '
	# Create a conflicting situation
	git switch -c revert-conflict main &&
	echo "line1" >conflict-file.txt &&
	git add conflict-file.txt &&
	git commit -m "Add conflict file" &&

	git switch -c revert-conflict-branch HEAD^ &&
	echo "different" >conflict-file.txt &&
	git add conflict-file.txt &&
	git commit -m "Different content" &&

	# Try to revert the first commit onto the conflicting branch
	test_expect_code 1 git replay --revert --onto revert-conflict-branch revert-conflict^..revert-conflict
'

test_expect_success 'replay --revert handles multiple commits' '
	# Verify that reverting multiple commits works correctly
	# The output should show both revert commits in the history
	git log --format=%s topic2 >topic2-log &&
	test_write_lines E D C B A >expected-topic2 &&
	test_cmp expected-topic2 topic2-log &&

	# Revert D and E from topic2, applying the reverts onto topic1
	git replay --revert --onto topic1 topic1..topic2 >result &&

	test_line_count = 1 result &&
	FINAL=$(cut -f 3 -d " " result) &&

	# Verify both revert commits appear in the log
	git log --format=%s $FINAL >log &&
	head -n 2 log >first-two &&
	test_grep "^Revert" first-two &&

	# Verify we have both "Revert D" and "Revert E"
	test_grep "Revert.*E" log &&
	test_grep "Revert.*D" log
'

test_done
