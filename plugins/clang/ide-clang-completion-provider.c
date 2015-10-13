/* ide-clang-completion-provider.c
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

#define G_LOG_DOMAIN "clang-completion-provider"

#include <string.h>

#include "ide-buffer.h"
#include "ide-completion-provider.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-completion-item-private.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"

struct _IdeClangCompletionProvider
{
  IdeObject      parent_instance;

  GSettings     *settings;
  gchar         *last_line;
  GPtrArray     *last_results;
  gchar         *last_query;
  /*
   * As an optimization, the linked list for result nodes are
   * embedded in the IdeClangCompletionItem structures and we
   * do not allocate them. This is the pointer to the first item
   * in the result set that matches our query. It is not allocated
   * and do not tree to free it or perform g_list_*() operations
   * upon it.
   */
  GList         *head;
  /*
   * We save a weak pointer to the view that performed the request
   * so that we can push a snippet onto the view instead of inserting
   * text into the buffer.
   */
  IdeSourceView *view;
};

typedef struct
{
  IdeClangCompletionProvider *self;
  GtkSourceCompletionContext *context;
  IdeFile *file;
  GCancellable *cancellable;
  gchar *line;
  gchar *query;
} IdeClangCompletionState;

static void ide_clang_completion_provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangCompletionProvider,
                        ide_clang_completion_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               ide_clang_completion_provider_iface_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

static void
ide_clang_completion_state_free (IdeClangCompletionState *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->cancellable);
  g_clear_object (&state->context);
  g_clear_object (&state->file);
  g_clear_pointer (&state->line, g_free);
  g_clear_pointer (&state->query, g_free);
  g_slice_free (IdeClangCompletionState, state);
}

static gchar *
ide_clang_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup ("Clang");
}

static gint
ide_clang_completion_provider_get_priority (GtkSourceCompletionProvider *provider)
{
  return IDE_CLANG_COMPLETION_PROVIDER_PRIORITY;
}

static gboolean
ide_clang_completion_provider_match (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionContext  *context)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  GtkSourceCompletionActivation activation;
  GtkTextBuffer *buffer;
  IdeFile *file;
  GtkTextIter iter;

  g_return_val_if_fail (IDE_IS_CLANG_COMPLETION_PROVIDER (self), FALSE);
  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), FALSE);

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return FALSE;

  buffer = gtk_text_iter_get_buffer (&iter);
  if (!IDE_IS_BUFFER (buffer) ||
      !(file = ide_buffer_get_file (IDE_BUFFER (buffer))) ||
      ide_file_get_is_temporary (file))
    return FALSE;

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      if (gtk_text_iter_starts_line (&iter) ||
          !gtk_text_iter_backward_char (&iter) ||
          g_unichar_isspace (gtk_text_iter_get_char (&iter)))
        return FALSE;
    }

  if (!g_settings_get_boolean (self->settings, "clang-autocompletion"))
    return FALSE;

  return TRUE;
}

static gboolean
ide_clang_completion_provider_can_replay (IdeClangCompletionProvider *self,
                                          const gchar                *line)
{
  const gchar *suffix;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));

  if (self->last_results == NULL)
    { g_print (">>>> %s\n", G_STRLOC); return FALSE; }

  if (line == NULL || self->last_line == NULL)
    { g_print (">>>> %s\n", G_STRLOC); return FALSE; }

  if (!g_str_has_prefix (line, self->last_line))
    { g_print (">>>> %s\n", G_STRLOC); return FALSE; }

  suffix = line + strlen (self->last_line);

  for (; *suffix; suffix = g_utf8_next_char (suffix))
    {
      gunichar ch = g_utf8_get_char (suffix);
      if (!g_unichar_isalnum (ch) && (ch != '_'))
        { g_print (">>>> %s\n", G_STRLOC); return FALSE; }
    }

  return TRUE;
}

static void
ide_clang_completion_provider_save_results (IdeClangCompletionProvider *self,
                                            GPtrArray                  *results,
                                            const gchar                *line,
                                            const gchar                *query)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));

  g_clear_pointer (&self->last_results, g_ptr_array_unref);
  g_clear_pointer (&self->last_line, g_free);
  g_clear_pointer (&self->last_query, g_free);
  self->head = NULL;

  if (results != NULL)
    {
      self->last_line = g_strdup (line);
      self->last_query = g_strdup (query);
      self->last_results = g_ptr_array_ref (results);
      if (results->len > 0)
        {
          IdeClangCompletionItem *head = g_ptr_array_index (results, 0);
          self->head = &head->link;
        }
    }

  IDE_EXIT;
}

