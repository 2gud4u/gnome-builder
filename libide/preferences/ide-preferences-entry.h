/* ide-preferences-entry.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_PREFERENCES_ENTRY_H
#define IDE_PREFERENCES_ENTRY_H

#include <gtk/gtk.h>

#include "ide-preferences-container.h"

G_BEGIN_DECLS

#define IDE_TYPE_PREFERENCES_ENTRY (ide_preferences_entry_get_type())

G_DECLARE_DERIVABLE_TYPE (IdePreferencesEntry, ide_preferences_entry, IDE, PREFERENCES_ENTRY, IdePreferencesContainer)

struct _IdePreferencesEntryClass
{
  IdePreferencesContainerClass parent_class;
};

GtkWidget *ide_preferences_entry_get_title_widget (IdePreferencesEntry *self);

G_END_DECLS

#endif /* IDE_PREFERENCES_ENTRY_H */
