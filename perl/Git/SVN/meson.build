foreach source : [
  'Editor.pm',
  'Fetcher.pm',
  'GlobSpec.pm',
  'Log.pm',
  'Migration.pm',
  'Prompt.pm',
  'Ra.pm',
  'Utils.pm',
]
  configure_file(
    input: source,
    output: source,
    configuration: perl_config,
    install: true,
    install_dir: get_option('datadir') / 'perl5/Git/SVN',
  )
endforeach

subdir('Memoize')
