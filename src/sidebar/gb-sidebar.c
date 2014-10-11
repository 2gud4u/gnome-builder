/*
 * Copyright (c) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author:
 *      Ikey Doherty <michael.i.doherty@intel.com>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "gb-sidebar.h"

/**
 * SECTION:gb-sidebar
 * @short_description: An automatic sidebar widget
 * @title: GbSidebar
 *
 * A GbSidebarWindow enables you to quickly and easily provide a consistent
 * "sidebar" object for your user interface.
 *
 * In order to use a GbSidebar, you simply use a GtkStack to organise
 * your UI flow, and add the sidebar to your sidebar area. You can use
 * gb_sidebar_set_stack() to connect the #GbSidebar to the #GtkStack.
 *
 * Since: 3.16
 */

struct _GbSidebarPrivate
{
  GtkListBox *list;
  GtkStack *stack;
  GHashTable *rows;
  gboolean in_child_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSidebar, gb_sidebar, GTK_TYPE_BIN)

enum
{
  PROP_0,
  PROP_STACK,
  N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
gb_sidebar_set_property (GObject    *object,
                          guint       prop_id,
                          const       GValue *value,
                          GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_STACK:
      gb_sidebar_set_stack (GB_SIDEBAR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gb_sidebar_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (GB_SIDEBAR (object));

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, priv->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_header (GtkListBoxRow *row,
               GtkListBoxRow *before,
               gpointer       userdata)
{
  GtkWidget *ret = NULL;

  if (before && !gtk_list_box_row_get_header (row))
    {
      ret = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, ret);
    }
}

static gint
sort_list (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       userdata)
{
  GbSidebar *sidebar = GB_SIDEBAR (userdata);
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  GtkWidget *widget;
  gint left = 0; gint right = 0;


  if (row1)
    {
      item = gtk_bin_get_child (GTK_BIN (row1));
      widget = g_object_get_data (G_OBJECT (item), "stack-child");
      gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                               "position", &left,
                               NULL);
    }

  if (row2)
    {
      item = gtk_bin_get_child (GTK_BIN (row2));
      widget = g_object_get_data (G_OBJECT (item), "stack-child");
      gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                               "position", &right,
                               NULL);
    }

  if (left < right)
    return  -1;

  if (left == right)
    return 0;

  return 1;
}

static void
gb_sidebar_row_selected (GtkListBox    *box,
                          GtkListBoxRow *row,
                          gpointer       userdata)
{
  GbSidebar *sidebar = GB_SIDEBAR (userdata);
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  GtkWidget *widget;

  if (priv->in_child_changed)
    return;

  if (!row)
    return;

  item = gtk_bin_get_child (GTK_BIN (row));
  widget = g_object_get_data (G_OBJECT (item), "stack-child");
  gtk_stack_set_visible_child (priv->stack, widget);
}

static void
gb_sidebar_init (GbSidebar *sidebar)
{
  GtkStyleContext *style;
  GbSidebarPrivate *priv;
  GtkWidget *sw;

  priv = gb_sidebar_get_instance_private (sidebar);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_widget_set_no_show_all (sw, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

#if 1
  gtk_container_add (GTK_CONTAINER (sidebar), sw);
#else
  _gtk_bin_set_child (GTK_BIN (sidebar), sw);
  gtk_widget_set_parent (sw, GTK_WIDGET (sidebar));
#endif

  priv->list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_show (GTK_WIDGET (priv->list));

  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (priv->list));

  gtk_list_box_set_header_func (priv->list, update_header, sidebar, NULL);
  gtk_list_box_set_sort_func (priv->list, sort_list, sidebar, NULL);

  g_signal_connect (priv->list, "row-selected",
                    G_CALLBACK (gb_sidebar_row_selected), sidebar);

  style = gtk_widget_get_style_context (GTK_WIDGET (sidebar));
  gtk_style_context_add_class (style, "sidebar");

  priv->rows = g_hash_table_new (NULL, NULL);
}

