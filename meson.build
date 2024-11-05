project('git', 'c',
  meson_version: '>=1.3.0',
  # MSVC does not support GNU99, and C99 does not define __STDC_VERSION__
  # on MSVC. So we instead fall back to C11 there.
  default_options: ['c_std=gnu99,c11'],
  version: 'v2.47.GIT',
)

fs = import('fs')

program_path = []
# Git for Windows provides all the tools we need to build Git.
if host_machine.system() == 'windows'
  program_path += [ 'C:/Program Files/Git/bin', 'C:/Program Files/Git/usr/bin' ]
endif

awk = find_program('awk', dirs: program_path)
cygpath = find_program('cygpath', dirs: program_path, required: false)
diff = find_program('diff', dirs: program_path)
shell = find_program('sh', dirs: program_path)
tar = find_program('tar', dirs: program_path)

script_environment = environment()
foreach tool : ['cat', 'cut', 'git', 'grep', 'sed', 'sort', 'tr', 'uname']
  program = find_program(tool, dirs: program_path)
  script_environment.prepend('PATH', fs.parent(program.full_path()))
endforeach

git_version = run_command(shell, 'GIT-VERSION-GEN', check: false, env: script_environment).stdout().strip()
if git_version == ''
  git_version = meson.project_version()
endif

compiler = meson.get_compiler('c')

libgit_sources = [
  'abspath.c',
  'add-interactive.c',
  'add-patch.c',
  'advice.c',
  'alias.c',
  'alloc.c',
  'apply.c',
  'archive-tar.c',
  'archive-zip.c',
  'archive.c',
  'attr.c',
  'base85.c',
  'bisect.c',
  'blame.c',
  'blob.c',
  'bloom.c',
  'branch.c',
  'bulk-checkin.c',
  'bundle-uri.c',
  'bundle.c',
  'cache-tree.c',
  'cbtree.c',
  'chdir-notify.c',
  'checkout.c',
  'chunk-format.c',
  'color.c',
  'column.c',
  'combine-diff.c',
  'commit-graph.c',
  'commit-reach.c',
  'commit.c',
  'common-exit.c',
  'common-init.c',
  'compat/nonblock.c',
  'compat/obstack.c',
  'compat/terminal.c',
  'compat/zlib-uncompress2.c',
  'config.c',
  'connect.c',
  'connected.c',
  'convert.c',
  'copy.c',
  'credential.c',
  'csum-file.c',
  'ctype.c',
  'date.c',
  'decorate.c',
  'delta-islands.c',
  'diagnose.c',
  'diff-delta.c',
  'diff-merges.c',
  'diff-lib.c',
  'diff-no-index.c',
  'diff.c',
  'diffcore-break.c',
  'diffcore-delta.c',
  'diffcore-order.c',
  'diffcore-pickaxe.c',
  'diffcore-rename.c',
  'diffcore-rotate.c',
  'dir-iterator.c',
  'dir.c',
  'editor.c',
  'entry.c',
  'environment.c',
  'ewah/bitmap.c',
  'ewah/ewah_bitmap.c',
  'ewah/ewah_io.c',
  'ewah/ewah_rlw.c',
  'exec-cmd.c',
  'fetch-negotiator.c',
  'fetch-pack.c',
  'fmt-merge-msg.c',
  'fsck.c',
  'fsmonitor.c',
  'fsmonitor-ipc.c',
  'fsmonitor-settings.c',
  'gettext.c',
  'git-zlib.c',
  'gpg-interface.c',
  'graph.c',
  'grep.c',
  'hash-lookup.c',
  'hashmap.c',
  'help.c',
  'hex.c',
  'hex-ll.c',
  'hook.c',
  'ident.c',
  'json-writer.c',
  'kwset.c',
  'levenshtein.c',
  'line-log.c',
  'line-range.c',
  'linear-assignment.c',
  'list-objects-filter-options.c',
  'list-objects-filter.c',
  'list-objects.c',
  'lockfile.c',
  'log-tree.c',
  'loose.c',
  'ls-refs.c',
  'mailinfo.c',
  'mailmap.c',
  'match-trees.c',
  'mem-pool.c',
  'merge-blobs.c',
  'merge-ll.c',
  'merge-ort.c',
  'merge-ort-wrappers.c',
  'merge-recursive.c',
  'merge.c',
  'midx.c',
  'midx-write.c',
  'name-hash.c',
  'negotiator/default.c',
  'negotiator/noop.c',
  'negotiator/skipping.c',
  'notes-cache.c',
  'notes-merge.c',
  'notes-utils.c',
  'notes.c',
  'object-file-convert.c',
  'object-file.c',
  'object-name.c',
  'object.c',
  'oid-array.c',
  'oidmap.c',
  'oidset.c',
  'oidtree.c',
  'pack-bitmap-write.c',
  'pack-bitmap.c',
  'pack-check.c',
  'pack-mtimes.c',
  'pack-objects.c',
  'pack-revindex.c',
  'pack-write.c',
  'packfile.c',
  'pager.c',
  'parallel-checkout.c',
  'parse.c',
  'parse-options-cb.c',
  'parse-options.c',
  'patch-delta.c',
  'patch-ids.c',
  'path-walk.c',
  'path.c',
  'pathspec.c',
  'pkt-line.c',
  'preload-index.c',
  'pretty.c',
  'prio-queue.c',
  'progress.c',
  'promisor-remote.c',
  'prompt.c',
  'protocol.c',
  'protocol-caps.c',
  'prune-packed.c',
  'pseudo-merge.c',
  'quote.c',
  'range-diff.c',
  'reachable.c',
  'read-cache.c',
  'rebase-interactive.c',
  'rebase.c',
  'ref-filter.c',
  'reflog-walk.c',
  'reflog.c',
  'refs.c',
  'refs/debug.c',
  'refs/files-backend.c',
  'refs/reftable-backend.c',
  'refs/iterator.c',
  'refs/packed-backend.c',
  'refs/ref-cache.c',
  'refspec.c',
  'reftable/basics.c',
  'reftable/error.c',
  'reftable/block.c',
  'reftable/blocksource.c',
  'reftable/iter.c',
  'reftable/merged.c',
  'reftable/pq.c',
  'reftable/reader.c',
  'reftable/record.c',
  'reftable/stack.c',
  'reftable/system.c',
  'reftable/tree.c',
  'reftable/writer.c',
  'remote.c',
  'replace-object.c',
  'repo-settings.c',
  'repository.c',
  'rerere.c',
  'reset.c',
  'resolve-undo.c',
  'revision.c',
  'run-command.c',
  'send-pack.c',
  'sequencer.c',
  'serve.c',
  'server-info.c',
  'setup.c',
  'shallow.c',
  'sideband.c',
  'sigchain.c',
  'sparse-index.c',
  'split-index.c',
  'stable-qsort.c',
  'statinfo.c',
  'strbuf.c',
  'streaming.c',
  'string-list.c',
  'strmap.c',
  'strvec.c',
  'sub-process.c',
  'submodule-config.c',
  'submodule.c',
  'symlinks.c',
  'tag.c',
  'tempfile.c',
  'thread-utils.c',
  'tmp-objdir.c',
  'trace.c',
  'trace2.c',
  'trace2/tr2_cfg.c',
  'trace2/tr2_cmd_name.c',
  'trace2/tr2_ctr.c',
  'trace2/tr2_dst.c',
  'trace2/tr2_sid.c',
  'trace2/tr2_sysenv.c',
  'trace2/tr2_tbuf.c',
  'trace2/tr2_tgt_event.c',
  'trace2/tr2_tgt_normal.c',
  'trace2/tr2_tgt_perf.c',
  'trace2/tr2_tls.c',
  'trace2/tr2_tmr.c',
  'trailer.c',
  'transport-helper.c',
  'transport.c',
  'tree-diff.c',
  'tree-walk.c',
  'tree.c',
  'unpack-trees.c',
  'upload-pack.c',
  'url.c',
  'urlmatch.c',
  'usage.c',
  'userdiff.c',
  'utf8.c',
  'varint.c',
  'versioncmp.c',
  'walker.c',
  'wildmatch.c',
  'worktree.c',
  'wrapper.c',
  'write-or-die.c',
  'ws.c',
  'wt-status.c',
  'xdiff-interface.c',
  'xdiff/xdiffi.c',
  'xdiff/xemit.c',
  'xdiff/xhistogram.c',
  'xdiff/xmerge.c',
  'xdiff/xpatience.c',
  'xdiff/xprepare.c',
  'xdiff/xutils.c',
]

