/* gb-terminal-view.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <ide.h>
#include <vte/vte.h>

#include "gb-terminal-document.h"
#include "gb-terminal-view.h"
#include "gb-terminal-view-private.h"
#include "gb-terminal-view-actions.h"
#include "gb-view.h"
#include "gb-widget.h"
#include "gb-workbench.h"

G_DEFINE_TYPE (GbTerminalView, gb_terminal_view, GB_TYPE_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_FONT_NAME,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static const GdkRGBA solarized_palette[] =
{
  /*
   * Solarized palette (1.0.0beta2):
   * http://ethanschoonover.com/solarized
   */
  { 0.02745,  0.211764, 0.258823, 1 },
  { 0.862745, 0.196078, 0.184313, 1 },
  { 0.521568, 0.6,      0,        1 },
  { 0.709803, 0.537254, 0,        1 },
  { 0.149019, 0.545098, 0.823529, 1 },
  { 0.82745,  0.211764, 0.509803, 1 },
  { 0.164705, 0.631372, 0.596078, 1 },
  { 0.933333, 0.909803, 0.835294, 1 },
  { 0,        0.168627, 0.211764, 1 },
  { 0.796078, 0.294117, 0.086274, 1 },
  { 0.345098, 0.431372, 0.458823, 1 },
  { 0.396078, 0.482352, 0.513725, 1 },
  { 0.513725, 0.580392, 0.588235, 1 },
  { 0.423529, 0.443137, 0.768627, 1 },
  { 0.57647,  0.631372, 0.631372, 1 },
  { 0.992156, 0.964705, 0.890196, 1 },
};

static void gb_terminal_view_connect_terminal    (GbTerminalView *self, VteTerminal *terminal);

static GbDocument *
gb_terminal_view_get_document (GbView *view)
{
  g_return_val_if_fail (GB_IS_TERMINAL_VIEW (view), NULL);

  return GB_DOCUMENT (GB_TERMINAL_VIEW (view)->document);
}

static void
gb_terminal_view_set_document (GbTerminalView     *view,
                               GbTerminalDocument *document)
{
  g_return_if_fail (GB_IS_TERMINAL_VIEW (view));

  if (view->document != document)
    {
      if (view->document)
        g_clear_object (&view->document);

      if (document)
        view->document = g_object_ref (document);

      g_object_notify (G_OBJECT (view), "document");
    }
}

static void
gb_terminal_respawn (GbTerminalView *self,
                     VteTerminal    *terminal)
{
  g_autoptr(GPtrArray) args = NULL;
  g_autofree gchar *workpath = NULL;
  GtkWidget *toplevel;
  GError *error = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  GPid child_pid;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  vte_terminal_reset (terminal, TRUE, TRUE);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!GB_IS_WORKBENCH (toplevel))
    return;

  context = gb_workbench_get_context (GB_WORKBENCH (toplevel));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  workpath = g_file_get_path (workdir);

  args = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (args, vte_get_user_shell ());
  g_ptr_array_add (args, NULL);

  vte_terminal_spawn_sync (terminal,
                           VTE_PTY_DEFAULT | VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP,
                           workpath,
                           (gchar **)args->pdata,
                           NULL,
                           G_SPAWN_DEFAULT,
                           NULL,
                           NULL,
                           &child_pid,
                           NULL,
                           &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  vte_terminal_watch_child (terminal, child_pid);
}

static void
child_exited_cb (VteTerminal    *terminal,
                 gint            exit_status,
                 GbTerminalView *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  if (!gb_widget_activate_action (GTK_WIDGET (self), "view-stack", "close", NULL))
    {
      if (!gtk_widget_in_destruction (GTK_WIDGET (terminal)))
        gb_terminal_respawn (self, terminal);
    }
}

static void
gb_terminal_realize (GtkWidget *widget)
{
  GbTerminalView *self = (GbTerminalView *)widget;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  GTK_WIDGET_CLASS (gb_terminal_view_parent_class)->realize (widget);

  if (!self->top_has_spawned)
    {
      self->top_has_spawned = TRUE;
      gb_terminal_respawn (self, self->terminal_top);
    }
}

