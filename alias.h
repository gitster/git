#ifndef ALIAS_H
#define ALIAS_H

struct string_list;

char *alias_lookup(const char *alias);
int split_cmdline(char *cmdline, const char ***argv);
/* Takes a negative value returned by split_cmdline */
const char *split_cmdline_strerror(int cmdline_errno);
void list_aliases(struct string_list *list);

#endif

An early preview release Git v2.25.0-rc0 is now available for
testing at the usual places.  It is comprised of 531 non-merge
commits since v2.24.0, contributed by 61 people, 24 of which are
new faces.

The tarballs are found at:

    https://www.kernel.org/pub/software/scm/git/testing/

The following public repositories all have a copy of the
'v2.25.0-rc0' tag and the 'master' branch that the tag points at:

  url = https://kernel.googlesource.com/pub/scm/git/git
  url = git://repo.or.cz/alt-git.git
  url = https://github.com/gitster/git

New contributors whose contributions weren't in v2.24.0 are as follows.
Welcome to the Git development community!

  Colin Stolley, Doan Tran Cong Danh, Dominic Jäger, Erik Chen,
  Hariom Verma, Heba Waly, James Coglan, James Shubin, Josh
  Holland, Łukasz Niemier, Manish Goregaokar, Matthew Rogers,
  Mihail Atanassov, Miriam Rubio, Nathan Stocks, Naveen Nathan,
  Nika Layzell, Philippe Blain, Prarit Bhargava, r.burenkov,
  Ruud van Asseldonk, ryenus, Slavica Đukić, and Utsav Shah.

Returning contributors who helped this release are as follows.
Thanks for your continued support.

  Alban Gruin, Alexandr Miloslavskiy, Andreas Schwab, Andrei Rybak,
  brian m. carlson, Daniel Ferreira, Denton Liu, Derrick Stolee,
  Dimitriy Ryazantcev, Ed Maste, Elia Pinto, Elijah Newren, Emily
  Shaffer, Garima Singh, Hans Jerry Illikainen, Jean-Noël Avila,
  Jeff Hostetler, Jeff King, Johannes Schindelin, Jonathan Nieder,
  Jonathan Tan, Junio C Hamano, Kevin Willford, Martin Ågren,
  Mike Hommey, Philip Oakley, Phillip Wood, Pratyush Yadav,
  Ralf Thielow, René Scharfe, Robin H. Johnson, Rohit Ashiwal,
  SZEDER Gábor, Tanushree Tumane, Thomas Gummerer, Todd Zullinger,
  and William Baker.

----------------------------------------------------------------

Git 2.25 Release Notes (draft)
==============================

Updates since v2.24
-------------------

Backward compatibility notes


UI, Workflows & Features

 * A tutorial on object enumeration has been added.

 * The branch description ("git branch --edit-description") has been
   used to fill the body of the cover letters by the format-patch
   command; this has been enhanced so that the subject can also be
   filled.

 * "git rebase --preserve-merges" has been marked as deprecated; this
   release stops advertising it in the "git rebase -h" output.

 * The code to generate multi-pack index learned to show (or not to
   show) progress indicators.

 * "git apply --3way" learned to honor merge.conflictStyle
   configuration variable, like merges would.

 * The custom format for "git log --format=<format>" learned the l/L
   placeholder that is similar to e/E that fills in the e-mail
   address, but only the local part on the left side of '@'.

 * Documentation pages for "git shortlog" now list commit limiting
   options explicitly.

 * The patterns to detect function boundary for Elixir language has
   been added.

 * The completion script (in contrib/) learned that the "--onto"
   option of "git rebase" can take its argument as the value of the
   option.

 * The userdiff machinery has been taught that "async def" is another
   way to begin a "function" in Python.

 * "git range-diff" learned to take the "--notes=<ref>" and the
   "--no-notes" options to control the commit notes included in the
   log message that gets compared.

 * "git rev-parse --show-toplevel" run outside of any working tree did
   not error out, which has been corrected.

 * A few commands learned to take the pathspec from the standard input
   or a named file, instead of taking it as the command line
   arguments, with the "--pathspec-from-file" option.

 * "git rebase -i" learned a few options that are known by "git
   rebase" proper.

 * "git submodule" learned a subcommand "set-url".

 * "git log" family learned "--pretty=reference" that gives the name
   of a commit in the format that is often used to refer to it in log
   messages.

 * The interaction between "git clone --recurse-submodules" and
   alternate object store was ill-designed.  The documentation and
   code have been taught to make more clear recommendations when the
   users see failures.

 * Management of sparsely checked-out working tree has gained a
   dedicated "sparse-checkout" command.


