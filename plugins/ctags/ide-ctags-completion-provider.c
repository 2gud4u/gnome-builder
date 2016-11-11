/* ide-ctags-completion-provider.c
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

#define G_LOG_DOMAIN "ide-ctags-completion-provider"

#include <glib/gi18n.h>

#include "sourceview/ide-source-iter.h"

#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-completion-provider-private.h"
#include "ide-ctags-service.h"
#include "ide-ctags-util.h"

static void provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsCompletionProvider,
                                ide_ctags_completion_provider,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, provider_iface_init)
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

void
ide_ctags_completion_provider_add_index (IdeCtagsCompletionProvider *self,
                                         IdeCtagsIndex              *index)
{
  GFile *file;
  gsize i;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_return_if_fail (!index || IDE_IS_CTAGS_INDEX (index));
  g_return_if_fail (self->indexes != NULL);

  file = ide_ctags_index_get_file (index);

  for (i = 0; i < self->indexes->len; i++)
    {
      IdeCtagsIndex *item = g_ptr_array_index (self->indexes, i);
      GFile *item_file = ide_ctags_index_get_file (item);

      if (g_file_equal (item_file, file))
        {
          g_ptr_array_remove_index_fast (self->indexes, i);
          g_ptr_array_add (self->indexes, g_object_ref (index));

          IDE_EXIT;
        }
    }

  g_ptr_array_add (self->indexes, g_object_ref (index));

  IDE_EXIT;
}

static void
ide_ctags_completion_provider_constructed (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;
  IdeContext *context;
  IdeCtagsService *service;

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE);
  ide_ctags_service_register_completion (service, self);
}

static void
ide_ctags_completion_provider_dispose (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;
  IdeContext *context;
  IdeCtagsService *service;

  if ((context = ide_object_get_context (IDE_OBJECT (self))) &&
      (service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE)))
    ide_ctags_service_unregister_completion (service, self);

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->dispose (object);
}

static void
ide_ctags_completion_provider_finalize (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;

  g_clear_pointer (&self->current_word, g_free);
  g_clear_pointer (&self->indexes, g_ptr_array_unref);
  g_clear_object (&self->settings);
  g_clear_object (&self->results);

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->finalize (object);
}

static void
ide_ctags_completion_provider_class_init (IdeCtagsCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_ctags_completion_provider_constructed;
  object_class->dispose = ide_ctags_completion_provider_dispose;
  object_class->finalize = ide_ctags_completion_provider_finalize;
}

static void
ide_ctags_completion_provider_class_finalize (IdeCtagsCompletionProviderClass *klass)
{
}

static void
ide_ctags_completion_provider_init (IdeCtagsCompletionProvider *self)
{
  self->minimum_word_size = 3;
  self->indexes = g_ptr_array_new_with_free_func (g_object_unref);
  self->settings = g_settings_new ("org.gnome.builder.code-insight");
}

static gchar *
ide_ctags_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup ("Ctags");
}

static const gchar * const *
get_allowed_suffixes (GtkSourceCompletionContext *context)
{
  GtkTextIter iter;
  GtkSourceBuffer *buffer;
  GtkSourceLanguage *language;
  const gchar *lang_id = NULL;

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return NULL;

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (&iter));
  if ((language = gtk_source_buffer_get_language (buffer)))
    lang_id = gtk_source_language_get_id (language);

  return ide_ctags_get_allowed_suffixes (lang_id);
}

static void
ide_ctags_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  const gchar * const *allowed;
  g_autofree gchar *casefold = NULL;
  gint word_len;
  guint i;
  guint j;
  g_autoptr(GHashTable) completions = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  g_clear_pointer (&self->current_word, g_free);
  self->current_word = ide_completion_provider_context_current_word (context);

  allowed = get_allowed_suffixes (context);

  if (self->results != NULL)
    {
      if (ide_completion_results_replay (self->results, self->current_word))
        {
          ide_completion_results_present (self->results, provider, context);
          IDE_EXIT;
        }
      g_clear_pointer (&self->results, g_object_unref);
    }

  word_len = strlen (self->current_word);
  if (word_len < self->minimum_word_size)
    IDE_GOTO (word_too_small);

  casefold = g_utf8_casefold (self->current_word, -1);

  self->results = ide_completion_results_new (self->current_word);

  completions = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; i < self->indexes->len; i++)
    {
      g_autofree gchar *copy = g_strdup (self->current_word);
      IdeCtagsIndex *index = g_ptr_array_index (self->indexes, i);
      const IdeCtagsIndexEntry *entries = NULL;
      guint tmp_len = word_len;
      gsize n_entries = 0;
      gchar gdata_key[64];

      /*
       * Make sure we hold a reference to the index for the lifetime of the results.
       * When the results are released, so could our indexes.
       */
      g_snprintf (gdata_key, sizeof gdata_key, "ctags-%d", i);
      g_object_set_data_full (G_OBJECT (self->results), gdata_key,
                              g_object_ref (index), g_object_unref);

      while (entries == NULL && *copy)
        {
          if (!(entries = ide_ctags_index_lookup_prefix (index, copy, &n_entries)))
            copy [--tmp_len] = '\0';
        }

      if ((entries == NULL) || (n_entries == 0))
        continue;

      for (j = 0; j < n_entries; j++)
        {
          const IdeCtagsIndexEntry *entry = &entries [j];
          IdeCtagsCompletionItem *item;

          if (g_hash_table_contains (completions, entry->name))
            continue;

          g_hash_table_add (completions, (gchar *)entry->name);

          if (!ide_ctags_is_allowed (entry, allowed))
            continue;

          item = ide_ctags_completion_item_new (self, entry);

          if (!ide_completion_item_match (IDE_COMPLETION_ITEM (item), self->current_word, casefold))
            {
              g_object_unref (item);
              continue;
            }

          ide_completion_results_take_proposal (self->results, IDE_COMPLETION_ITEM (item));
        }
    }

  ide_completion_results_present (self->results, provider, context);

  IDE_EXIT;