static void
size_allocate_cb (VteTerminal    *terminal,
                  GtkAllocation  *alloc,
                  GbTerminalView *self)
{
  glong width;
  glong height;
  glong columns;
  glong rows;

  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (alloc != NULL);
  g_assert (GB_IS_TERMINAL_VIEW (self));

  if ((alloc->width == 0) || (alloc->height == 0))
    return;

  width = vte_terminal_get_char_width (terminal);
  height = vte_terminal_get_char_height (terminal);

  if ((width == 0) || (height == 0))
    return;

  columns = alloc->width / width;
  rows = alloc->height / height;

  if ((columns < 2) || (rows < 2))
    return;

  vte_terminal_set_size (terminal, columns, rows);
}

static void
gb_terminal_get_preferred_width (GtkWidget *widget,
                                 gint      *min_width,
                                 gint      *nat_width)
{
  /*
   * Since we are placing the terminal in a GtkStack, we need
   * to fake the size a bit. Otherwise, GtkStack tries to keep the
   * widget at it's natural size (which prevents us from getting
   * appropriate size requests.
   */
  GTK_WIDGET_CLASS (gb_terminal_view_parent_class)->get_preferred_width (widget, min_width, nat_width);
  *nat_width = *min_width;
}

static void
gb_terminal_get_preferred_height (GtkWidget *widget,
                                  gint      *min_height,
                                  gint      *nat_height)
{
  /*
   * Since we are placing the terminal in a GtkStack, we need
   * to fake the size a bit. Otherwise, GtkStack tries to keep the
   * widget at it's natural size (which prevents us from getting
   * appropriate size requests.
   */
  GTK_WIDGET_CLASS (gb_terminal_view_parent_class)->get_preferred_height (widget, min_height, nat_height);
  *nat_height = *min_height;
}

static void
gb_terminal_set_needs_attention (GbTerminalView  *self,
                                 gboolean         needs_attention,
                                 GtkPositionType  position)
{
  GtkWidget *parent;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (GTK_IS_STACK (parent) &&
      !gtk_widget_in_destruction (GTK_WIDGET (self)) &&
      !gtk_widget_in_destruction (parent))
    {
      if (position == GTK_POS_TOP &&
          !gtk_widget_in_destruction (GTK_WIDGET (self->terminal_top)))
        {
          self->top_has_needs_attention = TRUE;
        }
      else if (position == GTK_POS_BOTTOM &&
               self->terminal_bottom != NULL &&
               !gtk_widget_in_destruction (GTK_WIDGET (self->terminal_bottom)))
        {
          self->bottom_has_needs_attention = TRUE;
        }

      gtk_container_child_set (GTK_CONTAINER (parent), GTK_WIDGET (self),
                               "needs-attention",
                               !!(self->top_has_needs_attention || self->bottom_has_needs_attention) &&
                               needs_attention,
                               NULL);
    }
}

static void
notification_received_cb (VteTerminal    *terminal,
                          const gchar    *summary,
                          const gchar    *body,
                          GbTerminalView *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  if (!gtk_widget_has_focus (GTK_WIDGET (terminal)))
    {
      if (terminal == self->terminal_top)
        gb_terminal_set_needs_attention (self, TRUE, GTK_POS_TOP);
      else if (terminal == self->terminal_bottom)
        gb_terminal_set_needs_attention (self, TRUE, GTK_POS_BOTTOM);
    }
}

static const gchar *
gb_terminal_get_title (GbView *view)
{
  const gchar *title;
  GbTerminalView *self = (GbTerminalView *)view;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  if (self->bottom_has_focus)
    title = vte_terminal_get_window_title (self->terminal_bottom);
  else
    title = vte_terminal_get_window_title (self->terminal_top);

  return title;
}

