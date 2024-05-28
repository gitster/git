# Upcoming breaking changes

The Git project aims to ensure backwards compatibility to the best extent
possible. Minor releases will not break backwards compatibility unless there is
a very strong reason to do so, like for example a security vulnerability.

Regardless of that, due to the age of the Git project, it is only natural to
accumulate a backlog of backwards-incompatible changes that will eventually be
required to keep the project aligned with a changing world. These changes fall
into several categories:

  - Changes to long established defaults.

  - Concepts that have been replaced with a superior design.

  - Concepts, commands, configuration or options that have been lacking in major
    ways and that cannot be fixed.

The Git project will thus irregularly release major versions that deliberately
break backwards compatibility with older versions. This is done to ensure that
Git remains relevant, safe and maintainable going forward. The release cadence
of major versions is typically measured in multiple years.

The intent of this document is to track upcoming deprecations for the next major
Git release. Furthermore, this document also tracks what will _not_ be
deprecated. This is done such that the outcome of discussions documente both
when the discussion favors deprecation, but also when it rejects a deprecation.

It is supposed to be a living document where proposed changes and
deprecations are up for discussion.

## Git 3.0

### Changes

  - The default initial branch name will be changed from "master" to "main".

    Cf. <pull.762.git.1605221038.gitgitgadget@gmail.com>,
    <CAMP44s3BJ3dGsLJ-6yA-Po459=+m826KD9an4+P3qOY1vkbxZg@mail.gmail.com>.

  - The default hash function for new repositories will be changed from "sha1"
    to "sha256".

  - The default ref backend for new repositories will be changed from "files" to
    "reftable".

### Removals

 - git-http-push(1) can be used to push objects to a remote repository via
   HTTP/DAV. Support for write support via WebDAV is not in widespread use
   nowadays anymore and will be removed together with the command.

 - The dumb HTTP protocol can be used to serve repositories via a plain HTTP
   server like Apache. The protocol has not seen any updates recently and is
   neither compatible with alternative hash functions nor with alternative ref
   backends. It will thus be removed.

 - git-update-server-info(1) generates data required when serving data via the
   dumb HTTP protocol. Given the removal of that protocol, it serves no purpose
   anymore and will be removed together with the protocol. This includes the
   "receive.updateServerInfo" and "repack.updateServerInfo" config keys and the
   `git repack -n` flag.

 - `$GIT_DIR/branches/` and `$GIT_DIR/remotes/` can be used to specify
   shorthands for URLs for git-fetch(1), git-pull(1) and git-push(1). This
   concept has long been replaced by remotes and will thus be removed.

 - "gitweb" and git-instaweb(1) can be used to browse Git repositories via an
   HTTP server. These scripts have been unmaintained for a significant amount of
   time and will be removed.

 - git-filter-branch(1) can be used to rewrite history of a repository. It is
   very slow, hard to use and has many gotchas. It will thus be removed in favor
   of [git-filter-repo](https://github.com/newren/git-filter-repo).

 - The "dashed form", i.e. support for calling `git-<command>` instead of
   `git <command>` in scripts, has been deprecated for a long time and will be
   removed.

 - The command to import patches from Quilt seems to be used rarely, if
   ever, and will be removed.

 - Support for importing repositories from GNU Arch will be removed because
   it would not appear to have any users.

 - Support for interacting with CVS repositories (via `cvsimport`,
   `cvsexportcommit` and `cvsserver`) is of dubious use by now, judging by
   the number of times these commands have been mentioned recently. The
   code has been essentially unmaintained, too, and will be removed.

 - Support for grafting commits has long been superseded by git-replace(1).
   Grafts are inferior to replacement refs as the mechanism can lead to
   hard-to-diagnose problems when transferring objects between repositories.
   They have been outdated since e650d0643b (docs: mark info/grafts as outdated,
   2014-03-05) and will be removed.

 - git-pickaxe, an alias for git-blame(1), has been retired in acca687fa9
   (git-pickaxe: retire pickaxe, 2006-11-08) and will be removed.

## Superseded features that will not be deprecated

Some features have gained newer replacements that aim to improve the design in
certain ways. The fact that there is a replacement does not automatically mean
that the old way of doing things will eventually be removed. This section tracks
those superseded features.

 - git-annotate(1) is an alias for git-blame(1) with the `-c` flag. As
   the maintenance burden of carrying both of these commands is negligible, both
   commands will stay.

 - git-restore(1) and git-switch(1) have been introduced as a replacement for
   git-checkout(1). As git-checkout(1) is quite established, and as the benefit
   of switching to git-restore(1) and git-switch(1) is contended, all three
   commands will stay.

 - git-config(1) has learned to use subcommands that replace implicit actions
   (e.g. `git config foo.bar baz`) as well as the action flags (e.g. `git config
   --unset-all`). The action flags will not be removed in the next major Git
   release as there likely exist a lot of scripts out there that use the old
   syntax.

   Cf. <ZjiL7vu5kCVwpsLd@tanuki>.