builtin_sources = [
  'builtin/add.c',
  'builtin/am.c',
  'builtin/annotate.c',
  'builtin/apply.c',
  'builtin/archive.c',
  'builtin/bisect.c',
  'builtin/blame.c',
  'builtin/branch.c',
  'builtin/bugreport.c',
  'builtin/bundle.c',
  'builtin/cat-file.c',
  'builtin/check-attr.c',
  'builtin/check-ignore.c',
  'builtin/check-mailmap.c',
  'builtin/check-ref-format.c',
  'builtin/checkout--worker.c',
  'builtin/checkout-index.c',
  'builtin/checkout.c',
  'builtin/clean.c',
  'builtin/clone.c',
  'builtin/column.c',
  'builtin/commit-graph.c',
  'builtin/commit-tree.c',
  'builtin/commit.c',
  'builtin/config.c',
  'builtin/count-objects.c',
  'builtin/credential-cache--daemon.c',
  'builtin/credential-cache.c',
  'builtin/credential-store.c',
  'builtin/credential.c',
  'builtin/describe.c',
  'builtin/diagnose.c',
  'builtin/diff-files.c',
  'builtin/diff-index.c',
  'builtin/diff-tree.c',
  'builtin/diff.c',
  'builtin/difftool.c',
  'builtin/fast-export.c',
  'builtin/fast-import.c',
  'builtin/fetch-pack.c',
  'builtin/fetch.c',
  'builtin/fmt-merge-msg.c',
  'builtin/for-each-ref.c',
  'builtin/for-each-repo.c',
  'builtin/fsck.c',
  'builtin/fsmonitor--daemon.c',
  'builtin/gc.c',
  'builtin/get-tar-commit-id.c',
  'builtin/grep.c',
  'builtin/hash-object.c',
  'builtin/help.c',
  'builtin/hook.c',
  'builtin/index-pack.c',
  'builtin/init-db.c',
  'builtin/interpret-trailers.c',
  'builtin/log.c',
  'builtin/ls-files.c',
  'builtin/ls-remote.c',
  'builtin/ls-tree.c',
  'builtin/mailinfo.c',
  'builtin/mailsplit.c',
  'builtin/merge-base.c',
  'builtin/merge-file.c',
  'builtin/merge-index.c',
  'builtin/merge-ours.c',
  'builtin/merge-recursive.c',
  'builtin/merge-tree.c',
  'builtin/merge.c',
  'builtin/mktag.c',
  'builtin/mktree.c',
  'builtin/multi-pack-index.c',
  'builtin/mv.c',
  'builtin/name-rev.c',
  'builtin/notes.c',
  'builtin/pack-objects.c',
  'builtin/pack-redundant.c',
  'builtin/pack-refs.c',
  'builtin/patch-id.c',
  'builtin/prune-packed.c',
  'builtin/prune.c',
  'builtin/pull.c',
  'builtin/push.c',
  'builtin/range-diff.c',
  'builtin/read-tree.c',
  'builtin/rebase.c',
  'builtin/receive-pack.c',
  'builtin/reflog.c',
  'builtin/refs.c',
  'builtin/remote-ext.c',
  'builtin/remote-fd.c',
  'builtin/remote.c',
  'builtin/repack.c',
  'builtin/replace.c',
  'builtin/replay.c',
  'builtin/rerere.c',
  'builtin/reset.c',
  'builtin/rev-list.c',
  'builtin/rev-parse.c',
  'builtin/revert.c',
  'builtin/rm.c',
  'builtin/send-pack.c',
  'builtin/shortlog.c',
  'builtin/show-branch.c',
  'builtin/show-index.c',
  'builtin/show-ref.c',
  'builtin/sparse-checkout.c',
  'builtin/stash.c',
  'builtin/stripspace.c',
  'builtin/submodule--helper.c',
  'builtin/symbolic-ref.c',
  'builtin/tag.c',
  'builtin/unpack-file.c',
  'builtin/unpack-objects.c',
  'builtin/update-index.c',
  'builtin/update-ref.c',
  'builtin/update-server-info.c',
  'builtin/upload-archive.c',
  'builtin/upload-pack.c',
  'builtin/var.c',
  'builtin/verify-commit.c',
  'builtin/verify-pack.c',
  'builtin/verify-tag.c',
  'builtin/worktree.c',
  'builtin/write-tree.c',
]

libgit_sources += custom_target(
  input: 'command-list.txt',
  output: 'command-list.h',
  command: [shell, meson.current_source_dir() + '/generate-cmdlist.sh', meson.current_source_dir(), '@OUTPUT@'],
  env: script_environment,
)

libgit_sources += custom_target(
  output: 'config-list.h',
  command: [
    shell,
    meson.current_source_dir() + '/generate-configlist.sh',
    meson.current_source_dir(),
    '@OUTPUT@',
  ],
  env: script_environment,
)

libgit_sources += custom_target(
  input: 'Documentation/githooks.txt',
  output: 'hook-list.h',
  command: [
    shell,
    meson.current_source_dir() + '/generate-hooklist.sh',
    meson.current_source_dir(),
    '@OUTPUT@',
  ],
  env: script_environment,
)

