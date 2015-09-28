/* ide-completion-provider.c
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

#include "ide-completion-provider.h"
#include "ide-context.h"

G_DEFINE_INTERFACE (IdeCompletionProvider,
                    ide_completion_provider,
                    GTK_SOURCE_TYPE_COMPLETION_PROVIDER)

static void
ide_completion_provider_default_init (IdeCompletionProviderInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            IDE_TYPE_CONTEXT,
                                                            (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));
}

gboolean
ide_completion_provider_context_in_comment (GtkSourceCompletionContext *context)
{
  GtkTextIter iter;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), FALSE);

  if (gtk_source_completion_context_get_iter (context, &iter))
    {
      GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (&iter));

      if (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"))
        return TRUE;

      if (!gtk_text_iter_starts_line (&iter))
        {
          gtk_text_iter_backward_char (&iter);
          if (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"))
            return TRUE;
        }
    }

  return FALSE;
}

gchar *
ide_completion_provider_context_current_word (GtkSourceCompletionContext *context)
{
  GtkTextIter end;
  GtkTextIter begin;
  gunichar ch = 0;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), NULL);

  if (!gtk_source_completion_context_get_iter (context, &end))
    return NULL;

  begin = end;

  do
    {
      if (!gtk_text_iter_backward_char (&begin))
        break;
      ch = gtk_text_iter_get_char (&begin);
    }
  while (g_unichar_isalnum (ch) || (ch == '_'));

  if (ch && !g_unichar_isalnum (ch) && (ch != '_'))
    gtk_text_iter_forward_char (&begin);

  return gtk_text_iter_get_slice (&begin, &end);
}