static void
update_row (GbSidebar *sidebar,
            GtkWidget  *widget,
            GtkWidget  *row)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  gchar *title;
  gboolean needs_attention;
  GtkStyleContext *context;

  gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                           "title", &title,
                           "needs-attention", &needs_attention,
                           NULL);

  item = gtk_bin_get_child (GTK_BIN (row));
  gtk_label_set_text (GTK_LABEL (item), title);

  gtk_widget_set_visible (row, gtk_widget_get_visible (widget) && title != NULL);

  context = gtk_widget_get_style_context (row);
  if (needs_attention)
     gtk_style_context_add_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);

  g_free (title);
}

static void
on_position_updated (GtkWidget  *widget,
                     GParamSpec *pspec,
                     GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);

  gtk_list_box_invalidate_sort (priv->list);
}

static void
on_child_updated (GtkWidget  *widget,
                  GParamSpec *pspec,
                  GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);
  GtkWidget *row;

  row = g_hash_table_lookup (priv->rows, widget);
  update_row (sidebar, widget, row);
}

static void
add_child (GtkWidget  *widget,
           GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);
  GtkStyleContext *style;
  GtkWidget *item;
  GtkWidget *row;

  /* Check we don't actually already know about this widget */
  if (g_hash_table_lookup (priv->rows, widget))
    return;

  /* Make a pretty item when we add kids */
  item = gtk_label_new ("");
  gtk_widget_set_halign (item, GTK_ALIGN_START);
  gtk_widget_set_valign (item, GTK_ALIGN_CENTER);
  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), item);
  gtk_widget_show (item);

  update_row (sidebar, widget, row);

  /* Fix up styling */
  style = gtk_widget_get_style_context (row);
  gtk_style_context_add_class (style, "sidebar-item");

  /* Hook up for events */
  g_signal_connect (widget, "child-notify::title",
                    G_CALLBACK (on_child_updated), sidebar);
  g_signal_connect (widget, "child-notify::needs-attention",
                    G_CALLBACK (on_child_updated), sidebar);
  g_signal_connect (widget, "notify::visible",
                    G_CALLBACK (on_child_updated), sidebar);
  g_signal_connect (widget, "child-notify::position",
                    G_CALLBACK (on_position_updated), sidebar);

  g_object_set_data (G_OBJECT (item), "stack-child", widget);
  g_hash_table_insert (priv->rows, widget, row);
  gtk_container_add (GTK_CONTAINER (priv->list), row);
}

static void
remove_child (GtkWidget  *widget,
              GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);
  GtkWidget *row;

  row = g_hash_table_lookup (priv->rows, widget);
  if (!row)
    return;

  g_signal_handlers_disconnect_by_func (widget, on_child_updated, sidebar);
  g_signal_handlers_disconnect_by_func (widget, on_position_updated, sidebar);

  gtk_container_remove (GTK_CONTAINER (priv->list), row);
  g_hash_table_remove (priv->rows, widget);
}

static void
populate_sidebar (GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);

  gtk_container_foreach (GTK_CONTAINER (priv->stack), (GtkCallback)add_child, sidebar);
}

static void
clear_sidebar (GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);

  gtk_container_foreach (GTK_CONTAINER (priv->stack), (GtkCallback)remove_child, sidebar);
}

static void
on_child_changed (GtkWidget  *widget,
                  GParamSpec *pspec,
                  GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);
  GtkWidget *child;
  GtkWidget *row;

  child = gtk_stack_get_visible_child (GTK_STACK (widget));
  row = g_hash_table_lookup (priv->rows, child);
  if (row != NULL)
    {
      priv->in_child_changed = TRUE;
      gtk_list_box_select_row (priv->list, GTK_LIST_BOX_ROW (row));
      priv->in_child_changed = FALSE;
    }
}

static void
on_stack_child_added (GtkContainer *container,
                      GtkWidget    *widget,
                      GbSidebar   *sidebar)
{
  add_child (widget, sidebar);
}

static void
on_stack_child_removed (GtkContainer *container,
                        GtkWidget    *widget,
                        GbSidebar   *sidebar)
{
  remove_child (widget, sidebar);
}

