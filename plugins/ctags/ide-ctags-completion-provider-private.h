/* ide-ctags-completion-provider-private.h
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

#ifndef IDE_CTAGS_COMPLETION_PROVIDER_PRIVATE_H
#define IDE_CTAGS_COMPLETION_PROVIDER_PRIVATE_H

#include "ide-ctags-completion-provider.h"
#include "ide-completion-results.h"

G_BEGIN_DECLS

struct _IdeCtagsCompletionProvider
{
  IdeObject             parent_instance;
  gint                  minimum_word_size;
  GSettings            *settings;
  GPtrArray            *indexes;
  IdeCompletionResults *results;
  gchar                *current_word;
};

G_END_DECLS

#endif /* IDE_CTAGS_COMPLETION_PROVIDER_PRIVATE_H */
