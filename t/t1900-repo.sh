#!/bin/sh

test_description='test git repo-info'

. ./test-lib.sh

# Test if a field is correctly returned in the null-terminated format
#
# Usage: test_repo_info <label> <init command> <key> <expected value>
#
# Arguments:
#   label: the label of the test
#   init command: a command that creates a repository called 'repo', configured
#      accordingly to what is being tested
#   key: the key of the field that is being tested
#   expected value: the value that the field should contain
test_repo_info () {
	label=$1
	init_command=$2
	key=$3
	expected_value=$4

	test_expect_success "null-terminated: $label" '
		test_when_finished "rm -rf repo" &&
		eval "$init_command" &&
		echo "$expected_value" | lf_to_nul >expected &&
		git -C repo repo info --format=null "$key" >output &&
		tail -n 1 output >actual &&
		test_cmp expected actual
	'

	test_expect_success "key-value: $label" '
		test_when_finished "rm -rf repo" &&
		eval "$init_command" &&
		echo "$expected_value" >expected &&
		git -C repo repo info --format=keyvalue "$key" >output &&
		cut -d "=" -f 2 <output >actual &&
		test_cmp expected actual
	'
}

test_repo_info 'ref format files is retrieved correctly' '
	git init --ref-format=files repo' 'references.format' 'files'

test_repo_info 'ref format reftable is retrieved correctly' '
	git init --ref-format=reftable repo' 'references.format' 'reftable'

test_repo_info 'bare repository = false is retrieved correctly' '
	git init repo' 'layout.bare' 'false'

test_repo_info 'bare repository = true is retrieved correctly' '
	git init --bare repo' 'layout.bare' 'true'

test_repo_info 'shallow repository = false is retrieved correctly' '
	git init repo' 'layout.shallow' 'false'

test_repo_info 'shallow repository = true is retrieved correctly' '
	git init remote &&
	cd remote &&
	echo x >x &&
	git add x &&
	git commit -m x &&
	cd .. &&
	git clone --depth 1 "file://$PWD/remote" repo &&
	rm -rf remote
	' 'layout.shallow' 'true'

test_expect_success "only one value is returned if the same key is requested twice" '
	test_when_finished "rm -f expected_key expected_value actual_key actual_value output" &&
	echo "references.format" >expected_key &&
	git rev-parse --show-ref-format >expected_value &&
	git repo info references.format references.format >output &&
	cut -d "=" -f 1 <output >actual_key &&
	cut -d "=" -f 2 <output >actual_value &&
	test_cmp expected_key actual_key &&
	test_cmp expected_value actual_value
'

test_expect_success 'output is returned correctly when two keys are requested' '
	test_when_finished "rm -f expect" &&
	printf "layout.bare=false\nlayout.shallow=false\n" >expect &&
	git repo info layout.shallow layout.bare >actual &&
	test_cmp expect actual
'

test_done