# This contains the variables for GIT-BUILD-OPTIONS, which we use to propagate
# build options to our tests.
build_options_config = configuration_data()
build_options_config.set('GIT_INTEROP_MAKE_OPTS', '')
build_options_config.set('GIT_PERF_LARGE_REPO', '')
build_options_config.set('GIT_PERF_MAKE_COMMAND', '')
build_options_config.set('GIT_PERF_MAKE_OPTS', '')
build_options_config.set('GIT_PERF_REPEAT_COUNT', '')
build_options_config.set('GIT_PERF_REPO', '')
build_options_config.set('GIT_TEST_CMP_USE_COPIED_CONTEXT', '')
build_options_config.set('GIT_TEST_INDEX_VERSION', '')
build_options_config.set('GIT_TEST_OPTS', '')
build_options_config.set('GIT_TEST_PERL_FATAL_WARNINGS', '')
build_options_config.set('GIT_TEST_UTF8_LOCALE', '')
build_options_config.set('SANITIZE_ADDRESS', '')
build_options_config.set('SANITIZE_LEAK', '')
build_options_config.set('BROKEN_PATH_FIX', '')
build_options_config.set_quoted('LOCALEDIR', fs.as_posix(get_option('prefix') / get_option('localedir')))
build_options_config.set('GITWEBDIR', fs.as_posix(get_option('prefix') / get_option('datadir') / 'gitweb'))

test_output_directory = get_option('test_output_directory')
if test_output_directory == ''
  test_output_directory = meson.project_build_root() / 'test-output'
endif

# These variables are used for building libgit.a.
libgit_c_args = [
  '-DBINDIR="' + get_option('bindir') + '"',
  '-DDEFAULT_EDITOR="' + get_option('default_editor') + '"',
  '-DDEFAULT_GIT_TEMPLATE_DIR="' + get_option('datadir') / 'git-core/templates' + '"',
  '-DDEFAULT_HELP_FORMAT="' + get_option('default_help_format') + '"',
  '-DDEFAULT_PAGER="' + get_option('default_pager') + '"',
  '-DETC_GITATTRIBUTES="' + get_option('gitattributes') + '"',
  '-DETC_GITCONFIG="' + get_option('gitconfig') + '"',
  '-DFALLBACK_RUNTIME_PREFIX="' + get_option('prefix') + '"',
  '-DGIT_EXEC_PATH="' + get_option('prefix') / get_option('libexecdir') / 'git-core"',
  '-DGIT_HOST_CPU="' + host_machine.cpu_family() + '"',
  '-DGIT_HTML_PATH="' + get_option('datadir') / 'doc/git-doc"',
  '-DGIT_INFO_PATH="' + get_option('infodir') + '"',
  '-DGIT_LOCALE_PATH="' + get_option('localedir') + '"',
  '-DGIT_MAN_PATH="' + get_option('mandir') + '"',
  '-DPAGER_ENV="' + get_option('pager_environment') + '"',
  '-DSHELL_PATH="' + fs.as_posix(shell.full_path()) + '"',
]
libgit_include_directories = [ '.' ]
libgit_dependencies = [ ]

executable_suffix = ''
if host_machine.system() == 'cygwin' or host_machine.system() == 'windows'
  executable_suffix = '.exe'
  libgit_c_args += '-DSTRIP_EXTENSION="' + executable_suffix + '"'
endif
build_options_config.set_quoted('X', executable_suffix)

python = import('python').find_installation('python3', required: get_option('python'))
if python.found()
  build_options_config.set('NO_PYTHON', '')
else
  libgit_c_args += '-DNO_PYTHON'
  build_options_config.set('NO_PYTHON', '1')
endif

# Perl is used for two different things: our test harness and to provide some
# features. It is optional if you want to neither execute tests nor use any of
# these optional features.
perl_required = get_option('perl')
if get_option('tests')
  perl_required = true
endif

# Note that we only set NO_PERL if the Perl features were disabled by the user.
# It may not be set when we have found Perl, but only use it to run tests.
perl = find_program('perl', version: '>=5.8.1', dirs: program_path, required: perl_required)
perl_features_enabled = perl.found() and get_option('perl').allowed()
if perl_features_enabled
  build_options_config.set('NO_PERL', '')

  if get_option('runtime_prefix')
    build_options_config.set('PERL_LOCALEDIR', '')
  else
    build_options_config.set_quoted('PERL_LOCALEDIR', fs.as_posix(get_option('prefix') / get_option('localedir')))
  endif

  if get_option('perl_cpan_fallback')
    build_options_config.set('NO_PERL_CPAN_FALLBACKS', '')
  else
    build_options_config.set_quoted('NO_PERL_CPAN_FALLBACKS', 'YesPlease')
  endif
else
  libgit_c_args += '-DNO_PERL'
  build_options_config.set('NO_PERL', '1')
  build_options_config.set('PERL_LOCALEDIR', '')
  build_options_config.set('NO_PERL_CPAN_FALLBACKS', '')
endif

zlib = dependency('zlib', default_options: ['default_library=static', 'tests=disabled'])
if zlib.version().version_compare('<1.2.0')
  libgit_c_args += '-DNO_DEFLATE_BOUND'
endif
libgit_dependencies += zlib

threads = dependency('threads', required: false)
if threads.found()
  libgit_dependencies += threads
  build_options_config.set('NO_PTHREADS', '')
else
  libgit_c_args += '-DNO_PTHREADS'
  build_options_config.set('NO_PTHREADS', '1')
endif

if get_option('gettext').allowed() and host_machine.system() == 'darwin' and get_option('macos_use_homebrew_gettext')
  if host_machine.cpu_family() == 'x86_64'
    libintl_prefix = '/usr/local'
  elif host_machine.cpu_family() == 'aarch64'
    libintl_prefix = '/opt/homebrew'
  else
    error('Homebrew workaround not supported on current architecture')
  endif

  intl = compiler.find_library('intl', dirs: libintl_prefix / 'lib', required: get_option('gettext'))
  if intl.found()
    intl = declare_dependency(
      dependencies: intl,
      include_directories: libintl_prefix / 'include',
    )
  endif
else
  intl = dependency('intl', required: get_option('gettext'))
endif
if intl.found()
  libgit_dependencies += intl
  build_options_config.set('NO_GETTEXT', '')
  build_options_config.set('USE_GETTEXT_SCHEME', '')
else
  libgit_c_args += '-DNO_GETTEXT'
  build_options_config.set('NO_GETTEXT', '1')
  build_options_config.set('USE_GETTEXT_SCHEME', 'fallthrough')
endif

