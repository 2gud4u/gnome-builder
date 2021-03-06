/* gstyle-cielab.h
 *
 * Copyright © 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib.h>

#include "gstyle-types.h"

G_BEGIN_DECLS

#define GSTYLE_TYPE_CIELAB (gstyle_cielab_get_type ())

struct _GstyleCielab
{
  gdouble l;
  gdouble a;
  gdouble b;
  gdouble alpha;
};

GType          gstyle_cielab_get_type                (void) G_GNUC_CONST;

GstyleCielab  *gstyle_cielab_copy                    (const GstyleCielab  *self);
void           gstyle_cielab_free                    (GstyleCielab        *self);

G_END_DECLS
