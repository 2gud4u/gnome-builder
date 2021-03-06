/* ide-greeter-section.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>

#include "ide-version-macros.h"

#include "projects/ide-project-info.h"

G_BEGIN_DECLS

#define IDE_TYPE_GREETER_SECTION (ide_greeter_section_get_type ())

IDE_AVAILABLE_IN_3_28
G_DECLARE_INTERFACE (IdeGreeterSection, ide_greeter_section, IDE, GREETER_SECTION, GtkWidget)

struct _IdeGreeterSectionInterface
{
  GTypeInterface parent_iface;

  void     (*project_activated)  (IdeGreeterSection *self,
                                  IdeProjectInfo    *project_info);
  gint     (*get_priority)       (IdeGreeterSection *self);
  gboolean (*filter)             (IdeGreeterSection *self,
                                  DzlPatternSpec    *spec);
  gboolean (*activate_first)     (IdeGreeterSection *self);
  void     (*set_selection_mode) (IdeGreeterSection *self,
                                  gboolean           selection_mode);
  void     (*delete_selected)    (IdeGreeterSection *self);
  void     (*purge_selected)     (IdeGreeterSection *self);
};

IDE_AVAILABLE_IN_3_28
gint     ide_greeter_section_get_priority           (IdeGreeterSection *self);
IDE_AVAILABLE_IN_3_28
gboolean ide_greeter_section_filter                 (IdeGreeterSection *self,
                                                     DzlPatternSpec    *spec);
IDE_AVAILABLE_IN_3_28
void     ide_greeter_section_emit_project_activated (IdeGreeterSection *self,
                                                     IdeProjectInfo    *project_info);
IDE_AVAILABLE_IN_3_28
gboolean ide_greeter_section_activate_first         (IdeGreeterSection *self);
IDE_AVAILABLE_IN_3_28
void     ide_greeter_section_set_selection_mode     (IdeGreeterSection *self,
                                                     gboolean           selection_mode);
IDE_AVAILABLE_IN_3_28
void     ide_greeter_section_delete_selected        (IdeGreeterSection *self);
IDE_AVAILABLE_IN_3_28
void     ide_greeter_section_purge_selected         (IdeGreeterSection *self);

G_END_DECLS
