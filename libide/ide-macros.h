/* ide-macros.h
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

#ifndef IDE_MACROS_H
#define IDE_MACROS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ide_clear_weak_pointer(ptr) \
  (*(ptr) ? (g_object_remove_weak_pointer((GObject*)*(ptr), (gpointer*)ptr),*(ptr)=NULL,1) : 0)

#define ide_set_weak_pointer(ptr,obj) \
  ((obj!=*(ptr))?(ide_clear_weak_pointer(ptr),*(ptr)=obj,((obj)?g_object_add_weak_pointer((GObject*)obj,(gpointer*)ptr),NULL:NULL),1):0)

#define ide_clear_signal_handler(obj,ptr) \
  G_STMT_START { \
    if (*(ptr) != 0) { \
      g_signal_handler_disconnect((obj), *(ptr)); \
      *(ptr) = 0; \
    } \
  } G_STMT_END

#define ide_str_empty0(str) (((str) == NULL)||(*(str) == 0))
#define ide_str_equal0(a,b) (g_strcmp0((a),(b)) == 0)

G_END_DECLS

#endif /* IDE_MACROS_H */
