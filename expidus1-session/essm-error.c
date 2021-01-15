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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <expidus1-session/essm-error.h>

#define ESSM_DBUS_NAME "com.expidus.SessionManager"

static const GDBusErrorEntry essm_error_entries[] =
{
        { ESSM_ERROR_BAD_STATE,   ESSM_DBUS_NAME ".Error.Failed" },
        { ESSM_ERROR_BAD_VALUE,   ESSM_DBUS_NAME ".Error.General" },
        { ESSM_ERROR_UNSUPPORTED, ESSM_DBUS_NAME ".Error.Unsupported" },
};

GQuark
essm_error_get_quark (void)
{
  static volatile gsize quark_volatile = 0;

  g_dbus_error_register_error_domain ("essm_error",
                                      &quark_volatile,
                                      essm_error_entries,
                                      G_N_ELEMENTS (essm_error_entries));

  return (GQuark) quark_volatile;
}

GType
essm_error_get_type (void)
{
  static GType essm_error_type = 0;

  if (G_UNLIKELY (essm_error_type == 0))
    {
      static const GEnumValue values[] = {
        { ESSM_ERROR_BAD_STATE, "ESSM_ERROR_BAD_STATE", "BadState" },
        { ESSM_ERROR_BAD_VALUE, "ESSM_ERROR_BAD_VALUE", "BadValue" },
        { ESSM_ERROR_UNSUPPORTED, "ESSM_ERROR_UNSUPPORTED", "Unsupported" },
        { 0, NULL, NULL },
      };

      essm_error_type = g_enum_register_static ("EssmError", values);
    }

  return essm_error_type;
}

void
throw_error (GDBusMethodInvocation *context,
             gint                   error_code,
             const gchar           *format,
             ...)
{
        va_list args;
        gchar *message;

        va_start (args, format);
        message = g_strdup_vprintf (format, args);
        va_end (args);

        g_dbus_method_invocation_return_error (context, ESSM_ERROR, error_code, "%s", message);

        g_free (message);
}
