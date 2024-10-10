foreach source : [
  'I18N.pm',
  'IndexInfo.pm',
  'LoadCPAN.pm',
  'Packet.pm',
  'SVN.pm',
]
  configure_file(
    input: source,
    output: source,
    configuration: perl_config,
    install: true,
    install_dir: get_option('datadir') / 'perl5/Git',
  )
endforeach

subdir('LoadCPAN')
subdir('SVN')
