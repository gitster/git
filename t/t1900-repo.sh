#!/bin/sh

test_description='test git repo-info'

. ./test-lib.sh

# Test whether a key-value pair is correctly returned
#
# Usage: test_repo_info <label> <init command> <key> <expected value>
#
# Arguments:
#   label: the label of the test
#   init command: a command which creates a repository named with its first argument,
#      accordingly to what is being tested
#   key: the key of the field that is being tested
#   expected value: the value that the field should contain
test_repo_info () {
	label=$1
	init_command=$2
	repo_name=$3
	key=$4
	expected_value=$5

	test_expect_success "$label" '
		eval "$init_command $repo_name" &&
		echo "$key=$expected_value" >expected &&
		git -C $repo_name repo info "$key" >actual &&
		test_cmp expected actual
	'
}

test_repo_info 'ref format files is retrieved correctly' '
	git init --ref-format=files' 'format-files' 'references.format' 'files'

test_repo_info 'ref format reftable is retrieved correctly' '
	git init --ref-format=reftable' 'format-reftable' 'references.format' 'reftable'

test_expect_success 'git-repo-info fails if an invalid key is requested' '
	echo "error: key '\'foo\'' not found" >expected_err &&
	test_must_fail git repo info foo 2>actual_err &&
	test_cmp expected_err actual_err
'

test_expect_success 'git-repo-info outputs data even if there is an invalid field' '
	echo "references.format=files" >expected &&
	test_must_fail git repo info foo references.format bar >actual &&
	test_cmp expected actual
'

test_expect_success 'only one value is returned if the same key is requested twice' '
	val=$(git rev-parse --show-ref-format) &&
	echo "references.format=$val" >expect &&
	git repo info references.format references.format >actual &&
	test_cmp expect actual
'

test_done
