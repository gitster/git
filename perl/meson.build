perl_config = {
  'LOCALEDIR': get_option('prefix') / get_option('datadir') / 'locale',
  'NO_GETTEXT': iconv.found() ? '' : '1',
  'NO_PERL_CPAN_FALLBACKS': get_option('perl_cpan_fallback') ? '' : '1',
}

configure_file(
  input: 'Git.pm',
  output: 'Git.pm',
  configuration: perl_config,
  install: true,
  install_dir: get_option('datadir') / 'perl5',
)

subdir('Git')
if get_option('perl_cpan_fallback')
  subdir('FromCPAN')
endif