iconv = dependency('iconv', required: get_option('iconv'))
if iconv.found()
  libgit_dependencies += iconv
  build_options_config.set('NO_ICONV', '')

  have_old_iconv = false
  if not compiler.compiles('''
    #include <iconv.h>

    extern size_t iconv(iconv_t cd,
                        char **inbuf, size_t *inbytesleft,
                        char **outbuf, size_t *outbytesleft);
  ''', name: 'old iconv interface', dependencies: [iconv])
    libgit_c_args += '-DOLD_ICONV'
    have_old_iconv = true
  endif

  iconv_omits_bom_source = '''#
    #include <iconv.h>

    int main(int argc, const char **argv)
    {
  '''
  if have_old_iconv
    iconv_omits_bom_source += '''
      typedef const char *iconv_ibp;
    '''
  else
    iconv_omits_bom_source += '''
      typedef char *iconv_ibp;
    '''
  endif
  iconv_omits_bom_source += '''
      int v;
      iconv_t conv;
      char in[] = "a"; iconv_ibp pin = in;
      char out[20] = ""; char *pout = out;
      size_t isz = sizeof in;
      size_t osz = sizeof out;

      conv = iconv_open("UTF-16", "UTF-8");
      iconv(conv, &pin, &isz, &pout, &osz);
      iconv_close(conv);
      v = (unsigned char)(out[0]) + (unsigned char)(out[1]);
      return v != 0xfe + 0xff;
    }
  '''

  if compiler.run(iconv_omits_bom_source,
    dependencies: iconv,
    name: 'iconv omits BOM',
  ).returncode() != 0
    libgit_c_args += '-DICONV_OMITS_BOM'
  endif
else
  libgit_c_args += '-DNO_ICONV'
  build_options_config.set('NO_ICONV', '1')
endif

pcre2 = dependency('libpcre2-8', required: get_option('pcre2'), default_options: ['default_library=static', 'test=false'])
if pcre2.found()
  libgit_dependencies += pcre2
  libgit_c_args += '-DUSE_LIBPCRE2'
  build_options_config.set('USE_LIBPCRE2', '1')
else
  build_options_config.set('USE_LIBPCRE2', '')
endif

curl = dependency('libcurl', version: '>=7.21.3', required: get_option('curl'), default_options: ['default_library=static', 'tests=disabled'])
use_curl_for_imap_send = false
if curl.found()
  if curl.version().version_compare('>=7.34.0')
    libgit_c_args += '-DUSE_CURL_FOR_IMAP_SEND'
    use_curl_for_imap_send = true
  endif

  libgit_dependencies += curl
  libgit_c_args += '-DCURL_DISABLE_TYPECHECK'
  build_options_config.set('NO_CURL', '')
else
  libgit_c_args += '-DNO_CURL'
  build_options_config.set('NO_CURL', '1')
endif

expat = dependency('expat', required: get_option('expat'), default_options: ['default_library=static', 'build_tests=false'])
if expat.found()
  libgit_dependencies += expat

  if expat.version().version_compare('<=1.2')
    libgit_c_args += '-DEXPAT_NEEDS_XMLPARSE_H'
  endif
  build_options_config.set('NO_EXPAT', '')
else
  libgit_c_args += '-DNO_EXPAT'
  build_options_config.set('NO_EXPAT', '1')
endif

if not compiler.has_header('sys/select.h')
  libgit_c_args += '-DNO_SYS_SELECT_H'
endif

has_poll_h = compiler.has_header('poll.h')
if not has_poll_h
  libgit_c_args += '-DNO_POLL_H'
endif

has_sys_poll_h = compiler.has_header('sys/poll.h')
if not has_sys_poll_h
  libgit_c_args += '-DNO_SYS_POLL_H'
endif

if not has_poll_h and not has_sys_poll_h
  libgit_c_args += '-DNO_POLL'
  libgit_sources += 'compat/poll/poll.c'
  libgit_include_directories += 'compat/poll'
endif

if not compiler.has_header('inttypes.h')
  libgit_c_args += '-DNO_INTTYPES_H'
endif

if compiler.has_header('libcharset.h')
  libcharset = compiler.find_library('charset')

  if compiler.has_function('locale_charset',
    prefix: '#include <libcharset.h>',
    dependencies: iconv,
  )
    libgit_c_args += '-DHAVE_LIBCHARSET_H'
  elif compiler.has_function('locale_charset',
    prefix: '#include <libcharset.h>',
    dependencies: libcharset,
  )
    libgit_c_args += '-DHAVE_LIBCHARSET_H'
  endif
endif

if compiler.has_header('alloca.h')
  libgit_c_args += '-DHAVE_ALLOCA_H'
endif

if compiler.has_header('sys/sysinfo.h')
  libgit_c_args += '-DHAVE_SYSINFO'
endif

# Windows has libgen.h and a basename implementation, but we still need our own
# implementation to threat things like drive prefixes specially.
if host_machine.system() == 'windows' or not compiler.has_header('libgen.h')
  libgit_c_args += '-DNO_LIBGEN_H'
  libgit_sources += 'compat/basename.c'
endif

if compiler.has_header('paths.h')
  libgit_c_args += '-DHAVE_PATHS_H'
endif

if compiler.has_header('strings.h')
  libgit_c_args += '-DHAVE_STRINGS_H'
endif

networking_dependencies = [ ]
if host_machine.system() == 'windows'
  winsock = compiler.find_library('ws2_32', required: false)
  if winsock.found()
    networking_dependencies += winsock
  endif
else
  libresolv = compiler.find_library('resolv', required: false)
  if libresolv.found()
    networking_dependencies += libresolv
  endif
endif
libgit_dependencies += networking_dependencies

foreach symbol : ['inet_ntop', 'inet_pton', 'strerror']
  if not compiler.has_function(symbol, dependencies: networking_dependencies)
    libgit_c_args += '-DNO_' + symbol.to_upper()
  endif
endforeach

has_ipv6 = compiler.has_function('getaddrinfo', dependencies: networking_dependencies)
if not has_ipv6
  libgit_c_args += '-DNO_IPV6'
endif

if not compiler.compiles('''
  #ifdef _WIN32
  # include <winsock2.h>
  #else
  # include <sys/types.h>
  # include <sys/socket.h>
  #endif

  void func(void)
  {
    struct sockaddr_storage x;
  }
''', name: 'struct sockaddr_storage')
  if has_ipv6
    libgit_c_args += '-Dsockaddr_storage=sockaddr_in6'
  else
    libgit_c_args += '-Dsockaddr_storage=sockaddr_in'
  endif
endif

if compiler.has_function('socket', dependencies: networking_dependencies)
  libgit_sources += [
    'unix-socket.c',
    'unix-stream-server.c',
  ]
  build_options_config.set('NO_UNIX_SOCKETS', '')
else
  libgit_c_args += '-DNO_UNIX_SOCKETS'
  build_options_config.set('NO_UNIX_SOCKETS', '1')
endif

if not compiler.has_function('pread')
  libgit_c_args += '-DNO_PREAD'
  libgit_sources += 'compat/pread.c'
endif

if host_machine.system() == 'darwin'
  libgit_sources += 'compat/precompose_utf8.c'
  libgit_c_args += '-DPRECOMPOSE_UNICODE'
  libgit_c_args += '-DPROTECT_HFS_DEFAULT'