static void
ide_clang_completion_provider_update_links (IdeClangCompletionProvider *self,
                                            GPtrArray                  *results)
{
  IdeClangCompletionItem *item;
  IdeClangCompletionItem *next;
  IdeClangCompletionItem *prev;
  guint i;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (results != NULL);

  if (results->len == 0)
    {
      self->head = NULL;
      return;
    }

  /* Unrolling loops for the gentoo crowd */

  item = g_ptr_array_index (results, 0);
  item->link.prev = NULL;
  item->link.next = (results->len == 1) ? NULL : &((IdeClangCompletionItem*)g_ptr_array_index (results, 1))->link;

  self->head = &item->link;

  prev = item;

  for (i = 1; i < (results->len - 1); i++)
    {
      item = g_ptr_array_index (results, i);
      next = g_ptr_array_index (results, i + 1);

      item->link.prev = &prev->link;
      item->link.next = &next->link;

      prev = item;
    }

  if (results->len > 1)
    {
      item = g_ptr_array_index (results, results->len - 1);
      item->link.prev = &((IdeClangCompletionItem*)g_ptr_array_index (results, results->len-2))->link;
      item->link.next = NULL;
    }
}

static void
ide_clang_completion_provider_refilter (IdeClangCompletionProvider *self,
                                        GPtrArray                  *results,
                                        const gchar                *query)
{
  g_autofree gchar *lower = NULL;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (results != NULL);
  g_assert (results->len > 0);
  g_assert (query != NULL);

  /*
   * By traversing the linked list nodes instead of the array, we allow
   * ourselves to avoid rechecking items we already know filtered.
   * We do need to be mindful of this in case the user backspaced
   * and our list is no longer a continual "deep dive" of matched items.
   */
  if ((self->last_query != NULL) && g_str_has_prefix (query, self->last_query))
    ide_clang_completion_provider_update_links (self, results);

  lower = g_utf8_strdown (query, -1);

  if (!g_str_is_ascii (lower))
    {
      g_warning ("Item filtering requires ascii input.");
      return;
    }

  for (GList *iter = self->head; iter; iter = iter->next)
    {
      IdeClangCompletionItem *item = iter->data;

      if (!ide_clang_completion_item_match (item, lower))
        {
          if (iter->prev != NULL)
            iter->prev->next = iter->next;
          else
            self->head = iter->next;

          if (iter->next != NULL)
            iter->next->prev = iter->prev;
        }
    }

  g_free (self->last_query);
  self->last_query = g_strdup (query);
}

static void
ide_clang_completion_provider_code_complete_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  IdeClangTranslationUnit *unit = (IdeClangTranslationUnit *)object;
  IdeClangCompletionState *state = user_data;
  g_autoptr(GPtrArray) results = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (unit));
  g_assert (state != NULL);
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (state->self));
  g_assert (G_IS_CANCELLABLE (state->cancellable));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));

  if (!(results = ide_clang_translation_unit_code_complete_finish (unit, result, &error)))
    {
      g_debug ("%s", error->message);
      if (!g_cancellable_is_cancelled (state->cancellable))
        gtk_source_completion_context_add_proposals (state->context,
                                                     GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                     NULL, TRUE);
      ide_clang_completion_state_free (state);
      IDE_EXIT;
    }

  ide_clang_completion_provider_save_results (state->self, results, state->line, state->query);
  ide_clang_completion_provider_update_links (state->self, results);

  if (!g_cancellable_is_cancelled (state->cancellable))
    {
      if (results->len > 0)
        {
          ide_clang_completion_provider_refilter (state->self, results, state->query);
          gtk_source_completion_context_add_proposals (state->context,
                                                       GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                       state->self->head, TRUE);
        }
      else
        {
          gtk_source_completion_context_add_proposals (state->context,
                                                       GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                       NULL, TRUE);
        }
    }

  ide_clang_completion_state_free (state);

  IDE_EXIT;
}

