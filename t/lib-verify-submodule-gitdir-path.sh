# Helper to verify if repo $1 contains a submodule named $2 with gitdir in path $3

verify_submodule_gitdir_path() {
	repo="$1" &&
	name="$2" &&
	path="$3" &&
	(
		cd "$repo" &&
		cat >expect <<-EOF &&
			$(git rev-parse --git-common-dir)/$path
		EOF
		git submodule--helper gitdir "$name" >actual &&
		test_cmp expect actual
	)
}