endif

# Configure general compatibility wrappers.
if host_machine.system() == 'cygwin'
  libgit_sources += [
    'compat/win32/path-utils.c',
  ]
elif host_machine.system() == 'windows'
  libgit_sources += [
    'compat/mingw.c',
    'compat/winansi.c',
    'compat/win32/flush.c',
    'compat/win32/path-utils.c',
    'compat/win32/pthread.c',
    'compat/win32/syslog.c',
    'compat/win32/dirent.c',
    'compat/win32mmap.c',
    'compat/nedmalloc/nedmalloc.c',
  ]

  libgit_c_args += [
    '-DDETECT_MSYS_TTY',
    '-DENSURE_MSYSTEM_IS_SET',
    '-DNATIVE_CRLF',
    '-DNOGDI',
    '-DNO_POSIX_GOODIES',
    '-DWIN32',
    '-D_CONSOLE',
    '-D_CONSOLE_DETECT_MSYS_TTY',
    '-D__USE_MINGW_ANSI_STDIO=0',
  ]

  libgit_dependencies += compiler.find_library('ntdll')
  libgit_include_directories += 'compat/win32'
  if compiler.get_id() == 'msvc'
    libgit_include_directories += 'compat/vcbuild/include'
  endif
endif

if host_machine.system() == 'linux'
  libgit_sources += 'compat/linux/procinfo.c'
elif host_machine.system() == 'windows'
  libgit_sources += 'compat/win32/trace2_win32_process_info.c'
else
  libgit_sources += 'compat/stub/procinfo.c'
endif

if host_machine.system() == 'cygwin' or host_machine.system() == 'windows'
  libgit_c_args += [
    '-DUNRELIABLE_FSTAT',
    '-DMMAP_PREVENTS_DELETE',
    '-DOBJECT_CREATION_MODE=1',
  ]
endif

# Configure the simple-ipc subsystem required fro the fsmonitor.
if host_machine.system() == 'windows'
  libgit_sources += [
    'compat/simple-ipc/ipc-shared.c',
    'compat/simple-ipc/ipc-win32.c',
  ]
  libgit_c_args += '-DSUPPORTS_SIMPLE_IPC'
else
  libgit_sources += [
    'compat/simple-ipc/ipc-shared.c',
    'compat/simple-ipc/ipc-unix-socket.c',
  ]
  libgit_c_args += '-DSUPPORTS_SIMPLE_IPC'
endif

fsmonitor_backend = ''
if host_machine.system() == 'windows'
  fsmonitor_backend = 'win32'
elif host_machine.system() == 'darwin'
  fsmonitor_backend = 'darwin'
  libgit_dependencies += dependency('CoreServices')
endif
if fsmonitor_backend != ''
  libgit_c_args += '-DHAVE_FSMONITOR_DAEMON_BACKEND'
  libgit_c_args += '-DHAVE_FSMONITOR_OS_SETTINGS'

  libgit_sources += [
    'compat/fsmonitor/fsm-health-' + fsmonitor_backend + '.c',
    'compat/fsmonitor/fsm-ipc-' + fsmonitor_backend + '.c',
    'compat/fsmonitor/fsm-listen-' + fsmonitor_backend + '.c',
    'compat/fsmonitor/fsm-path-utils-' + fsmonitor_backend + '.c',
    'compat/fsmonitor/fsm-settings-' + fsmonitor_backend + '.c',
  ]
endif
build_options_config.set_quoted('FSMONITOR_DAEMON_BACKEND', fsmonitor_backend)
build_options_config.set_quoted('FSMONITOR_OS_SETTINGS', fsmonitor_backend)

if compiler.has_header('regex.h') and compiler.get_define('REG_STARTEND', prefix: '#include <regex.h>') != ''
  build_options_config.set('NO_REGEX', '')

  if compiler.get_define('REG_ENHANCED', prefix: '#include <regex.h>') != ''
    libgit_c_args += '-DUSE_ENHANCED_BASIC_REGULAR_EXPRESSIONS'
    libgit_sources += 'compat/regcomp_enhanced.c'
  endif
else
  libgit_c_args += [
    '-DNO_REGEX',
    '-DGAWK',
    '-DNO_MBSUPPORT',
  ]
  build_options_config.set('NO_REGEX', '1')
  libgit_sources += 'compat/regex/regex.c'
  libgit_include_directories += 'compat/regex'
endif

# setitimer and friends are provided by compat/mingw.c.
if host_machine.system() != 'windows'
  if not compiler.compiles('''
    #include <sys/time.h>
    void func(void)
    {
      struct itimerval value;
    }
  ''', name: 'struct itimerval')
    libgit_c_args += '-DNO_STRUCT_ITIMERVAL'
    libgit_c_args += '-DNO_SETITIMER'
  elif not compiler.has_function('setitimer')
    libgit_c_args += '-DNO_SETITIMER'
  endif
endif

if compiler.has_member('struct stat', 'st_mtimespec.tv_nsec', prefix: '#include <sys/stat.h>')
  libgit_c_args += '-DUSE_ST_TIMESPEC'
elif not compiler.has_member('struct stat', 'st_mtim.tv_nsec', prefix: '#include <sys/stat.h>')
  libgit_c_args += '-DNO_NSEC'
endif

if not compiler.has_member('struct stat', 'st_blocks', prefix: '#include <sys/stat.h>')
  libgit_c_args += '-DNO_ST_BLOCKS_IN_STRUCT_STAT'
endif

if not compiler.has_member('struct dirent', 'd_type', prefix: '#include <dirent.h>')
  libgit_c_args += '-DNO_D_TYPE_IN_DIRENT'
endif

if not compiler.has_member('struct passwd', 'pw_gecos', prefix: '#include <pwd.h>')
  libgit_c_args += '-DNO_GECOS_IN_PWENT'
endif

if compiler.has_function('sync_file_range')
  libgit_c_args += '-DHAVE_SYNC_FILE_RANGE'
endif

if not compiler.has_function('strcasestr')
  libgit_c_args += '-DNO_STRCASESTR'
  libgit_sources += 'compat/strcasestr.c'
endif

if not compiler.has_function('memmem')
  libgit_c_args += '-DNO_MEMMEM'
  libgit_sources += 'compat/memmem.c'
endif

if not compiler.has_function('strlcpy')
  libgit_c_args += '-DNO_STRLCPY'
  libgit_sources += 'compat/strlcpy.c'
endif

if not compiler.has_function('strdup')
  libgit_c_args += '-DOVERRIDE_STRDUP'
  libgit_sources += 'compat/strdup.c'
endif

if not compiler.has_function('strtoumax')
  libgit_c_args += '-DNO_STRTOUMAX'
  libgit_sources += [
    'compat/strtoumax.c',
    'compat/strtoimax.c',
  ]
