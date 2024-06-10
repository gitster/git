#!/bin/sh

test_description='handling of missing objects in upload-pack'

. ./test-lib.sh

# Setup the repository with three commits, this way HEAD is always
# available and we can hide commit 1 or 2.
test_expect_success 'setup: create "template" repository' '
	git init template &&
	test_commit -C template 1 &&
	test_commit -C template 2 &&
	test_commit -C template 3 &&
	test-tool genrandom foo 10240 >template/foo &&
	git -C template add foo &&
	git -C template commit -m foo
'

# A bare repo will act as a server repo with unpacked objects.
test_expect_success 'setup: create bare "server" repository' '
	git clone --bare --no-local template server &&
	mv server/objects/pack/pack-* . &&
	packfile=$(ls pack-*.pack) &&
	git -C server unpack-objects --strict <"$packfile"
'

# Fetching with 'uploadpack.missingAction=allow-any' only works with
# blobs, as `git pack-objects --missing=allow-any` fails if a missing
# object is not a blob.
test_expect_success "fetch with uploadpack.missingAction=allow-any" '
	oid="$(git -C server rev-parse HEAD:1.t)" &&
	oid_path="$(test_oid_to_path $oid)" &&
	path="server/objects/$oid_path" &&

	mv "$path" "$path.hidden" &&
	test_when_finished "mv $path.hidden $path" &&

	git init client &&
	test_when_finished "rm -rf client" &&

	# Client needs the missing objects to be available somehow
	client_path="client/.git/objects/$oid_path" &&
	mkdir -p $(dirname "$client_path") &&
	cp "$path.hidden" "$client_path" &&

	test_must_fail git -C client fetch ../server &&
	git -C server config uploadpack.missingAction error &&
	test_must_fail git -C client fetch ../server &&
	git -C server config uploadpack.missingAction allow-any &&
	git -C client fetch ../server &&
	git -C server config --unset uploadpack.missingAction
'

check_missing_objects () {
	git -C "$1" rev-list --objects --all --missing=print > all.txt &&
	perl -ne 'print if s/^[?]//' all.txt >missing.txt &&
	test_line_count = "$2" missing.txt &&
	test "$3" = "$(cat missing.txt)"
}

test_expect_success "setup for testing uploadpack.missingAction=allow-promisor" '
	# Create another bare repo called "server2"
	git init --bare server2 &&

	# Copy the largest object from server to server2
	obj="HEAD:foo" &&
	oid="$(git -C server rev-parse $obj)" &&
	oid_path="$(test_oid_to_path $oid)" &&
	path="server/objects/$oid_path" &&
	path2="server2/objects/$oid_path" &&
	mkdir -p $(dirname "$path2") &&
	cp "$path" "$path2" &&

	# Repack everything first
	git -C server -c repack.writebitmaps=false repack -a -d &&

	# Repack without the largest object and create a promisor pack on server
	git -C server -c repack.writebitmaps=false repack -a -d \
	    --filter=blob:limit=5k --filter-to="$(pwd)" &&
	promisor_file=$(ls server/objects/pack/*.pack | sed "s/\.pack/.promisor/") &&
	> "$promisor_file" &&

	# Check that only one object is missing on the server
	check_missing_objects server 1 "$oid" &&

	# Configure server2 as promisor remote for server
	git -C server remote add server2 "file://$(pwd)/server2" &&
	git -C server config remote.server2.promisor true &&

	git -C server2 config uploadpack.allowFilter true &&
	git -C server2 config uploadpack.allowAnySHA1InWant true &&
	git -C server config uploadpack.allowFilter true &&
	git -C server config uploadpack.allowAnySHA1InWant true
'

test_expect_success "fetch with uploadpack.missingAction=allow-promisor" '
	git -C server config uploadpack.missingAction allow-promisor &&

	# Clone from server to create a client
	GIT_NO_LAZY_FETCH=0 git clone -c remote.server2.promisor=true \
		-c remote.server2.fetch="+refs/heads/*:refs/remotes/server2/*" \
		-c remote.server2.url="file://$(pwd)/server2" \
		--no-local --filter="blob:limit=5k" server client &&
	test_when_finished "rm -rf client" &&

	# Check that the largest object is still missing on the server
	check_missing_objects server 1 "$oid"
'

test_expect_success "fetch without uploadpack.missingAction=allow-promisor" '
	git -C server config --unset uploadpack.missingAction &&

	# Clone from server to create a client
	GIT_NO_LAZY_FETCH=0 git clone -c remote.server2.promisor=true \
		-c remote.server2.fetch="+refs/heads/*:refs/remotes/server2/*" \
		-c remote.server2.url="file://$(pwd)/server2" \
		--no-local --filter="blob:limit=5k" server client &&
	test_when_finished "rm -rf client" &&

	# Check that the largest object is not missing on the server anymore
	check_missing_objects server 0 ""
'

test_done