static gboolean
focus_in_event_cb (VteTerminal    *terminal,
                   GdkEvent       *event,
                   GbTerminalView *self)
{
  const gchar *title;

  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  self->bottom_has_focus = (terminal != self->terminal_top);

  title = gb_terminal_get_title (GB_VIEW (self));
  if (self->document)
    gb_terminal_document_set_title (self->document, title);

  if (terminal == self->terminal_top)
    {
      self->top_has_needs_attention = FALSE;
      gb_terminal_set_needs_attention (self, FALSE, GTK_POS_TOP);
    }
  else if (terminal == self->terminal_bottom)
    {
      self->bottom_has_needs_attention = FALSE;
      gb_terminal_set_needs_attention (self, FALSE, GTK_POS_BOTTOM);
    }

  g_object_notify (G_OBJECT (self), "title");

  return GDK_EVENT_PROPAGATE;
}

static void
window_title_changed_cb (VteTerminal    *terminal,
                         GbTerminalView *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  g_object_notify (G_OBJECT (self), "title");
}

static void
style_context_changed (GtkStyleContext *style_context,
                       GbTerminalView  *self)
{
  GdkRGBA fg;
  GdkRGBA bg;

  g_assert (GTK_IS_STYLE_CONTEXT (style_context));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  gtk_style_context_get_color (style_context, GTK_STATE_FLAG_NORMAL, &fg);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_NORMAL, &bg);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (bg.alpha == 0.0)
    {
      gdk_rgba_parse (&bg, "#f6f7f8");
    }

  vte_terminal_set_colors (self->terminal_top, &fg, &bg,
                           solarized_palette,
                           G_N_ELEMENTS (solarized_palette));

  if (self->terminal_bottom)
    vte_terminal_set_colors (self->terminal_bottom, &fg, &bg,
                             solarized_palette,
                             G_N_ELEMENTS (solarized_palette));
}

static GbView *
gb_terminal_create_split (GbView *view)
{
  GbView *new_view;

  g_assert (GB_IS_TERMINAL_VIEW (view));

  new_view = g_object_new (GB_TYPE_TERMINAL_VIEW,
                          "document", gb_terminal_view_get_document (view),
                          "visible", TRUE,
                          NULL);

  return new_view;
}

static void
gb_terminal_view_set_font_name (GbTerminalView *self,
                                const gchar    *font_name)
{
  PangoFontDescription *font_desc = NULL;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  if (font_name != NULL)
    font_desc = pango_font_description_from_string (font_name);

  if (font_desc != NULL)
    {
      vte_terminal_set_font (self->terminal_top, font_desc);

      if (self->terminal_bottom)
        vte_terminal_set_font (self->terminal_bottom, font_desc);

      pango_font_description_free (font_desc);
    }
}

static void
gb_terminal_set_split_view (GbView   *view,
                            gboolean  split_view)
{
  GbTerminalView *self = (GbTerminalView *)view;
  GtkStyleContext *style_context;

  g_assert (GB_IS_TERMINAL_VIEW (self));
  g_return_if_fail (GB_IS_TERMINAL_VIEW (self));

  if (split_view && (self->terminal_bottom != NULL))
    return;

  if (!split_view && (self->terminal_bottom == NULL))
    return;

  if (split_view)
    {
      style_context = gtk_widget_get_style_context (GTK_WIDGET (view));

      self->terminal_bottom = g_object_new (VTE_TYPE_TERMINAL,
                                            "audible-bell", FALSE,
                                            "expand", TRUE,
                                            "visible", TRUE,
                                            NULL);
      gtk_container_add (GTK_CONTAINER (self->scrolled_window_bottom), GTK_WIDGET (self->terminal_bottom));
      gtk_widget_show (self->scrolled_window_bottom);

      gb_terminal_view_connect_terminal (self, self->terminal_bottom);
      style_context_changed (style_context, GB_TERMINAL_VIEW (view));

      gtk_widget_grab_focus (GTK_WIDGET (self->terminal_bottom));

      if (!self->bottom_has_spawned)
        {
          self->bottom_has_spawned = TRUE;
          gb_terminal_respawn (self, self->terminal_bottom);
        }
    }
  else
    {
      gtk_container_remove (GTK_CONTAINER (self->scrolled_window_bottom),
                            GTK_WIDGET (self->terminal_bottom));
      gtk_widget_hide (self->scrolled_window_bottom);

      self->terminal_bottom = NULL;
      self->bottom_has_focus = FALSE;
      self->bottom_has_spawned = FALSE;
      self->bottom_has_needs_attention = FALSE;
      g_clear_object (&self->save_as_file_bottom);
      gtk_widget_grab_focus (GTK_WIDGET (self->terminal_top));
    }
}