endif

if not compiler.has_function('strtoull')
  libgit_c_args += '-DNO_STRTOULL'
endif

if not compiler.has_function('setenv')
  libgit_c_args += '-DNO_SETENV'
  libgit_sources += 'compat/setenv.c'
endif

if not compiler.has_function('qsort')
  libgit_c_args += '-DINTERNAL_QSORT'
endif
libgit_sources += 'compat/qsort_s.c'

# unsetenv is provided by compat/mingw.c.
if host_machine.system() != 'windows' and not compiler.has_function('unsetenv')
  libgit_c_args += '-DNO_UNSETENV'
  libgit_sources += 'compat/unsetenv.c'
endif

if not compiler.has_function('mkdtemp')
  libgit_c_args += '-DNO_MKDTEMP'
  libgit_sources += 'compat/mkdtemp.c'
endif

if not compiler.has_function('initgroups')
  libgit_c_args += '-DNO_INITGROUPS'
endif

if compiler.has_function('getdelim')
  libgit_c_args += '-DHAVE_GETDELIM'
endif

if host_machine.system() == 'windows'
  libgit_c_args += '-DUSE_WIN32_MMAP'
elif not compiler.has_function('mmap')
  libgit_c_args += '-DNO_MMAP'
  libgit_sources += 'compat/mmap.c'
endif

if compiler.has_function('clock_gettime')
  libgit_c_args += '-DHAVE_CLOCK_GETTIME'
endif

if compiler.compiles('''
  #include <time.h>

  void func(void)
  {
    clockid_t id = CLOCK_MONOTONIC;
  }
''', name: 'monotonic clock')
  libgit_c_args += '-DHAVE_CLOCK_MONOTONIC'
endif

if compiler.compiles('''
  #include <inttypes.h>

  void func(void)
  {
    uintmax_t x = 0;
  }
''', name: 'uintmax_t')
  libgit_c_args += '-DNO_UINTMAX_T'
endif

has_bsd_sysctl = false
if compiler.has_header('sys/sysctl.h')
  if compiler.compiles('''
    #include <stddef.h>
    #include <sys/sysctl.h>

    void func(void)
    {
      int val, mib[2] = { 0 };
      size_t len = sizeof(val);
      sysctl(mib, 2, &val, &len, NULL, 0);
    }
  ''', name: 'BSD sysctl')
    libgit_c_args += '-DHAVE_BSD_SYSCTL'
    has_bsd_sysctl = true
  endif
endif

if compiler.run('''
  #include <stdio.h>

  int main(int argc, const char **argv)
  {
    FILE *f = fopen(".", "r");
    return f ? 0 : 1;
  }
''', name: 'fread reads directories').returncode() == 0
  libgit_c_args += '-DFREAD_READS_DIRECTORIES'
  libgit_sources += 'compat/fopen.c'
endif

if not meson.is_cross_build() and fs.exists('/dev/tty')
  libgit_c_args += '-DHAVE_DEV_TTY'
endif

if get_option('openssl').enabled() and get_option('CommonCrypto').enabled()
  error('Can only use one SSL backend')
endif

security_framework = dependency('Security', required: get_option('CommonCrypto').disable_auto_if(host_machine.system() != 'darwin'))
core_foundation_framework = dependency('CoreFoundation', required: security_framework.found())
if security_framework.found()
  libgit_dependencies += security_framework
  libgit_dependencies += core_foundation_framework
  libgit_c_args += '-DAPPLE_COMMON_CRYPTO'
endif

# OpenSSL is required when requested via the 'openssl' feature or via one of
# the SHA1/SHA256 backends.
openssl_required = get_option('openssl').disable_auto_if(security_framework.found())
if get_option('sha1_backend') == 'openssl' or get_option('sha256_backend') == 'openssl'
  openssl_required = true
endif

openssl = dependency('openssl', required: openssl_required, default_options: ['default_library=static'])
if openssl.found()
  libgit_dependencies += openssl
endif

# We may not want to use OpenSSL for anything but our SHA1/SHA256 backends, so
# we cannot just set NO_OPENSSL based on whether or not the library was found.
if not openssl.found() or get_option('openssl').disabled()
  libgit_c_args += '-DNO_OPENSSL'
endif

sha1_backend = get_option('sha1_backend')
if sha1_backend == 'sha1dc'
  libgit_c_args += '-DSHA1_DC'
  libgit_c_args += '-DSHA1DC_NO_STANDARD_INCLUDES=1'
  libgit_c_args += '-DSHA1DC_INIT_SAFE_HASH_DEFAULT=0'
  libgit_c_args += '-DSHA1DC_CUSTOM_INCLUDE_SHA1_C="git-compat-util.h"'
  libgit_c_args += '-DSHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C="git-compat-util.h"'

  libgit_sources += [
    'sha1dc_git.c',
    'sha1dc/sha1.c',
    'sha1dc/ubc_check.c',
  ]
elif sha1_backend == 'common-crypto'
  libgit_c_args += '-DCOMMON_DIGEST_FOR_OPENSSL'
  libgit_c_args += '-DSHA1_APPLE'
  libgit_c_args += '-DSHA1_MAX_BLOCK_SIZE=1024L*1024L*1024L'
elif sha1_backend == 'openssl'
  if not openssl.found()
    openssl = dependency('openssl', required: true)
  endif

  libgit_c_args += '-DSHA1_OPENSSL'
  # Apple CommonCrypto requires chunking
elif sha1_backend == 'block'
  libgit_c_args += '-DSHA1_BLK'
  libgit_sources += 'block-sha1/sha1.c'
else
  error('Unhandled SHA1 backend ' + sha1_backend)
endif

sha256_backend = get_option('sha256_backend')
if sha256_backend == 'openssl'
  libgit_c_args += '-DSHA256_OPENSSL'
elif sha256_backend == 'nettle'
  nettle = dependency('nettle')
  libgit_dependencies += nettle
  libgit_c_args += '-DSHA256_NETTLE'
elif sha256_backend == 'gcrypt'
  gcrypt = dependency('gcrypt')
  libgit_dependencies += gcrypt
  libgit_c_args += '-DSHA256_GCRYPT'
elif sha256_backend == 'block'
  libgit_c_args += '-DSHA256_BLK'
  libgit_sources += 'sha256/block/sha256.c'
else
  error('Unhandled SHA256 backend ' + sha256_backend)
endif

if compiler.has_header_symbol('stdlib.h', 'arc4random_buf')
  libgit_c_args += '-DHAVE_ARC4RANDOM'
elif compiler.has_header_symbol('bsd/stdlib.h', 'arc4random_buf')
  libgit_c_args += '-DHAVE_ARC4RANDOM_BSD'
elif compiler.has_function('getrandom', prefix: '#include <sys/random.h>')
  libgit_c_args += '-DHAVE_GETRANDOM'
