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

	test_expect_success "$label" '
		test_when_finished "rm -rf repo" &&
		eval "$init_command" &&
		echo "$expected_value" | lf_to_nul >expected &&
		git -C repo repo info "$key" >output &&
		tail -n 1 output >actual &&
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

test_expect_success "only one value is returned if the same key is requested twice" '
	echo "references.format" > expected &&
	git rev-parse --show-ref-format > ref-format &&
	lf_to_nul <ref-format >>expected &&
	git repo info references.format references.format > actual &&
	test_cmp expected actual
'

test_done
