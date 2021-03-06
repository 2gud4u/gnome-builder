/* gb-terminal-view.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <vte/vte.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_SEARCH (ide_terminal_search_get_type())

IDE_AVAILABLE_IN_3_28
G_DECLARE_FINAL_TYPE (IdeTerminalSearch, ide_terminal_search, IDE, TERMINAL_SEARCH, GtkBin)

IDE_AVAILABLE_IN_3_28
VteRegex    *ide_terminal_search_get_regex       (IdeTerminalSearch *self);
IDE_AVAILABLE_IN_3_28
gboolean     ide_terminal_search_get_wrap_around (IdeTerminalSearch *self);
IDE_AVAILABLE_IN_3_28
void         ide_terminal_search_set_terminal    (IdeTerminalSearch *self,
                                                  VteTerminal       *terminal);
IDE_AVAILABLE_IN_3_28
GtkRevealer *ide_terminal_search_get_revealer    (IdeTerminalSearch *self);

G_END_DECLS