elif compiler.has_function('getentropy', prefix: '#include <unistd.h>')
  libgit_c_args += '-DHAVE_GETENTROPY'
elif compiler.has_function('RtlGenRandom', prefix: '#include <windows.h>\n#include <ntsecapi.h>')
  libgit_c_args += '-DHAVE_RTLGENRANDOM'
elif openssl.found()
  libgit_c_args += '-DHAVE_OPENSSL_CSPRNG'
endif

if get_option('runtime_prefix')
  libgit_c_args += '-DRUNTIME_PREFIX'
  build_options_config.set('RUNTIME_PREFIX', '1')

  if compiler.has_header('mach-o/dyld.h')
    libgit_c_args += '-DHAVE_NS_GET_EXECUTABLE_PATH'
  endif

  if has_bsd_sysctl and compiler.compiles('''
    #include <sys/sysctl.h>

    void func(void)
    {
      KERN_PROC_PATHNAME; KERN_PROC;
    }
  ''', name: 'BSD KERN_PROC_PATHNAME')
    libgit_c_args += '-DHAVE_NS_GET_EXECUTABLE_PATH'
  endif

  if host_machine.system() == 'linux'
    libgit_c_args += '-DPROCFS_EXECUTABLE_PATH="/proc/self/exe' + '"'
  elif host_machine.system() == 'openbsd'
    libgit_c_args += '-DPROCFS_EXECUTABLE_PATH="' + '/proc/curproc/file' + '"'
  elif host_machine.system() == 'netbsd'
    libgit_c_args += '-DPROCFS_EXECUTABLE_PATH="' + '/proc/curproc/exe' + '"'
  endif

  if host_machine.system() == 'windows' and compiler.compiles('''
    #include <stdlib.h>

    void func(void)
    {
      _wpgmptr;
    }
  ''', name: 'Win32 _wpgmptr')
    libgit_c_args += '-DHAVE_WPGMPTR'
  endif
else
  build_options_config.set('RUNTIME_PREFIX', '')
endif

foreach key, value : {
  'DIFF': diff.full_path(),
  'GIT_TEST_CMP': diff.full_path() + ' -u',
  'GIT_TEST_GITPERLLIB': meson.project_build_root() / 'perl',
  'GIT_TEST_MERGE_TOOLS_DIR': meson.project_source_root() / 'mergetools',
  'GIT_TEST_POPATH': meson.project_source_root() / 'po',
  'GIT_TEST_TEMPLATE_DIR': meson.project_build_root() / 'templates',
  'GIT_TEST_TEXTDOMAINDIR': meson.project_build_root() / 'po',
  'PAGER_ENV': get_option('pager_environment'),
  'PERL_PATH': perl.found() ? perl.full_path() : '',
  'PYTHON_PATH': python.found () ? python.full_path() : '',
  'SHELL_PATH': shell.full_path(),
  'TAR': tar.full_path(),
  'TEST_OUTPUT_DIRECTORY': test_output_directory,
  'TEST_SHELL_PATH': shell.full_path(),
}
  if value != '' and cygpath.found()
    value = run_command(cygpath, value, check: true).stdout().strip()
  endif
  build_options_config.set_quoted(key, value)
endforeach

configure_file(
  input: 'GIT-BUILD-OPTIONS.in',
  output: 'GIT-BUILD-OPTIONS',
  configuration: build_options_config,
)

# Build a separate library for "version.c" so that we do not have to rebuild
# everything when the current Git commit changes. TODO: this only gets set up
# at configuration time, so we do not notice version changes unless the build
# instructions get regenerated. We should refactor the source file such that we
# can substitute tags in the file via `vcs_tag()`.
libgit_version_library = static_library('git-version',
  sources: ['version.c'],
  c_args: libgit_c_args + [
    '-DGIT_VERSION="' + git_version + '"',
    '-DGIT_USER_AGENT="' + 'git/' + git_version + '"',
    '-DGIT_BUILT_FROM_COMMIT="' + run_command('git', 'rev-parse', '-q', '--verify', 'HEAD', check: false).stdout().strip() + '"',
  ],
  dependencies: libgit_dependencies,
  include_directories: libgit_include_directories,
)

libgit_library = static_library('git',
  sources: libgit_sources,
  c_args: libgit_c_args,
  link_with: libgit_version_library,
  dependencies: libgit_dependencies,
  include_directories: libgit_include_directories,
)

libgit = declare_dependency(
  compile_args: libgit_c_args,
  link_with: libgit_library,
  dependencies: libgit_dependencies,
  include_directories: libgit_include_directories,
)

common_main_sources = ['common-main.c']
common_main_link_args = [ ]
if host_machine.system() == 'windows'
  # TODO: wire these up properly.
  common_main_sources += import('windows').compile_resources('git.rc', args: [
    '-DMAJOR=1',
    '-DMINOR=1',
    '-DMICRO=1',
    '-DPATCHLEVEL=0',
    '-DGIT_VERSION=\\\"' + git_version + '.GIT\\\"',
  ])
  if compiler.get_argument_syntax() == 'gcc'
    common_main_link_args += [
      '-municode',
      '-Wl,-nxcompat',
      '-Wl,-dynamicbase',
      '-Wl,-pic-executable,-e,mainCRTStartup',
    ]
  elif compiler.get_argument_syntax() == 'msvc'
    common_main_link_args += [
      '/ENTRY:wmainCRTStartup',
      'invalidcontinue.obj',
    ]
  else
    error('Unsupported compiler ' + compiler.get_id())
  endif
endif
common_main_library = static_library('common-main',
  sources: common_main_sources,
  c_args: libgit_c_args,
  dependencies: libgit_dependencies,
  include_directories: libgit_include_directories,
)
common_main = declare_dependency(
  link_with: common_main_library,
  link_args: common_main_link_args,
)

bin_wrappers = [ ]
test_dependencies = [ ]

git = executable('git',
  sources: builtin_sources + 'git.c',
  dependencies: [libgit, common_main],
  install: true,
  install_dir: get_option('libexecdir') / 'git-core',
)
bin_wrappers += git

test_dependencies += executable('git-daemon',
  sources: 'daemon.c',
  dependencies: [libgit, common_main],
  install: true,
  install_dir: get_option('libexecdir') / 'git-core',
)

test_dependencies += executable('git-sh-i18n--envsubst',
  sources: 'sh-i18n--envsubst.c',
  dependencies: [libgit, common_main],
  install: true,
  install_dir: get_option('libexecdir') / 'git-core',
)

bin_wrappers += executable('git-shell',
  sources: 'shell.c',
  dependencies: [libgit, common_main],
  install: true,
  install_dir: get_option('libexecdir') / 'git-core',
)

test_dependencies += executable('git-http-backend',
  sources: 'http-backend.c',
  dependencies: [libgit, common_main],
  install: true,
  install_dir: get_option('libexecdir') / 'git-core',
)

