/* gbp-create-project-widget.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <egg-radio-box.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <libpeas/peas.h>
#include <stdlib.h>

#include "gbp-create-project-template-icon.h"
#include "gbp-create-project-widget.h"

struct _GbpCreateProjectWidget
{
  GtkBin                parent;

  GtkEntry             *project_name_entry;
  GtkEntry             *project_location_entry;
  GtkButton            *project_location_button;
  EggRadioBox          *project_language_chooser;
  GtkFlowBox           *project_template_chooser;
  GtkSwitch            *versioning_switch;
  EggRadioBox          *license_chooser;
};

enum {
  PROP_0,
  PROP_IS_READY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE (GbpCreateProjectWidget, gbp_create_project_widget, GTK_TYPE_BIN)

static int
sort_by_name (gconstpointer a,
              gconstpointer b)
{
  const gchar * const *astr = a;
  const gchar * const *bstr = b;

  return g_utf8_collate (*astr, *bstr);
}

static void
gbp_create_project_widget_add_languages (GbpCreateProjectWidget *self,
                                         GList                  *project_templates)
{
  g_autoptr(GHashTable) languages = NULL;
  const GList *iter;
  const gchar **keys;
  guint len;
  guint i;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (iter = project_templates; iter != NULL; iter = iter->next)
    {
      IdeProjectTemplate *template = iter->data;
      g_auto(GStrv) template_languages = NULL;

      g_assert (IDE_IS_PROJECT_TEMPLATE (template));

      template_languages = ide_project_template_get_languages (template);

      for (i = 0; template_languages [i]; i++)
        g_hash_table_add (languages, g_strdup (template_languages [i]));
    }

  keys = (const gchar **)g_hash_table_get_keys_as_array (languages, &len);
  qsort (keys, len, sizeof (gchar *), sort_by_name);
  for (i = 0; keys [i]; i++)
    egg_radio_box_add_item (self->project_language_chooser, keys [i], keys [i]);
  g_free (keys);
}

static gboolean
validate_name (const gchar *name)
{
  for (; *name; name = g_utf8_next_char (name))
    {
      gunichar ch = g_utf8_get_char (name);

      if (ch == '/')
        return FALSE;
    }

  return TRUE;
}

static void
gbp_create_project_widget_name_changed (GbpCreateProjectWidget *self,
                                        GtkEntry               *entry)
{
  const gchar *text;
  g_autofree gchar *project_name = NULL;
  g_autofree gchar *project_dir = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);
  project_name = g_strstrip (g_strdup (text));

  if (ide_str_empty0 (project_name) || !validate_name (project_name))
    {
      g_object_set (self->project_name_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    NULL);

      project_dir = g_strdup ("");
    }
  else
    {
      g_object_set (self->project_name_entry,
                    "secondary-icon-name", NULL,
                    NULL);

      project_dir = g_ascii_strdown (g_strdelimit (project_name, " ", '-'), -1);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static gboolean
gbp_create_project_widget_flow_box_filter (GtkFlowBoxChild *template_container,
                                           gpointer         object)
{
  GbpCreateProjectWidget *self = object;
  GbpCreateProjectTemplateIcon *template_icon;
  IdeProjectTemplate *template;
  const gchar *language = NULL;
  g_auto(GStrv) template_languages = NULL;
  gint i;

  g_assert (GTK_IS_FLOW_BOX_CHILD (template_container));
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  language = egg_radio_box_get_active_id (self->project_language_chooser);

  if (ide_str_empty0 (language))
    return TRUE;

  template_icon = GBP_CREATE_PROJECT_TEMPLATE_ICON (gtk_bin_get_child (GTK_BIN (template_container)));
  g_object_get (template_icon,
                "template", &template,
                NULL);
  template_languages = ide_project_template_get_languages (template);
  g_object_unref (template);

  for (i = 0; template_languages [i]; i++)
    {
      if (g_str_equal (language, template_languages [i]))
        return TRUE;
    }

  gtk_flow_box_unselect_child (self->project_template_chooser, template_container);

  return FALSE;
}

static void
gbp_create_project_widget_language_changed (GbpCreateProjectWidget *self,
                                            EggRadioBox            *language_chooser)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (EGG_IS_RADIO_BOX (language_chooser));

  gtk_flow_box_invalidate_filter (self->project_template_chooser);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static void
gbp_create_project_widget_template_selected (GbpCreateProjectWidget *self,
                                             GtkFlowBox             *box,
                                             GtkFlowBoxChild        *child)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static void
gbp_create_project_widget_add_template_buttons (GbpCreateProjectWidget *self,
                                                GList                  *project_templates)
{
  const GList *iter;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  for (iter = project_templates; iter != NULL; iter = iter->next)
    {
      IdeProjectTemplate *template = iter->data;
      GbpCreateProjectTemplateIcon *template_icon;
      GtkFlowBoxChild *template_container;

      g_assert (IDE_IS_PROJECT_TEMPLATE (template));

      template_icon = g_object_new (GBP_TYPE_CREATE_PROJECT_TEMPLATE_ICON,
                                    "visible", TRUE,
                                    "template", template,
                                    NULL);

      template_container = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                                         "visible", TRUE,
                                         NULL);
      gtk_container_add (GTK_CONTAINER (template_container), GTK_WIDGET (template_icon));
      gtk_flow_box_insert (self->project_template_chooser, GTK_WIDGET (template_container), -1);
    }
}

static void
template_providers_foreach_cb (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *exten,
                               gpointer          user_data)
{
  GbpCreateProjectWidget *self = user_data;
  IdeTemplateProvider *provider = (IdeTemplateProvider *)exten;
  GList *templates;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (IDE_IS_TEMPLATE_PROVIDER (provider));

  templates = ide_template_provider_get_project_templates (provider);

  gbp_create_project_widget_add_template_buttons (self, templates);
  gbp_create_project_widget_add_languages (self, templates);

  g_list_free_full (templates, g_object_unref);
}

static gchar *
gbp_create_project_widget_get_directory (GbpCreateProjectWidget *self)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  return ide_path_expand (gtk_entry_get_text (self->project_location_entry));
}

static void
gbp_create_project_widget_set_directory (GbpCreateProjectWidget *self,
                                         const gchar            *filename)
{
  g_autofree gchar *collapsed = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  collapsed = ide_path_collapse (filename);

  gtk_entry_set_text (self->project_location_entry, collapsed);
}

static void
on_dialog_response (GbpCreateProjectWidget *self,
                    gint                    response_id,
                    GtkFileChooserDialog   *dialog)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      g_autofree gchar *path = NULL;

      path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      gbp_create_project_widget_set_directory (self, path);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static void
gbp_create_project_widget_browse_clicked (GbpCreateProjectWidget *self,
                                          GtkButton              *button)
{
  g_autofree gchar *path = NULL;
  GtkWidget *dialog;
  GtkWidget *toplevel;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  path = gbp_create_project_widget_get_directory (self);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  dialog = gtk_file_chooser_dialog_new (_("Select Project Directory"),
                                        GTK_WINDOW (toplevel),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Select"), GTK_RESPONSE_OK,
                                        NULL);
  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), path);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (on_dialog_response),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
gbp_create_project_widget_constructed (GObject *object)
{
  GbpCreateProjectWidget *self = GBP_CREATE_PROJECT_WIDGET (object);
  PeasEngine *engine;
  PeasExtensionSet *extensions;

  engine = peas_engine_get_default ();

  /* Load templates */
  extensions = peas_extension_set_new (engine, IDE_TYPE_TEMPLATE_PROVIDER, NULL);
  peas_extension_set_foreach (extensions, template_providers_foreach_cb, self);
  g_clear_object (&extensions);

  G_OBJECT_CLASS (gbp_create_project_widget_parent_class)->constructed (object);

  egg_radio_box_set_active_id (self->project_language_chooser, "C");
}

