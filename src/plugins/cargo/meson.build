if get_option('with_cargo')

install_data('cargo_plugin.py', install_dir: plugindir)

configure_file(
          input: 'cargo.plugin',
         output: 'cargo.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