bin_wrappers += executable('scalar',
  sources: 'scalar.c',
  dependencies: [libgit, common_main],
  install: true,
  install_dir: get_option('libexecdir') / 'git-core',
)

if get_option('curl').enabled()
  curl_sources = [
    'http.c',
    'http-walker.c',
  ]

  git_remote_http = executable('git-remote-http',
    sources: curl_sources + 'remote-curl.c',
    dependencies: [libgit, common_main],
    install: true,
    install_dir: get_option('libexecdir') / 'git-core',
  )
  test_dependencies += git_remote_http

  test_dependencies += executable('git-http-fetch',
    sources: curl_sources + 'http-fetch.c',
    dependencies: [libgit, common_main],
    install: true,
    install_dir: get_option('libexecdir') / 'git-core',
  )

  if expat.found()
    test_dependencies += executable('git-http-push',
      sources: curl_sources + 'http-push.c',
      dependencies: [libgit, common_main],
      install: true,
      install_dir: get_option('libexecdir') / 'git-core',
    )
  endif

  foreach alias : [ 'git-remote-https', 'git-remote-ftp', 'git-remote-ftps' ]
    test_dependencies += executable(alias,
      objects: git_remote_http.extract_all_objects(recursive: false),
      dependencies: [libgit, common_main],
    )

    install_symlink(alias + executable_suffix,
      install_dir: get_option('libexecdir') / 'git-core',
      pointing_to: 'git-remote-http',
    )
  endforeach
endif

imap_send_sources = ['imap-send.c']
if use_curl_for_imap_send
  imap_send_sources += curl_sources
endif

test_dependencies += executable('git-imap-send',
  sources: imap_send_sources,
  dependencies: [libgit, common_main],
  install: true,
  install_dir: get_option('libexecdir') / 'git-core',
)

foreach alias : [ 'git-receive-pack', 'git-upload-archive', 'git-upload-pack' ]
  bin_wrappers += executable(alias,
    objects: git.extract_all_objects(recursive: false),
    dependencies: [libgit, common_main],
  )

  install_symlink(alias + executable_suffix,
    install_dir: get_option('libexecdir') / 'git-core',
    pointing_to: 'git-remote-http',
  )
endforeach

foreach symlink : [
  'git',
  'git-http-backend',
  'git-receive-pack',
  'git-shell',
  'git-upload-archive',
  'git-upload-pack',
  'scalar',
]
  install_symlink(symlink,
    install_dir: get_option('bindir'),
    pointing_to: fs.relative_to(get_option('libexecdir') / 'git-core' / symlink, get_option('bindir')),
  )
endforeach

scripts_sh = [
  'git-difftool--helper.sh',
  'git-filter-branch.sh',
  'git-merge-octopus.sh',
  'git-merge-one-file.sh',
  'git-merge-resolve.sh',
  'git-mergetool--lib.sh',
  'git-mergetool.sh',
  'git-quiltimport.sh',
  'git-request-pull.sh',
  'git-sh-i18n.sh',
  'git-sh-setup.sh',
  'git-submodule.sh',
  'git-web--browse.sh',
]
if perl_features_enabled
  scripts_sh += 'git-instaweb.sh'
endif

foreach script : scripts_sh
  test_dependencies += custom_target(
    input: script,
    output: fs.stem(script),
    command: [
      shell,
      meson.project_source_root() / 'generate-script.sh',
      '@INPUT@',
      '@OUTPUT@',
      meson.project_build_root() / 'GIT-BUILD-OPTIONS',
    ],
    install: true,
    install_dir: get_option('libexecdir') / 'git-core',
  )
endforeach

if perl_features_enabled
  scripts_perl = [
    'git-archimport.perl',
    'git-cvsexportcommit.perl',
    'git-cvsimport.perl',
    'git-cvsserver.perl',
    'git-send-email.perl',
    'git-svn.perl',
  ]

  pathsep = ':'
  if host_machine.system() == 'windows'
    pathsep = ';'
  endif

  perl_header_template = 'perl/header_templates/fixed_prefix.template.pl'
  if get_option('runtime_prefix')
    perl_header_template = 'perl/header_templates/runtime_prefix.template.pl'
  endif

  perl_header = configure_file(
    input: perl_header_template,
    output: 'GIT-PERL-HEADER',
    configuration: {
      'GITEXECDIR_REL': get_option('libexecdir') / 'git-core',
      'PERLLIBDIR_REL': get_option('datadir') / 'perl5',
      'LOCALEDIR_REL': get_option('datadir') / 'locale',
      'INSTLIBDIR': get_option('datadir') / 'perl5',
      'PATHSEP': pathsep,
    },
  )

  generate_perl_command = [
    shell,
    meson.project_source_root() / 'generate-perl.sh',
    meson.project_build_root() / 'GIT-BUILD-OPTIONS',
    git_version,
    perl_header,
    '@INPUT@',
    '@OUTPUT@',
  ]

  foreach script : scripts_perl
    generated_script = custom_target(
      input: script,
      output: fs.stem(script),
      command: generate_perl_command,
      install: true,
      install_dir: get_option('datadir') / 'perl5',
    )
    test_dependencies += generated_script
    if script == 'git-cvsserver.perl'
      bin_wrappers += generated_script
    endif
  endforeach

  subdir('perl')
endif

if python.found()
  scripts_python = [
    'git-p4.py'
  ]

  foreach script : scripts_python
    fs.copyfile(script, fs.stem(script),
      install: true,
      install_dir: get_option('libexecdir') / 'git-core',
    )
  endforeach
endif

mergetools = [
  'mergetools/araxis',
  'mergetools/bc',
  'mergetools/codecompare',
  'mergetools/deltawalker',
  'mergetools/diffmerge',
  'mergetools/diffuse',
  'mergetools/ecmerge',
  'mergetools/emerge',
  'mergetools/examdiff',
  'mergetools/guiffy',
  'mergetools/gvimdiff',
  'mergetools/kdiff3',
  'mergetools/kompare',
  'mergetools/meld',
  'mergetools/nvimdiff',
  'mergetools/opendiff',
  'mergetools/p4merge',
  'mergetools/smerge',
  'mergetools/tkdiff',
  'mergetools/tortoisemerge',
  'mergetools/vimdiff',
  'mergetools/vscode',
  'mergetools/winmerge',
  'mergetools/xxdiff',
]

foreach mergetool : mergetools
  install_data(mergetool, install_dir: get_option('libexecdir') / 'git-core' / 'mergetools')
endforeach

if intl.found()
  subdir('po')
endif
subdir('contrib')
subdir('gitweb')
subdir('templates')

# Everything but the bin-wrappers need to come before this target such that we
# can properly set up test dependencies. The bin-wrappers themselves are set up
# at configuration time, so these are fine.
if get_option('tests')
  subdir('t')
endif

subdir('bin-wrappers')
if get_option('docs') != []
  subdir('Documentation')
endif