static void
gbp_create_project_widget_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_create_project_widget_parent_class)->finalize (object);
}

static gboolean
gbp_create_project_widget_is_ready (GbpCreateProjectWidget *self)
{
  const gchar *text;
  g_autofree gchar *project_name = NULL;
  const gchar *language = NULL;
  GList *selected_template = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  text = gtk_entry_get_text (self->project_name_entry);
  project_name = g_strstrip (g_strdup (text));

  if (ide_str_empty0 (project_name) || !validate_name (project_name))
    return FALSE;

  language = egg_radio_box_get_active_id (self->project_language_chooser);

  if (ide_str_empty0 (language))
    return FALSE;

  selected_template = gtk_flow_box_get_selected_children (self->project_template_chooser);

  if (selected_template == NULL)
    return FALSE;

  g_list_free (selected_template);

  return TRUE;
}

static void
gbp_create_project_widget_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpCreateProjectWidget *self = GBP_CREATE_PROJECT_WIDGET(object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      g_value_set_boolean (value, gbp_create_project_widget_is_ready (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_create_project_widget_class_init (GbpCreateProjectWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_create_project_widget_constructed;
  object_class->finalize = gbp_create_project_widget_finalize;
  object_class->get_property = gbp_create_project_widget_get_property;

  properties [PROP_IS_READY] =
    g_param_spec_boolean ("is-ready",
                          "Is Ready",
                          "Is Ready",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "createprojectwidget");
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/plugins/create-project-plugin/gbp-create-project-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_location_button);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_location_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_language_chooser);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_template_chooser);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, versioning_switch);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, license_chooser);
}