static void
ide_clang_completion_provider_get_translation_unit_cb (GObject      *object,
                                                       GAsyncResult *result,
                                                       gpointer      user_data)
{
  g_autoptr(IdeClangTranslationUnit) unit = NULL;
  IdeClangService *service = (IdeClangService *)object;
  IdeClangCompletionState *state = user_data;
  GtkTextIter iter;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (state != NULL);
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (state->self));
  g_assert (G_IS_CANCELLABLE (state->cancellable));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));

  if (!(unit = ide_clang_service_get_translation_unit_finish (service, result, &error)))
    {
      g_debug ("%s", error->message);
      if (!g_cancellable_is_cancelled (state->cancellable))
        gtk_source_completion_context_add_proposals (state->context,
                                                     GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                     NULL, TRUE);
      ide_clang_completion_state_free (state);
      IDE_EXIT;
    }

  /*
   * It's like we are racing with our future self, which coallesced the
   * compile request into a single delayed query. Use the cancellable
   * to detect if we were cancelled and short-circuit early.
   */
  if (g_cancellable_is_cancelled (state->cancellable))
    {
      gtk_source_completion_context_add_proposals (state->context,
                                                   GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                   NULL, TRUE);
      ide_clang_completion_state_free (state);
      IDE_EXIT;
    }

  gtk_source_completion_context_get_iter (state->context, &iter);

  ide_clang_translation_unit_code_complete_async (unit,
                                                  ide_file_get_file (state->file),
                                                  &iter,
                                                  NULL,
                                                  ide_clang_completion_provider_code_complete_cb,
                                                  state);

  IDE_EXIT;
}

static void
ide_clang_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionState *state;
  g_autoptr(GtkSourceCompletion) completion = NULL;
  g_autofree gchar *line = NULL;
  g_autofree gchar *prefix = NULL;
  GtkTextIter stop;
  gunichar ch;
  GtkTextBuffer *buffer;
  IdeClangService *service;
  GtkTextIter iter;
  GtkTextIter begin;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (!gtk_source_completion_context_get_iter (context, &iter))
    {
      gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
      return;
    }

  buffer = gtk_text_iter_get_buffer (&iter);

  /* Get the line text up to the insertion mark */
  begin = iter;
  gtk_text_iter_set_line_offset (&begin, 0);
  line = gtk_text_iter_get_slice (&begin, &iter);

  /* Move to the just inserted character */
  stop = iter;
  if (!gtk_text_iter_starts_line (&stop))
    gtk_text_iter_backward_char (&stop);

  /*
   * Walk backwards to locate the first character after a stop character.
   * A stop character is anything that can't go in a function/type.
   */
  while (!gtk_text_iter_starts_line (&stop) &&
         (ch = gtk_text_iter_get_char (&stop)) &&
         (g_unichar_isalnum (ch) || (ch == '_')) &&
         gtk_text_iter_backward_char (&stop))
    {
      /* Do nothing */
    }

  ch = gtk_text_iter_get_char (&stop);

  /* Move forward if we moved past the last matching char */
  if (!g_unichar_isalnum (ch) &&
      (ch != '_') &&
      (gtk_text_iter_compare (&stop, &iter) < 0))
    gtk_text_iter_forward_char (&stop);

  prefix = g_strstrip (gtk_text_iter_get_slice (&stop, &iter));

  g_print ("RESULTS: %p (%d)\n", self->last_results, self->last_results? self->last_results->len : 0);
  g_print ("  last_line = %s\n", self->last_line ?: "<null>");
  g_print ("  last_query = %s\n", self->last_query ?: "<null>");

  /*
   * We might be able to reuse the results from our previous query if
   * the buffer is sufficiently similar. If so, possibly just rearrange
   * some things and redisplay those results.
   */
  if (ide_clang_completion_provider_can_replay (self, line))
    {
      IDE_PROBE;

      /*
       * Filter the items that no longer match our query.
       * We save a little state so that we can optimize further
       * passes of this operation by traversing the already filtered
       * linked list instead of all items.
       */
      ide_clang_completion_provider_refilter (self, self->last_results, prefix);
      gtk_source_completion_context_add_proposals (context, provider, self->head, TRUE);

      IDE_EXIT;
    }

  /* Save the view so we can push a snippet later */
  g_object_get (context, "completion", &completion, NULL);
  self->view = IDE_SOURCE_VIEW (gtk_source_completion_get_view (completion));

  ide_buffer_sync_to_unsaved_files (IDE_BUFFER (buffer));

  service = ide_context_get_service_typed (ide_object_get_context (IDE_OBJECT (self)),
                                           IDE_TYPE_CLANG_SERVICE);

  state = g_slice_new0 (IdeClangCompletionState);
  state->self = g_object_ref (self);
  state->context = g_object_ref (context);
  state->file = g_object_ref (ide_buffer_get_file (IDE_BUFFER (buffer)));
  state->cancellable = g_cancellable_new ();
  state->query = prefix, prefix = NULL;
  state->line = line, line = NULL;

  g_signal_connect_object (context,
                           "cancelled",
                           G_CALLBACK (g_cancellable_cancel),
                           state->cancellable,
                           G_CONNECT_SWAPPED);

  ide_clang_service_get_translation_unit_async (service,
                                                state->file,
                                                0,
                                                NULL,
                                                ide_clang_completion_provider_get_translation_unit_cb,
                                                state);

  IDE_EXIT;
}

