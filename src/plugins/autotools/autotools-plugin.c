/* autotools-plugin.c
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

#include <libpeas/peas.h>
#include <ide.h>

#include "ide-autotools-application-addin.h"
#include "ide-autotools-build-system.h"
#include "ide-autotools-build-target-provider.h"
#include "ide-autotools-pipeline-addin.h"

void
ide_autotools_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module, IDE_TYPE_APPLICATION_ADDIN, IDE_TYPE_AUTOTOOLS_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module, IDE_TYPE_BUILD_PIPELINE_ADDIN, IDE_TYPE_AUTOTOOLS_PIPELINE_ADDIN);
  peas_object_module_register_extension_type (module, IDE_TYPE_BUILD_SYSTEM, IDE_TYPE_AUTOTOOLS_BUILD_SYSTEM);
  peas_object_module_register_extension_type (module, IDE_TYPE_BUILD_TARGET_PROVIDER, IDE_TYPE_AUTOTOOLS_BUILD_TARGET_PROVIDER);
}
