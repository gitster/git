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

	test_expect_success "keyvalue: $label" '
		eval "$init_command keyvalue-$repo_name" &&
		echo "$key=$expected_value" >expected &&
		git -C keyvalue-$repo_name repo info "$key" >actual &&
		test_cmp expected actual
	'

	test_expect_success "nul: $label" '
		eval "$init_command nul-$repo_name" &&
		printf "%s\n%s\0" "$key" "$expected_value" >expected &&
		git -C nul-$repo_name repo info --format=nul "$key" >actual &&
		test_cmp expected actual
	'
}

test_repo_info 'ref format files is retrieved correctly' '
	git init --ref-format=files' 'format-files' 'references.format' 'files'

test_repo_info 'ref format reftable is retrieved correctly' '
	git init --ref-format=reftable' 'format-reftable' 'references.format' 'reftable'

test_repo_info 'bare repository = false is retrieved correctly' '
	git init' 'bare' 'layout.bare' 'false'

test_repo_info 'bare repository = true is retrieved correctly' '
	git init --bare' 'nonbare' 'layout.bare' 'true'

test_repo_info 'shallow repository = false is retrieved correctly' '
	git init' 'nonshallow' 'layout.shallow' 'false'

test_repo_info 'shallow repository = true is retrieved correctly' '
	test_when_finished "rm -rf remote" &&
	git init remote &&
	echo x >remote/x &&
	git -C remote add x &&
	git -C remote commit -m x &&
	git clone --depth 1 "file://$PWD/remote"' 'shallow' 'layout.shallow' 'true'

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

test_expect_success 'output is returned correctly when two keys are requested' '
	cat >expect <<-\EOF &&
	layout.bare=false
	references.format=files
	EOF
	git init --ref-format=files two-keys &&
	git -C two-keys repo info layout.bare references.format
'

test_expect_success 'git-repo-info aborts when requesting an invalid format' '
	test_when_finished "rm -f err expected" &&
	echo "fatal: invalid format '\'foo\''" >expected &&
	test_must_fail git repo info --format=foo 2>err &&
	test_cmp expected err
'

test_done