word_too_small:
  gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);

  IDE_EXIT;
}

static gint
ide_ctags_completion_provider_get_priority (GtkSourceCompletionProvider *provider)
{
  return IDE_CTAGS_COMPLETION_PROVIDER_PRIORITY;
}

static gboolean
ide_ctags_completion_provider_match (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionContext  *context)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  GtkSourceCompletionActivation activation;
  GtkTextIter iter;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return FALSE;

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      gunichar ch;

      if (gtk_text_iter_starts_line (&iter))
        return FALSE;

      gtk_text_iter_backward_char (&iter);

      ch = gtk_text_iter_get_char (&iter);

      if (g_unichar_isalnum (ch))
        return FALSE;
    }

  if (ide_completion_provider_context_in_comment_or_string (context))
    return FALSE;

  if (!g_settings_get_boolean (self->settings, "ctags-autocompletion"))
    return FALSE;

  return TRUE;
}

static gboolean
ide_ctags_completion_provider_activate_proposal (GtkSourceCompletionProvider *provider,
                                                 GtkSourceCompletionProposal *proposal,
                                                 GtkTextIter                 *iter)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  IdeCtagsCompletionItem *item = (IdeCtagsCompletionItem *)proposal;
  GtkTextBuffer *buffer;
  GtkTextIter begin;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CTAGS_COMPLETION_ITEM (item));
  g_assert (iter != NULL);

  begin = *iter;

  buffer = gtk_text_iter_get_buffer (iter);

  /*
   * NOTE:
   *
   * Try to reduce our insertion to just the new text. This avoids a delete
   * which can be troublesome when we are working on a snippet. Ideally we
   * would improve snippets, but due to various limitations in GtkTextBuffer
   * and GtkTextView, we don't have an answer for that yet.
   */
  if (_ide_source_iter_backward_visible_word_start (&begin))
    {
      g_autofree gchar *current_text = gtk_text_iter_get_slice (&begin, iter);
      g_autofree gchar *proposal_text = gtk_source_completion_proposal_get_text (proposal);

      if (g_str_has_prefix (proposal_text, current_text))
        {
          gtk_text_buffer_begin_user_action (buffer);
          gtk_text_buffer_insert (buffer,
                                  iter,
                                  proposal_text + strlen (current_text),
                                  -1);
          gtk_text_buffer_end_user_action (buffer);
          return TRUE;
        }
    }

  /* Fallback and let the default handler take care of things */

  return FALSE;
}

static void
provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_name = ide_ctags_completion_provider_get_name;
  iface->get_priority = ide_ctags_completion_provider_get_priority;
  iface->match = ide_ctags_completion_provider_match;
  iface->populate = ide_ctags_completion_provider_populate;
  iface->activate_proposal = ide_ctags_completion_provider_activate_proposal;
}

void
_ide_ctags_completion_provider_register_type (GTypeModule *module)
{
  ide_ctags_completion_provider_register_type (module);
}
