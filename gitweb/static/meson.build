foreach asset : [
  'git-favicon.png',
  'git-logo.png',
  'gitweb.css',
]
  fs.copyfile(asset,
    install: true,
    install_dir: get_option('datadir') / 'gitweb/static',
  )
endforeach

javascript_sources = [
  meson.current_source_dir() / 'js/adjust-timezone.js',
  meson.current_source_dir() / 'js/blame_incremental.js',
  meson.current_source_dir() / 'js/javascript-detection.js',
  meson.current_source_dir() / 'js/lib/common-lib.js',
  meson.current_source_dir() / 'js/lib/cookies.js',
  meson.current_source_dir() / 'js/lib/datetime.js',
]

custom_target(
  input: javascript_sources,
  output: 'gitweb.js',
  capture: true,
  command: ['cat'] + javascript_sources,
  install: true,
  install_dir: get_option('datadir') / 'gitweb/static',
)