static void
gbp_create_project_widget_init (GbpCreateProjectWidget *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *path = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  settings = g_settings_new ("org.gnome.builder");

  path = g_settings_get_string (settings, "projects-directory");
  gbp_create_project_widget_set_directory (self, path);

  gtk_flow_box_set_filter_func (self->project_template_chooser,
                                gbp_create_project_widget_flow_box_filter,
                                self,
                                NULL);

  g_signal_connect_object (self->project_name_entry,
                           "changed",
                           G_CALLBACK (gbp_create_project_widget_name_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_location_button,
                           "clicked",
                           G_CALLBACK (gbp_create_project_widget_browse_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_language_chooser,
                           "changed",
                           G_CALLBACK (gbp_create_project_widget_language_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_template_chooser,
                           "child-activated",
                           G_CALLBACK (gbp_create_project_widget_template_selected),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
init_vcs_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeVcsInitializer *vcs = (IdeVcsInitializer *)object;
  GbpCreateProjectWidget *self;
  IdeWorkbench *workbench;
  GFile *project_file;
  GError *error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_vcs_initializer_initialize_finish (vcs, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  self = g_task_get_source_object (task);
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  project_file = g_task_get_task_data (task);
  g_assert (G_IS_FILE (project_file));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  ide_workbench_open_project_async (workbench, project_file, NULL, NULL, NULL);

  g_task_return_boolean (task, TRUE);
}

static void
extract_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  IdeProjectTemplate *template = (IdeProjectTemplate *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeVcsInitializer) vcs = NULL;
  GbpCreateProjectWidget *self;
  IdeWorkbench *workbench;
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  GFile *project_file;
  GError *error = NULL;

  /* To keep the UI simple, we only support git from
   * the creation today. However, at the time of writing
   * that is our only supported VCS anyway. If you'd like to
   * add support for an additional VCS, we need to redesign
   * this part of the UI.
   */
  const gchar *vcs_id = "git-plugin";

  g_assert (IDE_IS_PROJECT_TEMPLATE (template));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_project_template_expand_finish (template, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  self = g_task_get_source_object (task);
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  project_file = g_task_get_task_data (task);
  g_assert (G_IS_FILE (project_file));

  if (!gtk_switch_get_active (self->versioning_switch))
    {
      workbench = ide_widget_get_workbench (GTK_WIDGET (self));
      ide_workbench_open_project_async (workbench, project_file, NULL, NULL, NULL);
      g_task_return_boolean (task, TRUE);
      return;
    }

  engine = peas_engine_get_default ();
  plugin_info = peas_engine_get_plugin_info (engine, vcs_id);
  if (plugin_info == NULL)
    goto failure;

  vcs = (IdeVcsInitializer *)peas_engine_create_extension (engine, plugin_info,
                                                           IDE_TYPE_VCS_INITIALIZER,
                                                           NULL);
  if (vcs == NULL)
    goto failure;

  ide_vcs_initializer_initialize_async (vcs,
                                        project_file,
                                        g_task_get_cancellable (task),
                                        init_vcs_cb,
                                        g_object_ref (task));

  return;

failure:
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("A failure occurred while initializing version control"));
}

void
gbp_create_project_widget_create_async (GbpCreateProjectWidget *self,
                                        GCancellable           *cancellable,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GHashTable) params = NULL;
  g_autoptr(IdeProjectTemplate) template = NULL;
  g_autoptr(IdeVcsConfig) vcs_conf = NULL;
  GValue str = G_VALUE_INIT;
  g_autofree gchar *name = NULL;
  g_autofree gchar *location = NULL;
  g_autofree gchar *path = NULL;
  const gchar *language = NULL;
  const gchar *license_id = NULL;
  GtkFlowBoxChild *template_container;
  GbpCreateProjectTemplateIcon *template_icon;
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  const gchar *text;
  const gchar *vcs_id = "git-plugin";
  const gchar *author_name;
  GList *selected_box_child;

  g_return_if_fail (GBP_CREATE_PROJECT_WIDGET (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  selected_box_child = gtk_flow_box_get_selected_children (self->project_template_chooser);
  template_container = selected_box_child->data;
  template_icon = GBP_CREATE_PROJECT_TEMPLATE_ICON (gtk_bin_get_child (GTK_BIN (template_container)));
  g_object_get (template_icon,
                "template", &template,
                NULL);
  g_list_free (selected_box_child);

  params = g_hash_table_new_full (g_str_hash,
                                  g_str_equal,
                                  g_free,
                                  (GDestroyNotify)g_variant_unref);

  text = gtk_entry_get_text (self->project_name_entry);
  name = g_strstrip (g_strdup (text));
  g_hash_table_insert (params,
                       g_strdup ("name"),
                       g_variant_ref_sink (g_variant_new_string (g_strdelimit (name, " ", '-'))));

  location = gbp_create_project_widget_get_directory (self);
  path = g_build_filename (location, name, NULL);

  g_hash_table_insert (params,
                       g_strdup ("path"),
                       g_variant_ref_sink (g_variant_new_string (path)));

  language = egg_radio_box_get_active_id (self->project_language_chooser);
  g_hash_table_insert (params,
                       g_strdup ("language"),
                       g_variant_ref_sink (g_variant_new_string (language)));

  license_id = egg_radio_box_get_active_id (EGG_RADIO_BOX (self->license_chooser));

  if (!g_str_equal (license_id, "none"))
    {
      g_autofree gchar *license_full_path = NULL;
      g_autofree gchar *license_short_path = NULL;

      license_full_path = g_strjoin (NULL, "resource://", "/org/gnome/builder/plugins/create-project-plugin/license/full/", license_id, NULL);
      license_short_path = g_strjoin (NULL, "resource://", "/org/gnome/builder/plugins/create-project-plugin/license/short/", license_id, NULL);

      g_hash_table_insert (params,
                           g_strdup ("license_full"),
                           g_variant_ref_sink (g_variant_new_string (license_full_path)));

      g_hash_table_insert (params,
                           g_strdup ("license_short"),
                           g_variant_ref_sink (g_variant_new_string (license_short_path)));
    }

  if (gtk_switch_get_active (self->versioning_switch))
    {
      engine = peas_engine_get_default ();
      plugin_info = peas_engine_get_plugin_info (engine, vcs_id);

      if (plugin_info != NULL)
        {
          vcs_conf = (IdeVcsConfig *)peas_engine_create_extension (engine, plugin_info,
                                                                   IDE_TYPE_VCS_CONFIG,
                                                                   NULL);

          if (vcs_conf != NULL)
            {
              g_value_init (&str, G_TYPE_STRING);
              ide_vcs_config_get_config (vcs_conf, IDE_VCS_CONFIG_FULL_NAME, &str);
            }
        }
    }

  if (G_VALUE_HOLDS_STRING (&str) && !ide_str_empty0 (g_value_get_string (&str)))
    author_name = g_value_get_string (&str);
  else
    author_name = g_get_real_name ();

  g_hash_table_insert (params,
                       g_strdup ("author"),
                       g_variant_ref_sink (g_variant_new_string (author_name)));

  g_value_unset (&str);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_file_new_for_path (path), g_object_unref);

  ide_project_template_expand_async (template,
                                     params,
                                     NULL,
                                     extract_cb,
                                     g_object_ref (task));
}

gboolean
gbp_create_project_widget_create_finish (GbpCreateProjectWidget *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  g_return_val_if_fail (GBP_IS_CREATE_PROJECT_WIDGET (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