static void
disconnect_stack_signals (GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);

  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_added, sidebar);
  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_removed, sidebar);
  g_signal_handlers_disconnect_by_func (priv->stack, on_child_changed, sidebar);
  g_signal_handlers_disconnect_by_func (priv->stack, disconnect_stack_signals, sidebar);
}

static void
connect_stack_signals (GbSidebar *sidebar)
{
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);

  g_signal_connect_after (priv->stack, "add",
                          G_CALLBACK (on_stack_child_added), sidebar);
  g_signal_connect_after (priv->stack, "remove",
                          G_CALLBACK (on_stack_child_removed), sidebar);
  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (on_child_changed), sidebar);
  g_signal_connect_swapped (priv->stack, "destroy",
                            G_CALLBACK (disconnect_stack_signals), sidebar);
}

static void
gb_sidebar_dispose (GObject *object)
{
  GbSidebar *sidebar = GB_SIDEBAR (object);

  gb_sidebar_set_stack (sidebar, NULL);

  G_OBJECT_CLASS (gb_sidebar_parent_class)->dispose (object);
}

static void
gb_sidebar_finalize (GObject *object)
{
  GbSidebar *sidebar = GB_SIDEBAR (object);
  GbSidebarPrivate *priv = gb_sidebar_get_instance_private (sidebar);

  g_hash_table_destroy (priv->rows);

  G_OBJECT_CLASS (gb_sidebar_parent_class)->finalize (object);
}

static void
gb_sidebar_class_init (GbSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gb_sidebar_dispose;
  object_class->finalize = gb_sidebar_finalize;
  object_class->set_property = gb_sidebar_set_property;
  object_class->get_property = gb_sidebar_get_property;

  obj_properties[PROP_STACK] =
      g_param_spec_object ("stack",
                           _("Stack"),
                           _("Associated stack for this GbSidebar"),
                           GTK_TYPE_STACK,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

/**
 * gb_sidebar_new:
 *
 * Creates a new sidebar.
 *
 * Returns: the new #GbSidebar
 *
 * Since: 3.16
 */
GtkWidget *
gb_sidebar_new (void)
{
  return GTK_WIDGET (g_object_new (GB_TYPE_SIDEBAR, NULL));
}

/**
 * gb_sidebar_set_stack:
 * @sidebar: a #GbSidebar
 * @stack: a #GtkStack
 *
 * Set the #GtkStack associated with this #GbSidebar.
 *
 * The sidebar widget will automatically update according to the order
 * (packing) and items within the given #GtkStack.
 *
 * Since: 3.16
 */
void
gb_sidebar_set_stack (GbSidebar *sidebar,
                       GtkStack   *stack)
{
  GbSidebarPrivate *priv;

  g_return_if_fail (GB_IS_SIDEBAR (sidebar));
  g_return_if_fail (GTK_IS_STACK (stack) || stack == NULL);

  priv = gb_sidebar_get_instance_private (sidebar);

  if (priv->stack == stack)
    return;

  if (priv->stack)
    {
      disconnect_stack_signals (sidebar);
      clear_sidebar (sidebar);
      g_clear_object (&priv->stack);
    }
  if (stack)
    {
      priv->stack = g_object_ref (stack);
      populate_sidebar (sidebar);
      connect_stack_signals (sidebar);
    }

  gtk_widget_queue_resize (GTK_WIDGET (sidebar));

  g_object_notify (G_OBJECT (sidebar), "stack");
}

/**
 * gb_sidebar_get_stack:
 * @sidebar: a #GbSidebar
 *
 * Returns: (transfer full): the associated #GtkStack
 *
 * Since: 3.16
 */
GtkStack *
gb_sidebar_get_stack (GbSidebar *sidebar)
{
  GbSidebarPrivate *priv;

  g_return_val_if_fail (GB_IS_SIDEBAR (sidebar), NULL);

  priv = gb_sidebar_get_instance_private (sidebar);

  return GTK_STACK (priv->stack);
}