Performance, Internal Implementation, Development Support etc.

 * Debugging support for lazy cloning has been a bit improved.

 * Move the definition of a set of bitmask constants from 0ctal
   literal to (1U<<count) notation.

 * Test updates to prepare for SHA-2 transition continues.

 * Crufty code and logic accumulated over time around the object
   parsing and low-level object access used in "git fsck" have been
   cleaned up.

 * The implementation of "git log --graph" got refactored and then its
   output got simplified.

 * Follow recent push to move API docs from Documentation/ to header
   files and update config.h

 * "git bundle" has been taught to use the parse options API.  "git
   bundle verify" learned "--quiet" and "git bundle create" learned
   options to control the progress output.

 * Handling of commit objects that use non UTF-8 encoding during
   "rebase -i" has been improved.

 * The beginning of rewriting "git add -i" in C.

 * A label used in the todo list that are generated by "git rebase
   --rebase-merges" is used as a part of a refname; the logic to come
   up with the label has been tightened to avoid names that cannot be
   used as such.

 * The logic to avoid duplicate label names generated by "git rebase
   --rebase-merges" forgot that the machinery itself uses "onto" as a
   label name, which must be avoided by auto-generated labels, which
   has been corrected.

 * We have had compatibility fallback macro definitions for "PRIuMAX",
   "PRIu32", etc. but did not for "PRIdMAX", while the code used the
   last one apparently without any hiccup reported recently.  The
   fallback macro definitions for these <inttypes.h> macros that must
   appear in C99 systems have been removed.

 * Recently we have declared that GIT_TEST_* variables take the
   usual boolean values (it used to be that some used "non-empty
   means true" and taking GIT_TEST_VAR=YesPlease as true); make
   sure we notice and fail when non-bool strings are given to
   these variables.

 * Users of oneway_merge() (like "reset --hard") learned to take
   advantage of fsmonitor to avoid unnecessary lstat(2) calls.

 * Performance tweak on "git push" into a repository with many refs
   that point at objects we have never heard of.

 * PerfTest fix to avoid stale result mixed up with the latest round
   of test results.

 * Hide lower-level verify_signed-buffer() API as a pure helper to
   implement the public check_signature() function, in order to
   encourage new callers to use the correct and more strict
   validation.

 * Unnecessary reading of state variables back from the disk during
   sequencer operation has been reduced.

 * The code has been made to avoid gmtime() and localtime() and prefer
   their reentrant counterparts.

 * The effort to reimplement "git add -i" in C continues.

 * In a repository with many packfiles, the cost of the procedure that
   avoids registering the same packfile twice was unnecessarily high
   by using an inefficient search algorithm, which has been corrected.

 * Redo "git name-rev" to avoid recursive calls.


Fixes since v2.24
-----------------

 * "rebase -i" ceased to run post-commit hook by mistake in an earlier
   update, which has been corrected.

 * "git notes copy $original" ought to copy the notes attached to the
   original object to HEAD, but a mistaken tightening to command line
   parameter validation made earlier disabled that feature by mistake.

 * When all files from some subdirectory were renamed to the root
   directory, the directory rename heuristics would fail to detect that
   as a rename/merge of the subdirectory to the root directory, which has
   been corrected.

 * Code clean-up and a bugfix in the logic used to tell worktree local
   and repository global refs apart.
   (merge f45f88b2e4 sg/dir-trie-fixes later to maint).

 * "git stash save" in a working tree that is sparsely checked out
   mistakenly removed paths that are outside the area of interest.
   (merge 4a58c3d7f7 js/update-index-ignore-removal-for-skip-worktree later to maint).

 * "git rev-parse --git-path HEAD.lock" did not give the right path
   when run in a secondary worktree.
   (merge 76a53d640f js/git-path-head-dot-lock-fix later to maint).

 * "git merge --no-commit" needs "--no-ff" if you do not want to move
   HEAD, which has been corrected in the manual page for "git bisect".
   (merge 8dd327b246 ma/bisect-doc-sample-update later to maint).

 * "git worktree add" internally calls "reset --hard" that should not
   descend into submodules, even when submodule.recurse configuration
   is set, but it was affected.  This has been corrected.
   (merge 4782cf2ab6 pb/no-recursive-reset-hard-in-worktree-add later to maint).

 * Messages from die() etc. can be mixed up from multiple processes
   without even line buffering on Windows, which has been worked
   around.
   (merge 116d1fa6c6 js/vreportf-wo-buffering later to maint).

 * HTTP transport had possible allocator/deallocator mismatch, which
   has been corrected.

 * The watchman integration for fsmonitor was racy, which has been
   corrected to be more conservative.
   (merge dd0b61f577 kw/fsmonitor-watchman-fix later to maint).

 * Fetching from multiple remotes into the same repository in parallel
   had a bad interaction with the recent change to (optionally) update
   the commit-graph after a fetch job finishes, as these parallel
   fetches compete with each other.  Which has been corrected.

 * Recent update to "git stash pop" made the command empty the index
   when run with the "--quiet" option, which has been corrected.

 * "git fetch" codepath had a big "do not lazily fetch missing objects
   when I ask if something exists" switch.  This has been corrected by
   marking the "does this thing exist?" calls with "if not please do not
   lazily fetch it" flag.

 * Test update to avoid wasted cycles.
   (merge e0316695ec sg/skip-skipped-prereq later to maint).

 * Error handling after "git push" finishes sending the packdata and
   waits for the response to the remote side has been improved.
   (merge ad7a403268 jk/send-pack-remote-failure later to maint).

 * Some codepaths in "gitweb" that forgot to escape URLs generated
   based on end-user input have been corrected.
   (merge a376e37b2c jk/gitweb-anti-xss later to maint).

 * CI jobs for macOS has been made less chatty when updating perforce
   package used during testing.
   (merge 0dbc4a0edf jc/azure-ci-osx-fix-fix later to maint).

 * "git unpack-objects" used to show progress based only on the number
   of received and unpacked objects, which stalled when it has to
   handle an unusually large object.  It now shows the throughput as
   well.
   (merge bae60ba7e9 sg/unpack-progress-throughput later to maint).

 * The sequencer machinery compared the HEAD and the state it is
   attempting to commit to decide if the result would be a no-op
   commit, even when amending a commit, which was incorrect, and
   has been corrected.

 * The code to parse GPG output used to assume incorrectly that the
   finterprint for the primary key would always be present for a valid
   signature, which has been corrected.
   (merge 67a6ea6300 hi/gpg-optional-pkfp-fix later to maint).

 * "git submodule status" and "git submodule status --cached" show
   different things, but the documentation did not cover them
   correctly, which has been corrected.
   (merge 8d483c8408 mg/doc-submodule-status-cached later to maint).

 * "git reset --patch $object" without any pathspec should allow a
   tree object to be given, but incorrectly required a committish,
   which has been corrected.

 * "git submodule status" that is run from a subdirectory of the
   superproject did not work well, which has been corrected.
   (merge 1f3aea22c7 mg/submodule-status-from-a-subdirectory later to maint).

 * The revision walking machinery uses resources like per-object flag
   bits that need to be reset before a new iteration of walking
   begins, but the resources related to topological walk were not
   cleared correctly, which has been corrected.
   (merge 0aa0c2b2ec mh/clear-topo-walk-upon-reset later to maint).

 * TravisCI update.
   (merge 176441bfb5 sg/osx-force-gcc-9 later to maint).

 * While running "revert" or "cherry-pick --edit" for multiple
   commits, a recent regression incorrectly detected "nothing to
   commit, working tree clean", instead of replaying the commits,
   which has been corrected.
   (merge befd4f6a81 sg/assume-no-todo-update-in-cherry-pick later to maint).

 * Work around a issue where a FD that is left open when spawning a
   child process and is kept open in the child can interfere with the
   operation in the parent process on Windows.

 * One kind of progress messages were always given during commit-graph
   generation, instead of following the "if it takes more than two
   seconds, show progress" pattern, which has been corrected.

 * "git rebase" did not work well when format.useAutoBase
   configuration variable is set, which has been corrected.

 * The "diff" machinery learned not to lose added/removed blank lines
   in the context when --ignore-blank-lines and --function-context are
   used at the same time.
   (merge 0bb313a552 rs/xdiff-ignore-ws-w-func-context later to maint).

 * The test on "fast-import" used to get stuck when "fast-import" died
   in the middle.
   (merge 0d9b0d7885 sg/t9300-robustify later to maint).

 * "git format-patch" can take a set of configured format.notes values
   to specify which notes refs to use in the log message part of the
   output.  The behaviour of this was not consistent with multiple
   --notes command line options, which has been corrected.
   (merge e0f9095aaa dl/format-patch-notes-config-fixup later to maint).

 * "git p4" used to ignore lfs.storage configuration variable, which
   has been corrected.
   (merge ea94b16fb8 rb/p4-lfs later to maint).

 * Assorted fixes to the directory traversal API.
   (merge 6836d2fe06 en/fill-directory-fixes later to maint).

 * Other code cleanup, docfix, build fix, etc.
   (merge 80736d7c5e jc/am-show-current-patch-docfix later to maint).
   (merge 8b656572ca sg/commit-graph-usage-fix later to maint).
   (merge 6c02042139 mr/clone-dir-exists-to-path-exists later to maint).
   (merge 44ae131e38 sg/blame-indent-heuristics-is-now-the-default later to maint).
   (merge 0115e5d929 dl/doc-diff-no-index-implies-exit-code later to maint).
   (merge 270de6acbe en/t6024-style later to maint).
   (merge 14c4776d75 ns/test-desc-typofix later to maint).
   (merge 68d40f30c4 dj/typofix-merge-strat later to maint).
   (merge f66e0401ab jk/optim-in-pack-idx-conversion later to maint).
   (merge 169bed7421 rs/parse-options-dup-null-fix later to maint).
   (merge 51bd6be32d rs/use-copy-array-in-mingw-shell-command-preparation later to maint).
   (merge b018719927 ma/t7004 later to maint).
   (merge 932757b0cc ar/install-doc-update-cmds-needing-the-shell later to maint).
   (merge 46efd28be1 ep/guard-kset-tar-headers later to maint).
   (merge 9e5afdf997 ec/fetch-mark-common-refs-trace2 later to maint).
   (merge f0e58b3fe8 pb/submodule-update-fetches later to maint).
   (merge 2a02262078 dl/t5520-cleanup later to maint).
   (merge a4fb016ba1 js/pkt-line-h-typofix later to maint).
   (merge 54a7a64613 rs/simplify-prepare-cmd later to maint).
   (merge 3eae30e464 jk/lore-is-the-archive later to maint).
   (merge 14b7664df8 dl/lore-is-the-archive later to maint).
   (merge 0e40a73a4c po/bundle-doc-clonable later to maint).
   (merge e714b898c6 as/t7812-missing-redirects-fix later to maint).
   (merge 528d9e6d01 jk/perf-wo-git-dot-pm later to maint).
   (merge fc42f20e24 sg/test-squelch-noise-in-commit-bulk later to maint).
   (merge c64368e3a2 bc/t9001-zsh-in-posix-emulation-mode later to maint).
   (merge 11de8dd7ef dr/branch-usage-casefix later to maint).
   (merge e05e8cf074 rs/archive-zip-code-cleanup later to maint).
   (merge 147ee35558 rs/commit-export-env-simplify later to maint).
   (merge 4507ecc771 rs/patch-id-use-oid-to-hex later to maint).
   (merge 51a0a4ed95 mr/bisect-use-after-free later to maint).
   (merge cc2bd5c45d pb/submodule-doc-xref later to maint).
   (merge df5be01669 ja/doc-markup-cleanup later to maint).
   (merge 7c5cea7242 mr/bisect-save-pointer-to-const-string later to maint).

----------------------------------------------------------------

Changes since v2.24.0 are as follows:

Alban Gruin (6):
      sequencer: update `total_nr' when adding an item to a todo list
      sequencer: update `done_nr' when skipping commands in a todo list
      sequencer: move the code writing total_nr on the disk to a new function
      rebase: fill `squash_onto' in get_replay_opts()
      sequencer: directly call pick_commits() from complete_action()
      sequencer: fix a memory leak in sequencer_continue()

Alexandr Miloslavskiy (14):
      parse-options.h: add new options `--pathspec-from-file`, `--pathspec-file-nul`
      pathspec: add new function to parse file
      doc: reset: synchronize <pathspec> description
      reset: support the `--pathspec-from-file` option
      doc: commit: synchronize <pathspec> description
      commit: support the --pathspec-from-file option
      cmd_add: prepare for next patch
      add: support the --pathspec-from-file option
      doc: checkout: remove duplicate synopsis
      doc: checkout: fix broken text reference
      doc: checkout: synchronize <pathspec> description
      doc: restore: synchronize <pathspec> description
      checkout, restore: support the --pathspec-from-file option
      commit: forbid --pathspec-from-file --all

Andreas Schwab (1):
      t7812: add missing redirects

Andrei Rybak (1):
      INSTALL: use existing shell scripts as example

Colin Stolley (1):
      packfile.c: speed up loading lots of packfiles

Daniel Ferreira (2):
      diff: export diffstat interface
      built-in add -i: implement the `status` command

Denton Liu (93):
      format-patch: replace erroneous and condition
      format-patch: use enum variables
      format-patch: teach --cover-from-description option
      rebase: hide --preserve-merges option
      t4108: replace create_file with test_write_lines
      t4108: remove git command upstream of pipe
      t4108: use `test_config` instead of `git config`
      t4108: demonstrate bug in apply
      apply: respect merge.conflictStyle in --3way
      submodule: teach set-url subcommand
      git-diff.txt: document return code of `--no-index`
      completion: learn to complete `git rebase --onto=`
      t4215: use helper function to check output
      argv-array: add space after `while`
      rev-list-options.txt: remove reference to --show-notes
      SubmittingPatches: use generic terms for hash
      pretty-formats.txt: use generic terms for hash
      SubmittingPatches: remove dq from commit reference
      completion: complete `tformat:` pretty format
      revision: make get_revision_mark() return const pointer
      pretty.c: inline initalize format_context
      t4205: cover `git log --reflog -z` blindspot
      pretty: add struct cmt_fmt_map::default_date_mode_type
      pretty: implement 'reference' format
      SubmittingPatches: use `--pretty=reference`
      pretty-options.txt: --notes accepts a ref instead of treeish
      t3206: remove spaces after redirect operators
      t3206: disable parameter substitution in heredoc
      t3206: s/expected/expect/
      t3206: range-diff compares logs with commit notes
      range-diff: output `## Notes ##` header
      range-diff: pass through --notes to `git log`
      format-patch: pass notes configuration to range-diff
      t0000: test multiple local assignment
      t: teach test_cmp_rev to accept ! for not-equals
      t5520: improve test style
      t5520: use sq for test case names
      t5520: let sed open its own input
      t5520: replace test -f with test-lib functions
      t5520: remove spaces after redirect operator
      t5520: use test_line_count where possible
      t5520: replace test -{n,z} with test-lib functions
      t5520: use test_cmp_rev where possible
      t5520: test single-line files by git with test_cmp
      t5520: don't put git in upstream of pipe
      t5520: replace $(cat ...) comparison with test_cmp
      t5520: remove redundant lines in test cases
      t5520: replace `! git` with `test_must_fail git`
      lib-bash.sh: move `then` onto its own line
      apply-one-time-sed.sh: modernize style
      t0014: remove git command upstream of pipe
      t0090: stop losing return codes of git commands
      t3301: stop losing return codes of git commands
      t3600: use test_line_count() where possible
      t3600: stop losing return codes of git commands
      t3600: comment on inducing SIGPIPE in `git rm`
      t4015: stop losing return codes of git commands
      t4015: use test_write_lines()
      t4138: stop losing return codes of git commands
      t5317: stop losing return codes of git commands
      t5317: use ! grep to check for no matching lines
      t5703: simplify one-time-sed generation logic
      t5703: stop losing return codes of git commands
      t7501: remove spaces after redirect operators
      t7501: stop losing return codes of git commands
      t7700: drop redirections to /dev/null
      t7700: remove spaces after redirect operators
      t7700: move keywords onto their own line
      t7700: s/test -f/test_path_is_file/
      doc: replace MARC links with lore.kernel.org
      RelNotes: replace Gmane with real Message-IDs
      doc: replace LKML link with lore.kernel.org
      t7700: consolidate code into test_no_missing_in_packs()
      t7700: consolidate code into test_has_duplicate_object()
      t7700: replace egrep with grep
      t7700: make references to SHA-1 generic
      t7700: stop losing return codes of git commands
      t3400: demonstrate failure with format.useAutoBase
      format-patch: fix indentation
      t4014: use test_config()
      format-patch: teach --no-base
      rebase: fix format.useAutoBase breakage
      t3206: fix incorrect test name
      range-diff: mark pointers as const
      range-diff: clear `other_arg` at end of function
      notes: rename to load_display_notes()
      notes: create init_display_notes() helper
      notes: extract logic into set_display_notes()
      format-patch: use --notes behavior for format.notes
      format-patch: move git_config() before repo_init_revisions()
      config/format.txt: clarify behavior of multiple format.notes
      notes: break set_display_notes() into smaller functions
      notes.h: fix typos in comment

Derrick Stolee (22):
      test-tool: use 'read-graph' helper
      sparse-checkout: create builtin with 'list' subcommand
      sparse-checkout: create 'init' subcommand
      clone: add --sparse mode
      sparse-checkout: 'set' subcommand
      sparse-checkout: add '--stdin' option to set subcommand
      sparse-checkout: create 'disable' subcommand
      sparse-checkout: add 'cone' mode
      sparse-checkout: use hashmaps for cone patterns
      sparse-checkout: init and set in cone mode
      unpack-trees: hash less in cone mode
      unpack-trees: add progress to clear_ce_flags()
      sparse-checkout: sanitize for nested folders
      sparse-checkout: update working directory in-process
      sparse-checkout: use in-process update for disable subcommand
      sparse-checkout: write using lockfile
      sparse-checkout: cone mode should not interact with .gitignore
      sparse-checkout: update working directory in-process for 'init'
      sparse-checkout: check for dirty status
      progress: create GIT_PROGRESS_DELAY
      commit-graph: use start_delayed_progress()
      sparse-checkout: respect core.ignoreCase in cone mode

Dimitriy Ryazantcev (1):
      l10n: minor case fix in 'git branch' '--unset-upstream' description

Doan Tran Cong Danh (14):
      t3301: test diagnose messages for too few/many paramters
      notes: fix minimum number of parameters to "copy" subcommand
      t0028: eliminate non-standard usage of printf
      configure.ac: define ICONV_OMITS_BOM if necessary
      t3900: demonstrate git-rebase problem with multi encoding
      sequencer: reencode to utf-8 before arrange rebase's todo list
      sequencer: reencode revert/cherry-pick's todo list
      sequencer: reencode squashing commit's message
      sequencer: reencode old merge-commit message
      sequencer: reencode commit message for am/rebase --show-current-patch
      sequencer: handle rebase-merges for "onto" message
      date.c: switch to reentrant {gm,local}time_r
      archive-zip.c: switch to reentrant localtime_r
      mingw: use {gm,local}time_s as backend for {gm,local}time_r

Dominic Jäger (1):
      merge-strategies: fix typo "reflected to" to "reflected in"

Ed Maste (3):
      t4210: skip i18n tests that don't work on FreeBSD
      userdiff: remove empty subexpression from elixir regex
      sparse-checkout: improve OS ls compatibility

Elia Pinto (1):
      kset.h, tar.h: add missing header guard to prevent multiple inclusion

Elijah Newren (26):
      merge-recursive: clean up get_renamed_dir_portion()
      merge-recursive: fix merging a subdirectory into the root directory
      t604[236]: do not run setup in separate tests
      Documentation: fix a bunch of typos, both old and new
      Fix spelling errors in documentation outside of Documentation/
      git-filter-branch.txt: correct argument name typo
      hashmap: fix documentation misuses of -> versus .
      name-hash.c: remove duplicate word in comment
      t6024: modernize style
      Fix spelling errors in code comments
      Fix spelling errors in comments of testcases
      Fix spelling errors in names of tests
      Fix spelling errors in messages shown to users
      Fix spelling errors in test commands
      sha1dc: fix trivial comment spelling error
      multimail: fix a few simple spelling errors
      Fix spelling errors in no-longer-updated-from-upstream modules
      t3011: demonstrate directory traversal failures
      Revert "dir.c: make 'git-status --ignored' work within leading directories"
      dir: remove stray quote character in comment
      dir: exit before wildcard fall-through if there is no wildcard
      dir: break part of read_directory_recursive() out for reuse
      t3434: mark successful test as such
      dir: fix checks on common prefix directory
      dir: synchronize treat_leading_path() and read_directory_recursive()
      dir: consolidate similar code in treat_directory()

Emily Shaffer (4):
      documentation: add tutorial for object walking
      myfirstcontrib: add 'psuh' to command-list.txt
      myfirstcontrib: add dependency installation step
      myfirstcontrib: hint to find gitgitgadget allower

Erik Chen (1):
      fetch: add trace2 instrumentation

Garima Singh (2):
      test-path-utils: offer to run a protectNTFS/protectHFS benchmark
      tests: add a helper to stress test argument quoting

Hans Jerry Illikainen (4):
      gpg-interface: refactor the free-and-xmemdupz pattern
      gpg-interface: limit search for primary key fingerprint
      gpg-interface: prefer check_signature() for GPG verification
      grep: don't return an expression from pcre2_free()

Hariom Verma (2):
      builtin/blame.c: constants into bit shift format
      git-compat-util.h: drop the `PRIuMAX` and other fallback definitions

Heba Waly (22):
      config: move documentation to config.h
      documentation: remove empty doc files
      diff: move doc to diff.h and diffcore.h
      dir: move doc to dir.h
      graph: move doc to graph.h and graph.c
      merge: move doc to ll-merge.h
      sha1-array: move doc to sha1-array.h
      remote: move doc to remote.h and refspec.h
      refs: move doc to refs.h
      attr: move doc to attr.h
      revision: move doc to revision.h
      pathspec: move doc to pathspec.h
      sigchain: move doc to sigchain.h
      cache: move doc to cache.h
      argv-array: move doc to argv-array.h
      credential: move doc to credential.h
      parse-options: add link to doc file in parse-options.h
      run-command: move doc to run-command.h
      trace: move doc to trace.h
      tree-walk: move doc to tree-walk.h
      submodule-config: move doc to submodule-config.h
      trace2: move doc to trace2.h

James Coglan (13):
      graph: automatically track display width of graph lines
      graph: handle line padding in `graph_next_line()`
      graph: reuse `find_new_column_by_commit()`
      graph: reduce duplication in `graph_insert_into_new_columns()`
      graph: remove `mapping_idx` and `graph_update_width()`
      graph: extract logic for moving to GRAPH_PRE_COMMIT state
      graph: example of graph output that can be simplified
      graph: tidy up display of left-skewed merges
      graph: commit and post-merge lines for left-skewed merges
      graph: rename `new_mapping` to `old_mapping`
      graph: smooth appearance of collapsing edges on commit lines
      graph: flatten edges that fuse with their right neighbor
      graph: fix coloring of octopus dashes

James Shubin (1):
      completion: tab-complete "git svn --recursive"

Jean-Noël Avila (2):
      doc: remove non pure ASCII characters
      doc: indent multi-line items in list

Jeff Hostetler (1):
      trace2: add region in clear_ce_flags

Jeff King (44):
      parse_commit_buffer(): treat lookup_commit() failure as parse error
      parse_commit_buffer(): treat lookup_tree() failure as parse error
      parse_tag_buffer(): treat NULL tag pointer as parse error
      commit, tag: don't set parsed bit for parse failures
      fsck: stop checking commit->tree value
      fsck: stop checking commit->parent counts
      fsck: stop checking tag->tagged
      fsck: require an actual buffer for non-blobs
      fsck: unify object-name code
      fsck_describe_object(): build on our get_object_name() primitive
      fsck: use oids rather than objects for object_name API
      fsck: don't require object structs for display functions
      fsck: only provide oid/type in fsck_error callback
      fsck: only require an oid for skiplist functions
      fsck: don't require an object struct for report()
      fsck: accept an oid instead of a "struct blob" for fsck_blob()
      fsck: drop blob struct from fsck_finish()
      fsck: don't require an object struct for fsck_ident()
      fsck: don't require an object struct in verify_headers()
      fsck: rename vague "oid" local variables
      fsck: accept an oid instead of a "struct tag" for fsck_tag()
      fsck: accept an oid instead of a "struct commit" for fsck_commit()
      fsck: accept an oid instead of a "struct tree" for fsck_tree()
      hex: drop sha1_to_hex_r()
      pack-objects: avoid pointless oe_map_new_pack() calls
      hex: drop sha1_to_hex()
      send-pack: check remote ref status on pack-objects failure
      t9502: pass along all arguments in xss helper
      t/gitweb-lib.sh: drop confusing quotes
      t/gitweb-lib.sh: set $REQUEST_URI
      gitweb: escape URLs generated by href()
      rev-parse: make --show-toplevel without a worktree an error
      perf-lib: use a single filename for all measurement types
      t/perf: don't depend on Git.pm
      send-pack: use OBJECT_INFO_QUICK to check negative objects
      doc: recommend lore.kernel.org over public-inbox.org
      doc: replace public-inbox links with lore.kernel.org
      t9300: drop some useless uses of cat
      t9300: create marks files for double-import-marks test
      fast-import: tighten parsing of boolean command line options
      fast-import: stop creating leading directories for import-marks
      fast-import: delay creating leading directories for export-marks
      fast-import: disallow "feature export-marks" by default
      fast-import: disallow "feature import-marks" by default

Johannes Schindelin (93):
      t1400: wrap setup code in test case
      git_path(): handle `.lock` files correctly
      vreportf(): avoid relying on stdio buffering
      update-index: optionally leave skip-worktree entries alone
      stash: handle staged changes in skip-worktree files correctly
      fetch: add the command-line option `--write-commit-graph`
      fetch: avoid locking issues between fetch.jobs/fetch.writeCommitGraph
      remote-curl: unbreak http.extraHeader with custom allocators
      Start to implement a built-in version of `git add --interactive`
      built-in add -i: implement the main loop
      built-in add -i: show unique prefixes of the commands
      built-in add -i: support `?` (prompt help)
      rebase-merges: move labels' whitespace mangling into `label_oid()`
      git svn: stop using `rebase --preserve-merges`
      mingw: demonstrate that all file handles are inherited by child processes
      mingw: work around incorrect standard handles
      mingw: spawned processes need to inherit only standard handles
      mingw: restrict file handle inheritance only on Windows 7 and later
      mingw: do set `errno` correctly when trying to restrict handle inheritance
      add-interactive: make sure to release `rev.prune_data`
      built-in add -i: allow filtering the modified files list
      built-in add -i: prepare for multi-selection commands
      built-in add -i: implement the `update` command
      built-in add -i: re-implement `revert` in C
      built-in add -i: re-implement `add-untracked` in C
      built-in add -i: implement the `patch` command
      built-in add -i: re-implement the `diff` command
      built-in add -i: offer the `quit` command
      pkt-line: fix a typo
      mingw: forbid translating ERROR_SUCCESS to an errno value
      clone --recurse-submodules: prevent name squatting on Windows
      mingw: disallow backslash characters in tree objects' file names
      path.c: document the purpose of `is_ntfs_dotgit()`
      is_ntfs_dotgit(): only verify the leading segment
      path: safeguard `.git` against NTFS Alternate Streams Accesses
      is_ntfs_dotgit(): speed it up
      mingw: fix quoting of arguments
      path: also guard `.gitmodules` against NTFS Alternate Data Streams
      protect_ntfs: turn on NTFS protection by default
      Disallow dubiously-nested submodule git directories
      quote-stress-test: accept arguments to test via the command-line
      t6130/t9350: prepare for stringent Win32 path validation
      quote-stress-test: allow skipping some trials
      unpack-trees: let merged_entry() pass through do_add_entry()'s errors
      mingw: refuse to access paths with illegal characters
      quote-stress-test: offer to test quoting arguments for MSYS2 sh
      mingw: refuse to access paths with trailing spaces or periods
      mingw: handle `subst`-ed "DOS drives"
      Git 2.14.6
      Git 2.15.4
      test-drop-caches: use `has_dos_drive_prefix()`
      Git 2.16.6
      Git 2.17.3
      Git 2.18.2
      Git 2.19.3
      t7415: adjust test for dubiously-nested submodule gitdirs for v2.20.x
      Git 2.20.2
      mingw: detect when MSYS2's sh is to be spawned more robustly
      mingw: use MSYS2 quoting even when spawning shell scripts
      mingw: fix quoting of empty arguments for `sh`
      t7415: drop v2.20.x-specific work-around
      mingw: sh arguments need quoting in more circumstances
      Git 2.21.1
      Git 2.22.2
      Git 2.23.1
      Git 2.24.1
      t3701: add a test for advanced split-hunk editing
      t3701: avoid depending on the TTY prerequisite
      t3701: add a test for the different `add -p` prompts
      t3701: verify the shown messages when nothing can be added
      t3701: verify that the diff.algorithm config setting is handled
      git add -p: use non-zero exit code when the diff generation failed
      apply --allow-overlap: fix a corner case
      t3404: fix indentation
      built-in add -i: start implementing the `patch` functionality in C
      built-in add -i: wire up the new C code for the `patch` command
      built-in add -p: show colored hunks by default
      built-in add -p: adjust hunk headers as needed
      built-in add -p: color the prompt and the help text
      built-in add -p: offer a helpful error message when hunk navigation failed
      built-in add -p: support multi-file diffs
      built-in add -p: handle deleted empty files
      built-in app -p: allow selecting a mode change as a "hunk"
      built-in add -p: show different prompts for mode changes and deletions
      built-in add -p: implement the hunk splitting feature
      built-in add -p: coalesce hunks after splitting them
      strbuf: add a helper function to call the editor "on an strbuf"
      built-in add -p: implement hunk editing
      built-in add -p: implement the 'g' ("goto") command
      built-in add -p: implement the '/' ("search regex") command
      built-in add -p: implement the 'q' ("quit") command
      built-in add -p: only show the applicable parts of the help text
      built-in add -p: show helpful hint when nothing can be staged

Jonathan Nieder (3):
      submodule: reject submodule.update = !command in .gitmodules
      fsck: reject submodule.update = !command in .gitmodules
      submodule: defend against submodule.update = !command in .gitmodules

Jonathan Tan (6):
      fetch-pack: write fetched refs to .promisor
      fetch: remove fetch_if_missing=0
      clone: remove fetch_if_missing=0
      promisor-remote: remove fetch_if_missing=0
      Doc: explain submodule.alternateErrorStrategy
      submodule--helper: advise on fatal alternate error

Josh Holland (1):
      userdiff: support Python async functions

Junio C Hamano (13):
      doc: am --show-current-patch gives an entire e-mail message
      The first batch post 2.24 cycle
      fsmonitor: do not compare bitmap size with size of split index
      ci(osx): update homebrew-cask repository with less noise
      rebase -i: finishing touches to --reset-author-date
      The second batch
      The third batch
      The fourth batch
      The fifth batch
      Makefile: drop GEN_HDRS
      The sixth batch
      dir.c: use st_add3() for allocation size
      Git 2.25-rc0

Kevin Willford (1):
      fsmonitor: fix watchman integration

Manish Goregaokar (2):
      doc: document 'git submodule status --cached'
      submodule: fix 'submodule status' when called from a subdirectory

Martin Ågren (1):
      t7004: check existence of correct tag

Matthew Rogers (1):
      rebase -r: let `label` generate safer labels

Mihail Atanassov (1):
      Documentation/git-bisect.txt: add --no-ff to merge command

Mike Hommey (2):
      revision: clear the topo-walk flags in reset_revision_walk
      revision: free topo_walk_info before creating a new one in init_topo_walk

Miriam Rubio (1):
      clone: rename static function `dir_exists()`.

Nathan Stocks (1):
      t: fix typo in test descriptions

Naveen Nathan (1):
      doc: improve readability of --rebase-merges in git-rebase

Nika Layzell (1):
      reset: parse rev as tree-ish in patch mode

Philip Oakley (1):
      Doc: Bundle file usage

Philippe Blain (4):
      help: add gitsubmodules to the list of guides
      worktree: teach "add" to ignore submodule.recurse config
      doc: mention that 'git submodule update' fetches missing commits
      gitmodules: link to gitsubmodules guide

Phillip Wood (7):
      t3404: remove unnecessary subshell
      t3404: set $EDITOR in subshell
      t3404: remove uneeded calls to set_fake_editor
      sequencer.h fix placement of #endif
      move run_commit_hook() to libgit and use it there
      sequencer: run post-commit hook
      sequencer: fix empty commit check when amending

Prarit Bhargava (3):
      t6006: use test-lib.sh definitions
      t4203: use test-lib.sh definitions
      pretty: add "%aL" etc. to show local-part of email addresses

Pratyush Yadav (1):
      git-shortlog.txt: include commit limiting options

Ralf Thielow (1):
      fetch.c: fix typo in a warning message

René Scharfe (32):
      trace2: add dots directly to strbuf in perf_fmt_prepare()
      utf8: use skip_iprefix() in same_utf_encoding()
      convert: use skip_iprefix() in validate_encoding()
      mingw: use COPY_ARRAY for copying array
      parse-options: avoid arithmetic on pointer that's potentially NULL
      pretty: provide short date format
      fetch: use skip_prefix() instead of starts_with()
      fmt-merge-msg: use skip_prefix() instead of starts_with()
      shell: use skip_prefix() instead of starts_with()
      push: use skip_prefix() instead of starts_with()
      name-rev: use skip_prefix() instead of starts_with()
      run-command: use prepare_git_cmd() in prepare_cmd()
      t1512: use test_line_count
      t1410: use test_line_count
      t1400: use test_must_be_empty
      test: use test_must_be_empty F instead of test -z $(cat F)
      test: use test_must_be_empty F instead of test_cmp empty F
      t9300: don't create unused file
      t7811: don't create unused file
      xdiff: unignore changes in function context
      name-rev: use strbuf_strip_suffix() in get_rev_name()
      commit: use strbuf_add() to add a length-limited string
      patch-id: use oid_to_hex() to print multiple object IDs
      archive-zip: use enum for compression method
      t4256: don't create unused file
      t7004: don't create unused file
      refs: pass NULL to refs_read_ref_full() because object ID is not needed
      remote: pass NULL to read_ref_full() because object ID is not needed
      t3501: don't create unused file
      t5580: don't create unused file
      t6030: don't create unused file
      t4015: improve coverage of function context test

Robin H. Johnson (3):
      bundle: framework for options before bundle file
      bundle-create: progress output control
      bundle-verify: add --quiet

Rohit Ashiwal (6):
      rebase -i: add --ignore-whitespace flag
      sequencer: allow callers of read_author_script() to ignore fields
      rebase -i: support --committer-date-is-author-date
      sequencer: rename amend_author to author_to_rename
      rebase -i: support --ignore-date
      rebase: add --reset-author-date

Ruud van Asseldonk (1):
      t5150: skip request-pull test if Perl is disabled

SZEDER Gábor (29):
      Documentation: mention more worktree-specific exceptions
      path.c: clarify trie_find()'s in-code comment
      path.c: mark 'logs/HEAD' in 'common_list' as file
      path.c: clarify two field names in 'struct common_dir'
      path.c: don't call the match function without value in trie_find()
      builtin/commit-graph.c: remove subcommand-less usage string
      builtin/blame.c: remove '--indent-heuristic' from usage string
      test-lib: don't check prereqs of test cases that won't be run anyway
      t6120-describe: correct test repo history graph in comment
      builtin/unpack-objects.c: show throughput progress
      tests: add 'test_bool_env' to catch non-bool GIT_TEST_* values
      t5608-clone-2gb.sh: turn GIT_TEST_CLONE_2GB into a bool
      sequencer: don't re-read todo for revert and cherry-pick
      test-lib-functions: suppress a 'git rev-parse' error in 'test_commit_bulk'
      ci: build Git with GCC 9 in the 'osx-gcc' build job
      t9300-fast-import: store the PID in a variable instead of pidfile
      t9300-fast-import: don't hang if background fast-import exits too early
      t6120-describe: modernize the 'check_describe' helper
      name-rev: avoid unnecessary cast in name_ref()
      name-rev: use sizeof(*ptr) instead of sizeof(type) in allocation
      t6120: add a test to cover inner conditions in 'git name-rev's name_rev()
      name-rev: extract creating/updating a 'struct name_rev' into a helper
      name-rev: pull out deref handling from the recursion
      name-rev: restructure parsing commits and applying date cutoff
      name-rev: restructure creating/updating 'struct rev_name' instances
      name-rev: drop name_rev()'s 'generation' and 'distance' parameters
      name-rev: use 'name->tip_name' instead of 'tip_name'
      name-rev: eliminate recursion in name_rev()
      name-rev: cleanup name_ref()

Slavica Đukić (3):
      built-in add -i: color the header in the `status` command
      built-in add -i: use color in the main loop
      built-in add -i: implement the `help` command

Tanushree Tumane (2):
      bisect--helper: avoid use-after-free
      bisect--helper: convert `*_warning` char pointers to char arrays.

Thomas Gummerer (1):
      stash: make sure we have a valid index before writing it

Todd Zullinger (1):
      t7812: expect failure for grep -i with invalid UTF-8 data

Utsav Shah (1):
      unpack-trees: skip stat on fsmonitor-valid files

William Baker (6):
      midx: add MIDX_PROGRESS flag
      midx: add progress to write_midx_file
      midx: add progress to expire_midx_packs
      midx: honor the MIDX_PROGRESS flag in verify_midx_file
      midx: honor the MIDX_PROGRESS flag in midx_repack
      multi-pack-index: add [--[no-]progress] option.

brian m. carlson (16):
      t/oid-info: allow looking up hash algorithm name
      t/oid-info: add empty tree and empty blob values
      rev-parse: add a --show-object-format option
      t1305: avoid comparing extensions
      t3429: remove SHA1 annotation
      t4010: abstract away SHA-1-specific constants
      t4011: abstract away SHA-1-specific constants
      t4015: abstract away SHA-1-specific constants
      t4027: make hash-size independent
      t4034: abstract away SHA-1-specific constants
      t4038: abstract away SHA-1 specific constants
      t4039: abstract away SHA-1-specific constants
      t4044: update test to work with SHA-256
      t4045: make hash-size independent
      t4048: abstract away SHA-1-specific constants
      t9001: avoid including non-trailing NUL bytes in variables

r.burenkov (1):
      git-p4: honor lfs.storage configuration variable

ryenus (1):
      fix-typo: consecutive-word duplications

Łukasz Niemier (1):
      userdiff: add Elixir to supported userdiff languages