static gboolean
get_start_iter (GtkSourceCompletionProvider *provider,
                const GtkTextIter           *location,
                GtkSourceCompletionProposal *proposal,
                GtkTextIter                 *iter)
{
  IdeClangCompletionItem *item = (IdeClangCompletionItem *)proposal;
  const gchar *typed_text = ide_clang_completion_item_get_typed_text (item);
  g_autofree gchar *text = g_strdup (typed_text);
  gint len = g_utf8_strlen (typed_text ?: "", -1);
  GtkTextIter begin;
  GtkTextIter end;
  guint offset;

  end = begin = *location;

  offset = gtk_text_iter_get_offset (&end);

  if (offset >= len)
    {
      gchar *textptr = g_utf8_offset_to_pointer (text, len);
      gchar *prevptr;
      GtkTextIter match_start;
      GtkTextIter match_end;

      gtk_text_iter_set_offset (&begin, offset - len);

      while (*text)
        {
          if (gtk_text_iter_forward_search (&begin, text, GTK_TEXT_SEARCH_TEXT_ONLY,
                                            &match_start, &match_end, &end))
            {
              *iter = match_start;
              return TRUE;
            }

          prevptr = textptr;
          textptr = g_utf8_find_prev_char (text, textptr);
          *prevptr = '\0';
        }
    }

  return FALSE;
}

static gboolean
ide_clang_completion_provider_get_start_iter (GtkSourceCompletionProvider *provider,
                                              GtkSourceCompletionContext  *context,
                                              GtkSourceCompletionProposal *proposal,
                                              GtkTextIter                 *iter)
{
  GtkTextIter location;

  gtk_source_completion_context_get_iter (context, &location);
  return get_start_iter (provider, &location, proposal, iter);
}

static gboolean
ide_clang_completion_provider_activate_proposal (GtkSourceCompletionProvider *provider,
                                                 GtkSourceCompletionProposal *proposal,
                                                 GtkTextIter                 *iter)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionItem *item = (IdeClangCompletionItem *)proposal;
  IdeSourceSnippet *snippet;
  GtkTextBuffer *buffer;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (item));

  if (!get_start_iter (provider, iter, proposal, &end))
    return FALSE;

  buffer = gtk_text_iter_get_buffer (iter);
  gtk_text_buffer_delete (buffer, iter, &end);

  snippet = ide_clang_completion_item_get_snippet (item);

  g_assert (snippet != NULL);
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));
  g_assert (IDE_IS_SOURCE_VIEW (self->view));

  ide_source_view_push_snippet (self->view, snippet);

  IDE_RETURN (TRUE);
}

static void
ide_clang_completion_provider_finalize (GObject *object)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)object;

  g_clear_pointer (&self->last_results, g_ptr_array_unref);
  g_clear_pointer (&self->last_line, g_free);
  g_clear_pointer (&self->last_query, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_clang_completion_provider_parent_class)->finalize (object);
}

static void
ide_clang_completion_provider_class_init (IdeClangCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_completion_provider_finalize;
}

static void
ide_clang_completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->activate_proposal = ide_clang_completion_provider_activate_proposal;
  iface->get_name = ide_clang_completion_provider_get_name;
  iface->get_priority = ide_clang_completion_provider_get_priority;
  iface->get_start_iter = ide_clang_completion_provider_get_start_iter;
  iface->match = ide_clang_completion_provider_match;
  iface->populate = ide_clang_completion_provider_populate;
}

static void
ide_clang_completion_provider_init (IdeClangCompletionProvider *self)
{
  IDE_ENTRY;
  self->settings = g_settings_new ("org.gnome.builder.code-insight");
  IDE_EXIT;
}