static void
gb_terminal_grab_focus (GtkWidget *widget)
{
  GbTerminalView *self = (GbTerminalView *)widget;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  if (self->bottom_has_focus && self->terminal_bottom)
    gtk_widget_grab_focus (GTK_WIDGET (self->terminal_bottom));
  else
    gtk_widget_grab_focus (GTK_WIDGET (self->terminal_top));
}

static void
gb_terminal_view_connect_terminal (GbTerminalView *self,
                                   VteTerminal    *terminal)
{
  GQuark quark;
  guint signal_id;

  g_signal_connect_object (terminal,
                           "size-allocate",
                           G_CALLBACK (size_allocate_cb),
                           self,
                           0);

  g_signal_connect_object (terminal,
                           "child-exited",
                           G_CALLBACK (child_exited_cb),
                           self,
                           0);

  g_signal_connect_object (terminal,
                           "focus-in-event",
                           G_CALLBACK (focus_in_event_cb),
                           self,
                           0);

  g_signal_connect_object (terminal,
                           "window-title-changed",
                           G_CALLBACK (window_title_changed_cb),
                           self,
                           0);

  if (g_signal_parse_name ("notification-received",
                           VTE_TYPE_TERMINAL,
                           &signal_id,
                           &quark,
                           FALSE))
    {
      g_signal_connect_object (terminal,
                               "notification-received",
                               G_CALLBACK (notification_received_cb),
                               self,
                               0);
    }
}

static void
gb_terminal_view_finalize (GObject *object)
{
  GbTerminalView *self = GB_TERMINAL_VIEW (object);

  g_clear_object (&self->document);
  g_clear_object (&self->save_as_file_top);
  g_clear_object (&self->save_as_file_bottom);
  g_clear_pointer (&self->selection_buffer, g_free);

  G_OBJECT_CLASS (gb_terminal_view_parent_class)->finalize (object);
}

static void
gb_terminal_view_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbTerminalView *self = GB_TERMINAL_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, self->document);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbTerminalView *self = GB_TERMINAL_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      gb_terminal_view_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_DOCUMENT:
      gb_terminal_view_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_view_class_init (GbTerminalViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbViewClass *view_class = GB_VIEW_CLASS (klass);

  object_class->finalize = gb_terminal_view_finalize;
  object_class->get_property = gb_terminal_view_get_property;
  object_class->set_property = gb_terminal_view_set_property;

  widget_class->realize = gb_terminal_realize;
  widget_class->get_preferred_width = gb_terminal_get_preferred_width;
  widget_class->get_preferred_height = gb_terminal_get_preferred_height;
  widget_class->grab_focus = gb_terminal_grab_focus;

  view_class->get_title = gb_terminal_get_title;
  view_class->get_document = gb_terminal_view_get_document;
  view_class->create_split = gb_terminal_create_split;
  view_class->set_split_view =  gb_terminal_set_split_view;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/terminal/gb-terminal-view.ui");
  gtk_widget_class_bind_template_child (widget_class, GbTerminalView, terminal_top);
  gtk_widget_class_bind_template_child (widget_class, GbTerminalView, scrolled_window_bottom);

  g_type_ensure (VTE_TYPE_TERMINAL);

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document for the Vte Terminal view."),
                         GB_TYPE_TERMINAL_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_FONT_NAME] =
    g_param_spec_string ("font-name",
                         _("Font Name"),
                         _("Font Name"),
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_terminal_view_init (GbTerminalView *self)
{
  GtkStyleContext *style_context;
  g_autoptr(GSettings) settings = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_terminal_view_connect_terminal (self, self->terminal_top);
  gb_terminal_view_actions_init (self);

  /*
   * FIXME: Should we allow setting the terminal font independently from editor?
   */
  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_bind (settings, "font-name", self, "font-name", G_SETTINGS_BIND_GET);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  g_signal_connect_object (style_context,
                           "changed",
                           G_CALLBACK (style_context_changed),
                           self,
                           0);
  style_context_changed (style_context, self);
}
