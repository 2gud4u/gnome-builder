schema_dir = join_paths(get_option('datadir'), 'glib-2.0', 'schemas')

# Perhaps this should be moved to the plugin
ctags_path = 'ctags'
ctags = find_program(
  'ctags-exuberant',
  'exctags',
  'ectags',
  'ctags',
  required: false
)
if ctags.found()
  ctags_path = ctags.path()
endif

schema_conf = configuration_data()
schema_conf.set('ECTAGS', ctags_path)

configure_file(
  input: 'org.gnome.builder.code-insight.gschema.xml.in',
  output: 'org.gnome.builder.code-insight.gschema.xml',
  configuration: schema_conf,
  install: true,
  install_dir: schema_dir,
)

install_data([
  'org.gnome.builder.build.gschema.xml',
  'org.gnome.builder.editor.gschema.xml',
  'org.gnome.builder.editor.language.gschema.xml',
  'org.gnome.builder.extension-type.gschema.xml',
  'org.gnome.builder.gschema.xml',
  'org.gnome.builder.plugin.gschema.xml',
  'org.gnome.builder.project.gschema.xml',
  'org.gnome.builder.project-tree.gschema.xml',
  'org.gnome.builder.terminal.gschema.xml',
  'org.gnome.builder.workbench.gschema.xml',
], install_dir: schema_dir)
