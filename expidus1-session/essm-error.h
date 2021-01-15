/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __ESSM_ERRORS_H__
#define __ESSM_ERRORS_H__

#include <glib-object.h>
#include <gio/gio.h>

#define ESSM_TYPE_ERROR  (essm_error_get_type ())
#define ESSM_ERROR       (essm_error_get_quark ())

#define ERROR_MSG(err)   ((err) != NULL ? (err)->message : "Error not set")

G_BEGIN_DECLS

typedef enum
{
  ESSM_ERROR_BAD_STATE = 0,
  ESSM_ERROR_BAD_VALUE,
  ESSM_ERROR_UNSUPPORTED,
} EssmError;

GType essm_error_get_type (void) G_GNUC_CONST;
GQuark essm_error_get_quark (void) G_GNUC_CONST;

void throw_error (GDBusMethodInvocation *context,
                  gint                   error_code,
                  const gchar           *format,
                  ...);

G_END_DECLS

#endif  /* !__ESSM_ERRORS_H__ */
