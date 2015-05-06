/* ide-modelines-file-settings.h
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

#ifndef IDE_MODELINES_FILE_SETTINGS_H
#define IDE_MODELINES_FILE_SETTINGS_H

#include "ide-file-settings.h"

G_BEGIN_DECLS

#define IDE_TYPE_MODELINES_FILE_SETTINGS (ide_modelines_file_settings_get_type())

G_DECLARE_FINAL_TYPE (IdeModelinesFileSettings, ide_modelines_file_settings,
                      IDE, MODELINES_FILE_SETTINGS, IdeFileSettings)

G_END_DECLS

#endif /* IDE_MODELINES_FILE_SETTINGS_H */
